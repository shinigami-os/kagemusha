#include "mode.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define DEFAULT_HZ 60

static int parse_uint(const char **s, uint32_t *out)
{
	unsigned long v;
	char *end;

	if (!isdigit((unsigned char)**s))
		return -EINVAL;
	errno = 0;
	v = strtoul(*s, &end, 10);
	if (errno || v > UINT32_MAX)
		return -EINVAL;
	*out = (uint32_t)v;
	*s = end;
	return 0;
}

int kg_parse_mode(const char *text, uint32_t *width, uint32_t *height, uint32_t *refresh_hz)
{
	uint32_t w, h, hz = DEFAULT_HZ;
	const char *s = text;

	if (!s || !width || !height || !refresh_hz)
		return -EINVAL;
	if (parse_uint(&s, &w) < 0)
		return -EINVAL;
	if (*s != 'x' && *s != 'X')
		return -EINVAL;
	s++;
	if (parse_uint(&s, &h) < 0)
		return -EINVAL;
	if (*s == '@') {
		s++;
		if (parse_uint(&s, &hz) < 0)
			return -EINVAL;
	}
	if (*s)
		return -EINVAL;

	*width = w;
	*height = h;
	*refresh_hz = hz;
	return 0;
}
