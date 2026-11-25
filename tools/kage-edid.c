#include "../src/edid.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	struct kg_edid_params p = { 0 };
	uint8_t e[KG_EDID_MAX_SIZE];
	unsigned w, h, hz = 60;
	int len;

	if (argc < 2 || (sscanf(argv[1], "%ux%u@%u", &w, &h, &hz) < 2 && sscanf(argv[1], "%ux%u", &w, &h) != 2)) {
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
