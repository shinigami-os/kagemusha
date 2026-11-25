#include "../src/hypr.h"

#include <errno.h>
#include <string.h>

#include "check.h"

static const char *two_monitors =
	"Monitor DVI-I-1 (ID 1):\n"
	"\t2560x1440@59.95100 at 1920x0\n"
	"\tdescription: KGM KAGEMUSHA (DVI-I-1)\n"
	"\tmake: KGM\n"
	"\tscale: 1.00\n"
	"\ttransform: 0\n"
	"\n"
	"Monitor eDP-1 (ID 0):\n"
	"\t1920x1080@60.00800 at 0x0\n"
	"\tdescription: Chimei Innolux Corporation 0x15E7\n"
	"\tphysical size (mm): 340x190\n"
	"\tscale: 1.50\n"
	"\tmirrorOf: none\n";

static void test_parse(void)
{
	struct hypr_monitor m[4];
	int n = hypr_parse_monitors(two_monitors, m, 4);

	CHECK(n == 2);
	CHECK(strcmp(m[0].name, "DVI-I-1") == 0);
	CHECK(m[0].width == 2560 && m[0].height == 1440 && m[0].x == 1920 && m[0].y == 0);
	CHECK(m[0].refresh > 59.9 && m[0].refresh < 60.0);
	CHECK(m[0].scale == 1.0);
	CHECK(strstr(m[0].description, "KAGEMUSHA") != NULL);
	CHECK(strcmp(m[1].name, "eDP-1") == 0);
	CHECK(m[1].width == 1920 && m[1].height == 1080 && m[1].x == 0);
	CHECK(m[1].scale == 1.5);
	CHECK(strstr(m[1].description, "Chimei") != NULL);

	CHECK(hypr_parse_monitors(two_monitors, m, 1) == 1);
	CHECK(hypr_parse_monitors("", m, 4) == 0);
	CHECK(hypr_parse_monitors("no monitors here\n\tscale: 2.00\n", m, 4) == 0);
	CHECK(hypr_parse_monitors("Monitor eDP-1 (ID 0):", m, 4) == 1);
}

static void test_rules(void)
{
	char out[512];
	struct hypr_rule r = { .output = "eDP-1" };

	r.mode = "1920x1080@60.01";
	r.has_position = 1;
	r.x = 0;
	r.y = 0;
	r.has_scale = 1;
	r.scale = 1.0;
	r.mirror = "DVI-I-1";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) > 0);
	CHECK(strcmp(out, "hl.monitor({ output = \"eDP-1\", mode = \"1920x1080@60.01\", position = \"0x0\", scale = 1.00, mirror = \"DVI-I-1\" })") == 0);

	memset(&r, 0, sizeof(r));
	r.output = "DVI-I-1";
	r.disabled = 1;
	r.mode = "1x1";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) > 0);
	CHECK(strcmp(out, "hl.monitor({ output = \"DVI-I-1\", disabled = true })") == 0);

	memset(&r, 0, sizeof(r));
	r.output = "DP-2";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) > 0);
	CHECK(strcmp(out, "hl.monitor({ output = \"DP-2\" })") == 0);

	r.output = "bad\"name";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) == -EINVAL);
	r.output = "a b";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) == -EINVAL);
	r.output = "x\")os.execute(\"id\")--";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) == -EINVAL);
	r.output = "";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) == -EINVAL);
	r.output = NULL;
	CHECK(hypr_build_rule(&r, out, sizeof(out)) == -EINVAL);
	r.output = "DP-2";
	r.mode = "1920x1080\"";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) == -EINVAL);
	r.mode = NULL;
	r.mirror = "x y";
	CHECK(hypr_build_rule(&r, out, sizeof(out)) == -EINVAL);
	CHECK(hypr_build_rule(NULL, out, sizeof(out)) == -EINVAL);

	memset(&r, 0, sizeof(r));
	r.output = "DP-2";
	r.mode = "1920x1080@60";
	CHECK(hypr_build_rule(&r, out, 10) == -ENOSPC);
	CHECK(hypr_build_rule(&r, out, 0) == -EINVAL);
	CHECK(hypr_build_rule(&r, out, 32) == -ENOSPC);
}

int main(void)
{
	test_parse();
	test_rules();
	CHECK_DONE();
}
