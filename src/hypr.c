#include "hypr.h"

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#define IO_TIMEOUT_S 3

static int newest_instance(const char *dir, char *out, size_t cap)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	time_t best = 0;
	int found = 0;

	if (!d)
		return -errno;
	while ((e = readdir(d))) {
		char path[512];
		struct stat st;

		if (e->d_name[0] == '.')
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if (stat(path, &st) < 0 || !S_ISDIR(st.st_mode))
			continue;
		if (!found || st.st_mtime > best) {
			best = st.st_mtime;
			snprintf(out, cap, "%s", e->d_name);
			found = 1;
		}
	}
	closedir(d);
	return found ? 0 : -ENOENT;
}

static int socket_path(char *out, size_t cap)
{
	const char *runtime = getenv("XDG_RUNTIME_DIR");
	const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
	char dir[256], found[128];
	int n;

	if (!runtime)
		return -ENOENT;
	if (!sig || !*sig) {
		snprintf(dir, sizeof(dir), "%s/hypr", runtime);
		if (newest_instance(dir, found, sizeof(found)) < 0)
			return -ENOENT;
		sig = found;
	}
	n = snprintf(out, cap, "%s/hypr/%s/.socket.sock", runtime, sig);
	return (n < 0 || (size_t)n >= cap) ? -ENAMETOOLONG : 0;
}

int hypr_request(const char *command, char *reply, size_t cap)
{
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	struct timeval tv = { .tv_sec = IO_TIMEOUT_S };
	char path[256];
	size_t len = strlen(command), total = 0;
	ssize_t n = 0;
	int fd, ret;

	if (cap == 0)
		return -EINVAL;
	ret = socket_path(path, sizeof(path));
	if (ret < 0)
		return ret;
	if (strlen(path) >= sizeof(addr.sun_path))
		return -ENAMETOOLONG;
	strcpy(addr.sun_path, path);

	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -errno;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		ret = -errno;
		close(fd);
		return ret;
	}
	if (write(fd, command, len) != (ssize_t)len) {
		ret = errno ? -errno : -EIO;
		close(fd);
		return ret;
	}
	while (total < cap - 1 && (n = read(fd, reply + total, cap - 1 - total)) > 0)
		total += (size_t)n;
	ret = n < 0 ? -errno : 0;
	close(fd);
	reply[total] = 0;
	return ret < 0 ? ret : (int)total;
}

int hypr_parse_monitors(const char *text, struct hypr_monitor *out, int max)
{
	const char *line = text;
	int count = 0;

	while (*line) {
		const char *end = strchr(line, '\n');
		size_t len = end ? (size_t)(end - line) : strlen(line);
		char buf[256];
		struct hypr_monitor *m = count > 0 ? &out[count - 1] : NULL;

		snprintf(buf, sizeof(buf), "%.*s", (int)len, line);
		if (strncmp(buf, "Monitor ", 8) == 0) {
			char name[HYPR_NAME_MAX];

			if (count < max && sscanf(buf, "Monitor %31s", name) == 1) {
				memset(&out[count], 0, sizeof(out[count]));
				snprintf(out[count].name, sizeof(out[count].name), "%s", name);
				count++;
			}
		} else if (m && buf[0] == '\t') {
			const char *p = buf + 1;
			int w, h, x, y;
			double hz, sc;

			if (sscanf(p, "%dx%d@%lf at %dx%d", &w, &h, &hz, &x, &y) == 5) {
				m->width = w;
				m->height = h;
				m->refresh = hz;
				m->x = x;
				m->y = y;
			} else if (strncmp(p, "description: ", 13) == 0) {
				snprintf(m->description, sizeof(m->description), "%s", p + 13);
			} else if (sscanf(p, "scale: %lf", &sc) == 1) {
				m->scale = sc;
			}
		}
		if (!end)
			break;
		line = end + 1;
	}
	return count;
}

static int valid_name(const char *s)
{
	if (!s || !*s || strlen(s) >= HYPR_NAME_MAX)
		return 0;
	for (; *s; s++)
		if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '-' ||
		      *s == '_' || *s == '.'))
			return 0;
	return 1;
}

static int valid_mode(const char *s)
{
	if (!s || !*s || strlen(s) >= 32)
		return 0;
	for (; *s; s++)
		if (!((*s >= '0' && *s <= '9') || *s == 'x' || *s == '@' || *s == '.'))
			return 0;
	return 1;
}

static int append(char *out, size_t cap, size_t *len, const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(out + *len, cap - *len, fmt, ap);
	va_end(ap);
	if (n < 0 || (size_t)n >= cap - *len)
		return -ENOSPC;
	*len += (size_t)n;
	return 0;
}

int hypr_build_rule(const struct hypr_rule *r, char *out, size_t cap)
{
	size_t len = 0;
	int ret;

	if (!r || !out || cap == 0 || !valid_name(r->output))
		return -EINVAL;
	if ((r->mode && !valid_mode(r->mode)) || (r->mirror && !valid_name(r->mirror)))
		return -EINVAL;

	ret = append(out, cap, &len, "hl.monitor({ output = \"%s\"", r->output);
	if (ret == 0 && r->disabled)
		ret = append(out, cap, &len, ", disabled = true");
	if (ret == 0 && !r->disabled && r->mode)
		ret = append(out, cap, &len, ", mode = \"%s\"", r->mode);
	if (ret == 0 && !r->disabled && r->has_position)
		ret = append(out, cap, &len, ", position = \"%dx%d\"", r->x, r->y);
	if (ret == 0 && !r->disabled && r->has_scale)
		ret = append(out, cap, &len, ", scale = %.2f", r->scale);
	if (ret == 0 && !r->disabled && r->mirror)
		ret = append(out, cap, &len, ", mirror = \"%s\"", r->mirror);
	if (ret == 0)
		ret = append(out, cap, &len, " })");
	return ret < 0 ? ret : (int)len;
}

static int expect_ok(const char *command, char *err, size_t err_cap)
{
	char reply[512];
	int n = hypr_request(command, reply, sizeof(reply));

	if (n < 0) {
		snprintf(err, err_cap, "cannot talk to Hyprland: %s", strerror(-n));
		return n;
	}
	if (strcmp(reply, "ok") != 0) {
		snprintf(err, err_cap, "Hyprland said: %s", reply);
		return -EIO;
	}
	return 0;
}

int hypr_apply_rule(const struct hypr_rule *rule, char *err, size_t err_cap)
{
	char lua[384], command[420];
	int n = hypr_build_rule(rule, lua, sizeof(lua));

	if (n < 0) {
		snprintf(err, err_cap, "invalid monitor rule");
		return n;
	}
	snprintf(command, sizeof(command), "/eval %s", lua);
	return expect_ok(command, err, err_cap);
}

int hypr_reload(char *err, size_t err_cap)
{
	return expect_ok("reload", err, err_cap);
}
