#include "edid.h"

#include <errno.h>
#include <string.h>

#define BLOCK 128
#define DEFAULT_NAME "KAGEMUSHA"
#define MM_PER_1000_PX 156

static const uint8_t edid_header[8] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
static const uint8_t srgb_chromaticity[10] = { 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26, 0x0f, 0x50, 0x54 };

static uint32_t vsync_width(uint32_t w, uint32_t h)
{
	if (w * 9 == h * 16)
		return 5;
	if (w * 10 == h * 16)
		return 6;
	if (w * 3 == h * 4)
		return 4;
	if (w * 4 == h * 5)
		return 7;
	if (w * 9 == h * 15)
		return 7;
	return 10;
}

int kg_cvt_rb(uint32_t width, uint32_t height, uint32_t refresh_hz, struct kg_timing *out)
{
	uint32_t vsw, vbi, min_vbi, htotal, vtotal;
	uint64_t khz;

	if (!out || width < KG_DIM_MIN || width > KG_DIM_MAX || height < KG_DIM_MIN || height > KG_DIM_MAX ||
	    refresh_hz < 1 || refresh_hz > KG_HZ_MAX)
		return -EINVAL;

	vsw = vsync_width(width, height);
	vbi = (uint32_t)((460ull * height * refresh_hz) / (1000000ull - 460ull * refresh_hz)) + 1;
	min_vbi = 3 + vsw + 6;
	if (vbi < min_vbi)
		vbi = min_vbi;

	htotal = width + 160;
	vtotal = height + vbi;
	khz = (uint64_t)htotal * vtotal * refresh_hz / 1000;
	khz -= khz % 250;

	out->pixel_khz = (uint32_t)khz;
	out->hactive = width;
	out->hblank = 160;
	out->hsync_offset = 48;
	out->hsync_width = 32;
	out->vactive = height;
	out->vblank = vbi;
	out->vsync_offset = 3;
	out->vsync_width = vsw;
	return 0;
}

static int fits_base_dtd(const struct kg_timing *t)
{
	return t->pixel_khz / 10 <= 0xffff && t->hactive <= 0xfff && t->hblank <= 0xfff && t->hsync_offset <= 0x3ff &&
	       t->hsync_width <= 0x3ff && t->vactive <= 0xfff && t->vblank <= 0xfff && t->vsync_offset <= 0x3f &&
	       t->vsync_width <= 0x3f;
}

static void put_dtd(uint8_t *d, const struct kg_timing *t, uint32_t wmm, uint32_t hmm)
{
	uint32_t clk = t->pixel_khz / 10;

	d[0] = clk & 0xff;
	d[1] = clk >> 8;
	d[2] = t->hactive & 0xff;
	d[3] = t->hblank & 0xff;
	d[4] = ((t->hactive >> 8) << 4) | (t->hblank >> 8);
	d[5] = t->vactive & 0xff;
	d[6] = t->vblank & 0xff;
	d[7] = ((t->vactive >> 8) << 4) | (t->vblank >> 8);
	d[8] = t->hsync_offset & 0xff;
	d[9] = t->hsync_width & 0xff;
	d[10] = ((t->vsync_offset & 0xf) << 4) | (t->vsync_width & 0xf);
	d[11] = ((t->hsync_offset >> 8) << 6) | ((t->hsync_width >> 8) << 4) | ((t->vsync_offset >> 4) << 2) |
		(t->vsync_width >> 4);
	d[12] = wmm & 0xff;
	d[13] = hmm & 0xff;
	d[14] = ((wmm >> 8) << 4) | (hmm >> 8);
	d[17] = 0x18 | 0x02;
}

static uint8_t aspect_code(uint32_t w, uint32_t h)
{
	if (w == h)
		return 0;
	if (w * 4 == h * 5)
		return 1;
	if (w * 3 == h * 4)
		return 2;
	if (w * 9 == h * 15)
		return 3;
	if (w * 9 == h * 16)
		return 4;
	if (w * 10 == h * 16)
		return 5;
	if (w * 27 == h * 64)
		return 6;
	if (w * 135 == h * 256)
		return 7;
	return 8;
}

static void put_displayid_timing(uint8_t *d, const struct kg_timing *t)
{
	uint32_t clk = t->pixel_khz / 10 - 1;
	uint16_t hsync = (t->hsync_offset - 1) | 0x8000;
	uint16_t vsync = t->vsync_offset - 1;
	uint16_t v[8] = { t->hactive - 1, t->hblank - 1, hsync, t->hsync_width - 1,
			  t->vactive - 1, t->vblank - 1, vsync, t->vsync_width - 1 };
	int i;

	d[0] = clk & 0xff;
	d[1] = (clk >> 8) & 0xff;
	d[2] = (clk >> 16) & 0xff;
	d[3] = 0x80 | aspect_code(t->hactive, t->vactive);
	for (i = 0; i < 8; i++) {
		d[4 + i * 2] = v[i] & 0xff;
		d[5 + i * 2] = v[i] >> 8;
	}
}

