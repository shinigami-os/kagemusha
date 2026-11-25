#include "../src/sysfs.h"

#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "check.h"

static char root[160];

static void path(char *out, size_t cap, const char *rel)
{
	snprintf(out, cap, "%s/%s", root, rel);
}

static void mk(const char *rel)
{
	char p[256];

	path(p, sizeof(p), rel);
	mkdir(p, 0755);
}

static void link_device(const char *card, const char *target)
{
	char from[256], to[256];

	path(from, sizeof(from), card);
	mkdir(from, 0755);
	snprintf(from, sizeof(from), "%s/%s/device", root, card);
	snprintf(to, sizeof(to), "../devices/%s", target);
	symlink(to, from);
}

static int rm(const char *p, const struct stat *sb, int type, struct FTW *f)
{
	(void)sb;
	(void)type;
	(void)f;
	return remove(p);
}

int main(void)
{
	const char *tmp = getenv("TMPDIR");
	char out[64];

	snprintf(root, sizeof(root), "%s/kage-sysfs-XXXXXX", tmp && *tmp ? tmp : "/tmp");
	if (!mkdtemp(root)) {
		perror("mkdtemp");
		return 1;
	}
	mk("devices");
	mk("devices/pci0000");
	mk("devices/evdi.0");
	mk("devices/evdi.1");

	link_device("card0", "pci0000");
	mk("card0-eDP-1");
	mk("card0-DVI-I-1");
	CHECK(kg_find_virtual_connector(root, out, sizeof(out)) == -ENODEV);

	link_device("card1", "evdi.0");
	mk("card1-DVI-I-1");
	CHECK(kg_find_virtual_connector(root, out, sizeof(out)) == 0);
	CHECK(strcmp(out, "DVI-I-1") == 0);

	link_device("card2", "evdi.1");
	mk("card2-DVI-I-2");
	CHECK(kg_find_virtual_connector(root, out, sizeof(out)) == 0);
	CHECK(strcmp(out, "DVI-I-1") == 0);

	CHECK(kg_find_virtual_connector(root, out, 4) == -ENAMETOOLONG);
	CHECK(kg_find_virtual_connector("/nonexistent-kage-dir", out, sizeof(out)) < 0);

	nftw(root, rm, 8, FTW_DEPTH | FTW_PHYS);
	CHECK_DONE();
}
