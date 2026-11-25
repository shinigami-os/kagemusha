#include "sysfs.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int card_is_evdi(const char *drm_root, const char *card)
{
	char link[PATH_MAX], real[PATH_MAX];
	const char *base;

	snprintf(link, sizeof(link), "%s/%s/device", drm_root, card);
	if (!realpath(link, real))
		return 0;
	base = strrchr(real, '/');
	base = base ? base + 1 : real;
	return strncmp(base, "evdi", 4) == 0;
}

int kg_find_virtual_connector(const char *drm_root, char *out, size_t cap)
{
	DIR *d = opendir(drm_root);
	struct dirent *e;
	char best[64] = "";

	if (!d)
		return -errno;
	while ((e = readdir(d))) {
		char card[32];
		const char *dash = strchr(e->d_name, '-');
		size_t cardlen;

		if (strncmp(e->d_name, "card", 4) != 0 || !dash)
			continue;
		cardlen = (size_t)(dash - e->d_name);
		if (cardlen >= sizeof(card))
			continue;
		memcpy(card, e->d_name, cardlen);
		card[cardlen] = 0;
		if (strncmp(dash + 1, "DVI-I-", 6) != 0 || !card_is_evdi(drm_root, card))
			continue;
		if (!best[0] || strcmp(dash + 1, best) < 0)
			snprintf(best, sizeof(best), "%s", dash + 1);
	}
	closedir(d);
	if (!best[0])
		return -ENODEV;
	if (strlen(best) >= cap)
		return -ENAMETOOLONG;
	strcpy(out, best);
	return 0;
}