static uint8_t checksum(const uint8_t *p, size_t len)
{
	uint8_t sum = 0;
	size_t i;

	for (i = 0; i < len; i++)
		sum += p[i];
	return (uint8_t)(0x100 - sum);
}

static void put_range_limits(uint8_t *d, uint32_t hz, uint32_t hfreq_khz, uint32_t pclk_khz)
{
	uint32_t vmin = hz < 24 ? hz : 24;
	uint32_t vmax = hz + 15;
	uint32_t hmin = hfreq_khz < 15 ? hfreq_khz : 15;
	uint32_t hmax = hfreq_khz + 10;
	uint32_t clk = (pclk_khz + 9999) / 10000 + 1;
	uint8_t flags = 0;

	if (vmax > 255) {
		flags |= 0x01;
		vmax -= 255;
	}
	if (vmin > 255) {
		flags |= 0x02;
		vmin -= 255;
	}
	if (hmax > 255) {
		flags |= 0x04;
		hmax -= 255;
	}
	if (hmin > 255) {
		flags |= 0x08;
		hmin -= 255;
	}
	if (hmax > 255)
		hmax = 255;
	if (clk > 255)
		clk = 255;

	d[3] = 0xfd;
	d[4] = flags;
	d[5] = vmin;
	d[6] = vmax;
	d[7] = hmin;
	d[8] = hmax;
	d[9] = clk;
	d[10] = 0x01;
	d[11] = 0x0a;
	memset(d + 12, 0x20, 6);
}

static int put_name(uint8_t *d, const char *name)
{
	size_t len = strlen(name), i;

	if (len == 0 || len > KG_NAME_MAX)
		return -EINVAL;
	for (i = 0; i < len; i++)
		if ((unsigned char)name[i] < 0x20 || (unsigned char)name[i] > 0x7e)
			return -EINVAL;

	d[3] = 0xfc;
	memset(d + 5, 0x20, KG_NAME_MAX);
	memcpy(d + 5, name, len);
	if (len < KG_NAME_MAX)
		d[5 + len] = 0x0a;
	return 0;
}

static void put_manufacturer(uint8_t *e)
{
	uint16_t id = ((('K' - 'A' + 1) << 10) | (('G' - 'A' + 1) << 5) | ('M' - 'A' + 1));

	e[8] = id >> 8;
	e[9] = id & 0xff;
	e[10] = 0x01;
	e[12] = 0x01;
}

int kg_edid_build(const struct kg_edid_params *p, uint8_t out[KG_EDID_MAX_SIZE])
{
	struct kg_timing t;
	uint32_t wmm, hmm, hfreq_khz;
	int base_dtd, ret, len;

	if (!p || !out)
		return -EINVAL;
	ret = kg_cvt_rb(p->width, p->height, p->refresh_hz, &t);
	if (ret < 0)
		return ret;

	wmm = p->width_mm ? p->width_mm : p->width * MM_PER_1000_PX / 1000;
	hmm = p->height_mm ? p->height_mm : p->height * MM_PER_1000_PX / 1000;
	if (wmm == 0)
		wmm = 1;
	if (hmm == 0)
		hmm = 1;
	if (wmm > 0xfff)
		wmm = 0xfff;
	if (hmm > 0xfff)
		hmm = 0xfff;

	memset(out, 0, KG_EDID_MAX_SIZE);
	memcpy(out, edid_header, 8);
	put_manufacturer(out);
	out[16] = 1;
	out[17] = 36;
	out[18] = 1;
	out[19] = 4;
	out[20] = 0xa5;
	if (wmm / 10 <= 255 && hmm / 10 <= 255) {
		out[21] = wmm / 10;
		out[22] = hmm / 10;
	}
	out[23] = 0x78;
	memcpy(out + 25, srgb_chromaticity, sizeof(srgb_chromaticity));
	memset(out + 38, 0x01, 16);

	base_dtd = fits_base_dtd(&t);
	out[24] = base_dtd ? 0x06 : 0x04;

	hfreq_khz = (t.pixel_khz + t.hactive + t.hblank - 1) / (t.hactive + t.hblank);
	if (base_dtd) {
		put_dtd(out + 54, &t, wmm, hmm);
	} else {
		out[54 + 3] = 0x10;
	}
	put_range_limits(out + 72, p->refresh_hz, hfreq_khz, t.pixel_khz);
	ret = put_name(out + 90, p->name ? p->name : DEFAULT_NAME);
	if (ret < 0)
		return ret;
	out[108 + 3] = 0x10;

	len = BLOCK;
	if (!base_dtd) {
		uint8_t *x = out + BLOCK;

		out[126] = 1;
		x[0] = 0x70;
		x[1] = 0x13;
		x[2] = 3 + 20;
		x[5] = 0x03;
		x[7] = 20;
		put_displayid_timing(x + 8, &t);
		x[28] = checksum(x + 1, 4 + x[2]);
		x[127] = checksum(x, BLOCK - 1);
		len = 2 * BLOCK;
	}
	out[127] = checksum(out, BLOCK - 1);
	return len;
}
