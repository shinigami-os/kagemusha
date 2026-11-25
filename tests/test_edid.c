#include "../src/edid.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond)                                                                       \
	do {                                                                              \
		checks++;                                                                 \
		if (!(cond)) {                                                            \
			failures++;                                                       \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
		}                                                                         \
	} while (0)

struct mode {
	uint32_t clock_khz;
	uint32_t hdisplay, hsync_start, hsync_end, htotal;
	uint32_t vdisplay, vsync_start, vsync_end, vtotal;
	int phsync, pvsync, preferred;
};

struct decoded {
	int blocks;
	int has_base_mode;
	int has_did_mode;
	struct mode base;
	struct mode did;
	char name[14];
	uint32_t vmin, vmax, hmin, hmax, max_clock_10mhz;
	uint32_t width_cm, height_cm;
};

static int block_ok(const uint8_t *b)
{
	uint8_t sum = 0;
	int i;

	for (i = 0; i < 128; i++)
		sum += b[i];
	return sum == 0;
}

static void decode_dtd(const uint8_t *d, struct mode *m)
{
	uint32_t hso, hsw, vso, vsw, hb, vb;

	m->clock_khz = (d[0] | (d[1] << 8)) * 10;
	m->hdisplay = d[2] | ((d[4] & 0xf0) << 4);
	hb = d[3] | ((d[4] & 0x0f) << 8);
	m->vdisplay = d[5] | ((d[7] & 0xf0) << 4);
	vb = d[6] | ((d[7] & 0x0f) << 8);
	hso = d[8] | ((d[11] & 0xc0) << 2);
	hsw = d[9] | ((d[11] & 0x30) << 4);
	vso = (d[10] >> 4) | ((d[11] & 0x0c) << 2);
	vsw = (d[10] & 0x0f) | ((d[11] & 0x03) << 4);
	m->htotal = m->hdisplay + hb;
	m->hsync_start = m->hdisplay + hso;
	m->hsync_end = m->hsync_start + hsw;
	m->vtotal = m->vdisplay + vb;
	m->vsync_start = m->vdisplay + vso;
	m->vsync_end = m->vsync_start + vsw;
	m->phsync = !!(d[17] & 0x02);
	m->pvsync = !!(d[17] & 0x04);
	m->preferred = 1;
}

static void decode_did_type1(const uint8_t *d, struct mode *m)
{
	uint32_t clk = (d[0] | (d[1] << 8) | (d[2] << 16)) + 1;
	uint32_t v[8];
	int i;

	for (i = 0; i < 8; i++)
		v[i] = d[4 + i * 2] | (d[5 + i * 2] << 8);

	m->clock_khz = clk * 10;
	m->hdisplay = v[0] + 1;
	m->htotal = m->hdisplay + v[1] + 1;
	m->hsync_start = m->hdisplay + (v[2] & 0x7fff) + 1;
	m->hsync_end = m->hsync_start + v[3] + 1;
	m->phsync = !!(v[2] & 0x8000);
	m->vdisplay = v[4] + 1;
	m->vtotal = m->vdisplay + v[5] + 1;
	m->vsync_start = m->vdisplay + (v[6] & 0x7fff) + 1;
	m->vsync_end = m->vsync_start + v[7] + 1;
	m->pvsync = !!(v[6] & 0x8000);
	m->preferred = !!(d[3] & 0x80);
}

static int decode(const uint8_t *e, int len, struct decoded *r)
{
	static const uint8_t hdr[8] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };
	int i;

	memset(r, 0, sizeof(*r));
	if (len < 128 || memcmp(e, hdr, 8) != 0 || e[18] != 1 || e[19] != 4)
		return -1;
	r->blocks = 1 + e[126];
	if (len != r->blocks * 128)
		return -2;
	for (i = 0; i < r->blocks; i++)
		if (!block_ok(e + i * 128))
			return -3;

	r->width_cm = e[21];
	r->height_cm = e[22];
	for (i = 0; i < 4; i++) {
		const uint8_t *d = e + 54 + i * 18;

		if (d[0] || d[1]) {
			decode_dtd(d, &r->base);
			r->has_base_mode = 1;
		} else if (d[3] == 0xfc) {
			int k;

			for (k = 0; k < 13 && d[5 + k] != 0x0a; k++)
				r->name[k] = d[5 + k];
			while (k > 0 && r->name[k - 1] == ' ')
				r->name[--k] = 0;
		} else if (d[3] == 0xfd) {
			r->vmin = d[5] + ((d[4] & 0x02) ? 255 : 0);
			r->vmax = d[6] + ((d[4] & 0x01) ? 255 : 0);
			r->hmin = d[7] + ((d[4] & 0x08) ? 255 : 0);
			r->hmax = d[8] + ((d[4] & 0x04) ? 255 : 0);
			r->max_clock_10mhz = d[9];
		}
	}
	if (r->blocks == 2) {
		const uint8_t *x = e + 128;
		uint32_t bytes;
		uint8_t sum = 0;
		int k;

		if (x[0] != 0x70)
			return -4;
		bytes = x[2];
		for (k = 1; k <= (int)(4 + bytes + 1); k++)
			sum += x[k];
		if (sum != 0)
			return -5;
		if (x[5] != 0x03 || x[7] % 20 != 0 || x[7] == 0)
			return -6;
		decode_did_type1(x + 8, &r->did);
		r->has_did_mode = 1;
	}
	return 0;
}

