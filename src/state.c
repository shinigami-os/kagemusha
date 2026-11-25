#include "state.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int state_path(char *out, size_t cap)
{
	const char *runtime = getenv("XDG_RUNTIME_DIR");
	int n;

	if (!runtime || !*runtime)
		return -ENOENT;
	n = snprintf(out, cap, "%s/kagemusha/state", runtime);
	return (n < 0 || (size_t)n >= cap) ? -ENAMETOOLONG : 0;
}

int state_save(const char *path, const struct kage_state *s)
{
	char dir[256], tmp[300];
	char *slash;
	FILE *f;

	snprintf(dir, sizeof(dir), "%s", path);
	slash = strrchr(dir, '/');
	if (!slash)
		return -EINVAL;
	*slash = 0;
	if (mkdir(dir, 0700) < 0 && errno != EEXIST)
		return -errno;

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	f = fopen(tmp, "w");
	if (!f)
		return -errno;
	fprintf(f, "pid=%ld\nvirtual=%s\nreal=%s\nmode=%s\n", s->pid, s->virtual_output, s->real_output, s->mode);
	if (fclose(f) != 0 || rename(tmp, path) < 0) {
		int err = errno;

		unlink(tmp);
		return -err;
	}
	return 0;
}

int state_load(const char *path, struct kage_state *s)
{
	char line[128];
	int fields = 0;
	FILE *f = fopen(path, "r");

	if (!f)
		return -errno;
	memset(s, 0, sizeof(*s));
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = 0;
		if (strncmp(line, "pid=", 4) == 0) {
			char *end;

			s->pid = strtol(line + 4, &end, 10);
			if (*end == 0 && s->pid > 0)
				fields++;
		} else if (strncmp(line, "virtual=", 8) == 0 && line[8] && strlen(line + 8) < sizeof(s->virtual_output)) {
			strcpy(s->virtual_output, line + 8);
			fields++;
		} else if (strncmp(line, "real=", 5) == 0 && line[5] && strlen(line + 5) < sizeof(s->real_output)) {
			strcpy(s->real_output, line + 5);
			fields++;
		} else if (strncmp(line, "mode=", 5) == 0 && line[5] && strlen(line + 5) < sizeof(s->mode)) {
			strcpy(s->mode, line + 5);
			fields++;
		}
	}
	fclose(f);
	return fields == 4 ? 0 : -EINVAL;
}

void state_remove(const char *path)
{
	unlink(path);
}
