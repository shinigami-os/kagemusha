#include "../src/edid.h"
#include "../src/mode.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	struct kg_edid_params p = { 0 };
	uint8_t e[KG_EDID_MAX_SIZE];
	uint32_t w, h, hz;
	int len;

	if (argc < 2 || kg_parse_mode(argv[1], &w, &h, &hz) < 0) {
		fprintf(stderr, "usage: %s WxH[@HZ] [NAME] > edid.bin\n", argv[0]);
		return 2;
	}
	p.width = w;
	p.height = h;
	p.refresh_hz = hz;
	p.name = argc > 2 ? argv[2] : NULL;

	len = kg_edid_build(&p, e);
	if (len < 0) {
		fprintf(stderr, "cannot build %ux%u@%u: %s\n", w, h, hz, strerror(-len));
		return 1;
	}
	fprintf(stderr, "%ux%u@%u: %d bytes\n", w, h, hz, len);
	if (fwrite(e, 1, len, stdout) != (size_t)len)
		return 1;
	return 0;
}