static void expect_mode(const struct mode *m, uint32_t w, uint32_t h, uint32_t hz)
{
	uint64_t total = (uint64_t)m->htotal * m->vtotal;
	uint64_t rate_mhz;
	uint64_t want = (uint64_t)hz * 1000;

	if (total == 0) {
		CHECK(total != 0);
		return;
	}
	rate_mhz = (uint64_t)m->clock_khz * 1000 * 1000 / total;

	CHECK(m->hdisplay == w);
	CHECK(m->vdisplay == h);
	CHECK(rate_mhz <= want && rate_mhz * 1000 >= want * 985);
	CHECK(m->clock_khz % 250 == 0);
	CHECK(m->htotal - m->hdisplay == 160);
	CHECK(m->hsync_start - m->hdisplay == 48);
	CHECK(m->hsync_end - m->hsync_start == 32);
	CHECK(m->vsync_start - m->vdisplay == 3);
	CHECK(m->phsync == 1);
	CHECK(m->pvsync == 0);
}

static void test_mode(uint32_t w, uint32_t h, uint32_t hz, int want_blocks)
{
	struct kg_edid_params p = { .width = w, .height = h, .refresh_hz = hz };
	uint8_t e[KG_EDID_MAX_SIZE];
	struct decoded r;
	int len = kg_edid_build(&p, e);

	CHECK(len == want_blocks * 128);
	if (len < 0)
		return;
	CHECK(decode(e, len, &r) == 0);
	if (r.blocks == 0)
		return;
	CHECK(r.blocks == want_blocks);
	CHECK(strcmp(r.name, "KAGEMUSHA") == 0);
	if (want_blocks == 1) {
		CHECK(r.has_base_mode && !r.has_did_mode);
		expect_mode(&r.base, w, h, hz);
	} else {
		CHECK(!r.has_base_mode && r.has_did_mode);
		expect_mode(&r.did, w, h, hz);
		CHECK(r.did.preferred);
	}
	CHECK(r.vmin <= hz && hz <= r.vmax);
	{
		const struct mode *m = want_blocks == 1 ? &r.base : &r.did;
		uint32_t hfreq = m->htotal ? (m->clock_khz + m->htotal - 1) / m->htotal : 0;

		CHECK(r.hmin <= hfreq && hfreq <= r.hmax);
		CHECK(r.max_clock_10mhz * 10000 >= m->clock_khz || r.max_clock_10mhz == 255);
	}
}

static void test_golden(void)
{
	struct kg_timing t;

	CHECK(kg_cvt_rb(1920, 1080, 60, &t) == 0);
	CHECK(t.pixel_khz == 138500 && t.hactive + t.hblank == 2080 && t.vactive + t.vblank == 1111 && t.vsync_width == 5);

	CHECK(kg_cvt_rb(2560, 1440, 60, &t) == 0);
	CHECK(t.pixel_khz == 241500 && t.hactive + t.hblank == 2720 && t.vactive + t.vblank == 1481 && t.vsync_width == 5);

	CHECK(kg_cvt_rb(3840, 2160, 60, &t) == 0);
	CHECK(t.pixel_khz == 533250 && t.hactive + t.hblank == 4000 && t.vactive + t.vblank == 2222 && t.vsync_width == 5);

	CHECK(kg_cvt_rb(1920, 1200, 60, &t) == 0);
	CHECK(t.vsync_width == 6);
	CHECK(kg_cvt_rb(1024, 768, 60, &t) == 0);
	CHECK(t.vsync_width == 4);
	CHECK(kg_cvt_rb(1280, 1024, 60, &t) == 0);
	CHECK(t.vsync_width == 7);
	CHECK(kg_cvt_rb(3440, 1440, 60, &t) == 0);
	CHECK(t.vsync_width == 10);
}

