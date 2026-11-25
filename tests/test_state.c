#include "../src/state.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "check.h"

int main(void)
{
	char dir[128], file[160], sub[160];
	const char *tmp = getenv("TMPDIR");
	struct kage_state in = { .pid = 4242 }, out;
	FILE *f;

	snprintf(dir, sizeof(dir), "%s/kage-state-XXXXXX", tmp && *tmp ? tmp : "/tmp");
	if (!mkdtemp(dir)) {
		perror("mkdtemp");
		return 1;
	}
	snprintf(sub, sizeof(sub), "%s/kagemusha", dir);
	snprintf(file, sizeof(file), "%s/state", sub);
	strcpy(in.virtual_output, "DVI-I-1");
	strcpy(in.real_output, "eDP-1");
	strcpy(in.mode, "2560x1440@60");

	CHECK(state_load(file, &out) == -ENOENT);
	CHECK(state_save(file, &in) == 0);
	CHECK(state_load(file, &out) == 0);
	CHECK(out.pid == 4242);
	CHECK(strcmp(out.virtual_output, "DVI-I-1") == 0);
	CHECK(strcmp(out.real_output, "eDP-1") == 0);
	CHECK(strcmp(out.mode, "2560x1440@60") == 0);
	CHECK(state_save(file, &in) == 0);

	f = fopen(file, "w");
	fputs("pid=12\nvirtual=DVI-I-1\n", f);
	fclose(f);
	CHECK(state_load(file, &out) == -EINVAL);

	f = fopen(file, "w");
	fputs("pid=abc\nvirtual=DVI-I-1\nreal=eDP-1\nmode=1x1\n", f);
	fclose(f);
	CHECK(state_load(file, &out) == -EINVAL);

	f = fopen(file, "w");
	fputs("pid=-5\nvirtual=DVI-I-1\nreal=eDP-1\nmode=1x1\n", f);
	fclose(f);
	CHECK(state_load(file, &out) == -EINVAL);

	f = fopen(file, "w");
	fputs("pid=7\nvirtual=THIS_NAME_IS_FAR_TOO_LONG_FOR_THE_FIELD_ABC\nreal=eDP-1\nmode=1x1\n", f);
	fclose(f);
	CHECK(state_load(file, &out) == -EINVAL);

	state_remove(file);
	CHECK(state_load(file, &out) == -ENOENT);

	unlink(file);
	rmdir(sub);
	rmdir(dir);
	CHECK_DONE();
}
