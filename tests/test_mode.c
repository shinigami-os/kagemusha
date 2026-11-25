#include "../src/mode.h"

#include <stdio.h>

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

static int ok(const char *s, uint32_t w, uint32_t h, uint32_t hz)
{
	uint32_t rw = 0, rh = 0, rhz = 0;

	return kg_parse_mode(s, &rw, &rh, &rhz) == 0 && rw == w && rh == h && rhz == hz;
}

static int bad(const char *s)
{
	uint32_t w = 7, h = 7, hz = 7;

	return kg_parse_mode(s, &w, &h, &hz) < 0 && w == 7 && h == 7 && hz == 7;
}

int main(void)
{
	uint32_t a, b, c;

	CHECK(ok("1920x1080", 1920, 1080, 60));
	CHECK(ok("2560x1440@60", 2560, 1440, 60));
	CHECK(ok("3840X2160@30", 3840, 2160, 30));
	CHECK(ok("7680x4320@144", 7680, 4320, 144));
	CHECK(ok("0x0", 0, 0, 60));

	CHECK(bad(""));
	CHECK(bad("x"));
	CHECK(bad("1920"));
	CHECK(bad("1920x"));
	CHECK(bad("x1080"));
	CHECK(bad("1920x1080@"));
	CHECK(bad("1920x1080@60junk"));
	CHECK(bad("1920x1080@60@30"));
	CHECK(bad(" 1920x1080"));
	CHECK(bad("1920x1080 "));
	CHECK(bad("-1920x1080"));
	CHECK(bad("+1920x1080"));
	CHECK(bad("1920*1080"));
	CHECK(bad("99999999999x1080"));
	CHECK(bad("1920x99999999999"));
	CHECK(bad("1920x1080@99999999999"));

	CHECK(kg_parse_mode(NULL, &a, &b, &c) < 0);
	CHECK(kg_parse_mode("1920x1080", NULL, &b, &c) < 0);
	CHECK(kg_parse_mode("1920x1080", &a, NULL, &c) < 0);
	CHECK(kg_parse_mode("1920x1080", &a, &b, NULL) < 0);

	printf("%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