static void test_invalid(void)
{
	struct kg_edid_params p = { .width = 1920, .height = 1080, .refresh_hz = 60 };
	uint8_t e[KG_EDID_MAX_SIZE];
	struct kg_timing t;

	CHECK(kg_edid_build(NULL, e) == -EINVAL);
	CHECK(kg_edid_build(&p, NULL) == -EINVAL);
	CHECK(kg_cvt_rb(0, 0, 60, &t) == -EINVAL);
	CHECK(kg_cvt_rb(63, 1080, 60, &t) == -EINVAL);
	CHECK(kg_cvt_rb(1920, 63, 60, &t) == -EINVAL);
	CHECK(kg_cvt_rb(16385, 1080, 60, &t) == -EINVAL);
	CHECK(kg_cvt_rb(1920, 16385, 60, &t) == -EINVAL);
	CHECK(kg_cvt_rb(1920, 1080, 0, &t) == -EINVAL);
	CHECK(kg_cvt_rb(1920, 1080, KG_HZ_MAX + 1, &t) == -EINVAL);
	CHECK(kg_cvt_rb(1920, 1080, 60, NULL) == -EINVAL);

	p.name = "";
	CHECK(kg_edid_build(&p, e) == -EINVAL);
	p.name = "FOURTEEN_CHARS";
	CHECK(kg_edid_build(&p, e) == -EINVAL);
	p.name = "caf\xc3\xa9";
	CHECK(kg_edid_build(&p, e) == -EINVAL);
	p.name = "THIRTEEN_CHAR";
	CHECK(kg_edid_build(&p, e) == 128);
}

static void test_name_and_size(void)
{
	struct kg_edid_params p = { .width = 2560, .height = 1440, .refresh_hz = 60, .name = "SPOOF 1440p",
				    .width_mm = 600, .height_mm = 340 };
	uint8_t e[KG_EDID_MAX_SIZE];
	struct decoded r;
	int len = kg_edid_build(&p, e);

	CHECK(len == 128);
	CHECK(decode(e, len, &r) == 0);
	CHECK(strcmp(r.name, "SPOOF 1440p") == 0);
	CHECK(r.width_cm == 60 && r.height_cm == 34);

	p.name = "THIRTEEN_CHAR";
	len = kg_edid_build(&p, e);
	CHECK(len == 128);
	CHECK(decode(e, len, &r) == 0);
	CHECK(strcmp(r.name, "THIRTEEN_CHAR") == 0);

	p.width = 16384;
	p.height = 8192;
	p.width_mm = 3000;
	p.height_mm = 1700;
	p.refresh_hz = 30;
	len = kg_edid_build(&p, e);
	CHECK(len == 256);
	CHECK(e[21] == 0 && e[22] == 0);
}

int main(void)
{
	setvbuf(stdout, NULL, _IOLBF, 0);

	static const struct {
		uint32_t w, h, hz;
		int blocks;
	} modes[] = {
		{ 640, 480, 60, 1 },    { 800, 600, 60, 1 },    { 1024, 768, 60, 1 },  { 1280, 720, 60, 1 },
		{ 1280, 1024, 60, 1 },  { 1366, 768, 60, 1 },   { 1600, 900, 60, 1 },  { 1920, 1080, 60, 1 },
		{ 1920, 1080, 144, 1 }, { 1920, 1200, 60, 1 },  { 2560, 1080, 60, 1 }, { 2560, 1440, 60, 1 },
		{ 2560, 1440, 144, 1 }, { 3440, 1440, 60, 1 },  { 3840, 1080, 60, 1 }, { 3840, 2160, 30, 1 },
		{ 3840, 2160, 60, 1 },  { 4095, 2160, 30, 1 },  { 4096, 2160, 30, 2 }, { 3840, 2160, 120, 2 },
		{ 5120, 1440, 60, 2 },  { 5120, 2880, 60, 2 },  { 6016, 3384, 60, 2 }, { 7680, 4320, 60, 2 },
		{ 7680, 4320, 30, 2 },  { 8192, 4320, 60, 2 },  { 10240, 4320, 30, 2 }, { 16384, 8192, 30, 2 },
	};
	size_t i;

	test_golden();
	test_invalid();
	test_name_and_size();
	for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
		test_mode(modes[i].w, modes[i].h, modes[i].hz, modes[i].blocks);

	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
