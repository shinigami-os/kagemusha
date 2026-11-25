#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "edid.h"
#include "hypr.h"
#include "mode.h"
#include "state.h"
#include "sysfs.h"

#define DRM_ROOT "/sys/class/drm"
#define APPEAR_TIMEOUT_MS 5000
#define STOP_TIMEOUT_MS 3000

static void usage(void)
{
	fprintf(stderr,
		"usage: kage up WxH[@HZ] [--on OUTPUT]\n"
		"       kage down\n"
		"       kage status\n");
}

static void sleep_ms(int ms)
{
	struct timespec t = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };

	nanosleep(&t, NULL);
}

static int alive(long pid)
{
	return kill((pid_t)pid, 0) == 0 || errno == EPERM;
}

static int list_monitors(struct hypr_monitor *mons, int max)
{
	char text[8192];
	int n = hypr_request("monitors", text, sizeof(text));

	if (n < 0)
		return n;
	return hypr_parse_monitors(text, mons, max);
}

static const struct hypr_monitor *find_monitor(const struct hypr_monitor *mons, int n, const char *name)
{
	int i;

	for (i = 0; i < n; i++)
		if (strcmp(mons[i].name, name) == 0)
			return &mons[i];
	return NULL;
}

static void run_dir(const char *state_file, char *dir, size_t cap)
{
	char *slash;

	snprintf(dir, cap, "%s", state_file);
	slash = strrchr(dir, '/');
	if (slash)
		*slash = 0;
}

static pid_t spawn_daemon(const char *mode, const char *state_file)
{
	char dir[256], log[300], self[PATH_MAX], daemon[PATH_MAX];
	ssize_t n;
	pid_t pid;
	int logfd, nullfd;

	run_dir(state_file, dir, sizeof(dir));
	if (mkdir(dir, 0700) < 0 && errno != EEXIST)
		return -1;
	snprintf(log, sizeof(log), "%s/daemon.log", dir);

	n = readlink("/proc/self/exe", self, sizeof(self) - 1);
	daemon[0] = 0;
	if (n > 0) {
		char *slash;

		self[n] = 0;
		slash = strrchr(self, '/');
		if (slash) {
			*slash = 0;
			snprintf(daemon, sizeof(daemon), "%s/kagemushad", self);
		}
	}
	if (!daemon[0] || access(daemon, X_OK) != 0)
		snprintf(daemon, sizeof(daemon), "kagemushad");

	pid = fork();
	if (pid != 0)
		return pid;

	setsid();
	logfd = open(log, O_WRONLY | O_CREAT | O_APPEND, 0600);
	nullfd = open("/dev/null", O_RDWR);
	if (logfd >= 0 && nullfd >= 0) {
		dup2(nullfd, 0);
		dup2(nullfd, 1);
		dup2(logfd, 2);
	}
	execlp(daemon, "kagemushad", mode, (char *)NULL);
	_exit(127);
}

static int stop_daemon(long pid)
{
	int waited = 0;

	if (!alive(pid))
		return 0;
	kill((pid_t)pid, SIGTERM);
	while (alive(pid) && waited < STOP_TIMEOUT_MS) {
		sleep_ms(50);
		waited += 50;
	}
	if (alive(pid)) {
		kill((pid_t)pid, SIGKILL);
		return -1;
	}
	return 0;
}

static void fail_cleanup(pid_t pid)
{
	char err[128];

	stop_daemon(pid);
	hypr_reload(err, sizeof(err));
}

static int cmd_up(int argc, char **argv)
{
	struct hypr_monitor mons[HYPR_MAX_MONITORS];
	struct kage_state st = { 0 };
	struct kg_timing timing;
	const struct hypr_monitor *real, *virt;
	struct hypr_rule rule = { 0 };
	char state_file[256], err[256], mode[32], real_mode[32], virt_name[HYPR_NAME_MAX], real_name[HYPR_NAME_MAX];
	const char *on = NULL;
	uint32_t w, h, hz;
	int i, n, waited = 0;
	pid_t pid;

	if (argc < 3 || kg_parse_mode(argv[2], &w, &h, &hz) < 0) {
		usage();
		return 2;
	}
	for (i = 3; i < argc; i++) {
		if (strcmp(argv[i], "--on") == 0 && i + 1 < argc) {
			on = argv[++i];
		} else {
			usage();
			return 2;
		}
	}
	if (kg_cvt_rb(w, h, hz, &timing) < 0) {
		fprintf(stderr, "kage: %ux%u@%u is outside the supported range\n", w, h, hz);
		return 1;
	}
	snprintf(mode, sizeof(mode), "%ux%u@%u", w, h, hz);

	if (state_path(state_file, sizeof(state_file)) < 0) {
		fprintf(stderr, "kage: XDG_RUNTIME_DIR is not set\n");
		return 1;
	}
	if (state_load(state_file, &st) == 0) {
		if (alive(st.pid)) {
			fprintf(stderr, "kage: already up (%s), run kage down first\n", st.mode);
			return 1;
		}
		state_remove(state_file);
	}

	n = list_monitors(mons, HYPR_MAX_MONITORS);
	if (n <= 0) {
		fprintf(stderr, "kage: cannot read the monitor list from Hyprland\n");
		return 1;
	}
	real = on ? find_monitor(mons, n, on) : &mons[0];
	if (!real) {
		fprintf(stderr, "kage: no output named %s\n", on);
		return 1;
	}
	snprintf(real_name, sizeof(real_name), "%s", real->name);
	if (kg_find_virtual_connector(DRM_ROOT, virt_name, sizeof(virt_name)) < 0) {
		fprintf(stderr, "kage: no evdi device found (is the evdi module loaded and a device created?)\n");
		return 1;
	}

	rule.output = virt_name;
	rule.mode = mode;
	rule.has_position = 1;
	rule.x = real->x + real->width;
	rule.y = real->y;
	rule.has_scale = 1;
	rule.scale = 1.0;
	if (hypr_apply_rule(&rule, err, sizeof(err)) < 0) {
		fprintf(stderr, "kage: %s\n", err);
		return 1;
	}

	pid = spawn_daemon(mode, state_file);
	if (pid < 0) {
		fprintf(stderr, "kage: cannot start kagemushad\n");
		hypr_reload(err, sizeof(err));
		return 1;
	}

	virt = NULL;
	while (waited < APPEAR_TIMEOUT_MS) {
		int status;

		if (waitpid(pid, &status, WNOHANG) == pid) {
			fprintf(stderr, "kage: kagemushad stopped at once, see the daemon.log next to %s\n", state_file);
			hypr_reload(err, sizeof(err));
			return 1;
		}
		n = list_monitors(mons, HYPR_MAX_MONITORS);
		virt = n > 0 ? find_monitor(mons, n, virt_name) : NULL;
		if (virt)
			break;
		sleep_ms(100);
		waited += 100;
	}
	if (!virt) {
		fprintf(stderr, "kage: Hyprland did not show %s within %d s\n", virt_name, APPEAR_TIMEOUT_MS / 1000);
		fail_cleanup(pid);
		return 1;
	}
	if ((uint32_t)virt->width != w || (uint32_t)virt->height != h)
		fprintf(stderr, "kage: warning: the output came up as %dx%d\n", virt->width, virt->height);

	real = find_monitor(mons, n, real_name);
	if (!real) {
		fprintf(stderr, "kage: the real output disappeared\n");
		fail_cleanup(pid);
		return 1;
	}
	snprintf(real_mode, sizeof(real_mode), "%dx%d@%.2f", real->width, real->height, real->refresh);
	memset(&rule, 0, sizeof(rule));
	rule.output = real->name;
	rule.mode = real_mode;
	rule.has_position = 1;
	rule.x = real->x;
	rule.y = real->y;
	rule.has_scale = 1;
	rule.scale = real->scale > 0 ? real->scale : 1.0;
	rule.mirror = virt_name;
	if (hypr_apply_rule(&rule, err, sizeof(err)) < 0) {
		fprintf(stderr, "kage: %s\n", err);
		fail_cleanup(pid);
		return 1;
	}

	st.pid = pid;
	snprintf(st.virtual_output, sizeof(st.virtual_output), "%s", virt_name);
	snprintf(st.real_output, sizeof(st.real_output), "%s", real->name);
	snprintf(st.mode, sizeof(st.mode), "%s", mode);
	if (state_save(state_file, &st) < 0) {
		fprintf(stderr, "kage: cannot write %s\n", state_file);
		fail_cleanup(pid);
		return 1;
	}
	printf("up: %s on %s, mirrored on %s\n", mode, virt_name, st.real_output);
	return 0;
}

static int cmd_down(void)
{
	struct kage_state st;
	char state_file[256], err[256];

	if (state_path(state_file, sizeof(state_file)) < 0 || state_load(state_file, &st) < 0) {
		printf("kagemusha is not up\n");
		return 0;
	}
	if (hypr_reload(err, sizeof(err)) < 0)
		fprintf(stderr, "kage: %s\n", err);
	if (stop_daemon(st.pid) < 0)
		fprintf(stderr, "kage: kagemushad did not stop in time and was killed\n");
	state_remove(state_file);
	printf("down\n");
	return 0;
}

static int cmd_status(void)
{
	struct hypr_monitor mons[HYPR_MAX_MONITORS];
	struct kage_state st;
	char state_file[256];
	int n;

	if (state_path(state_file, sizeof(state_file)) < 0 || state_load(state_file, &st) < 0) {
		printf("down\n");
		return 0;
	}
	if (!alive(st.pid)) {
		printf("down (stale state, the daemon is gone: run kage down)\n");
		return 1;
	}
	n = list_monitors(mons, HYPR_MAX_MONITORS);
	printf("up: %s on %s, mirrored on %s, daemon pid %ld, output %s\n", st.mode, st.virtual_output, st.real_output,
	       st.pid, n > 0 && find_monitor(mons, n, st.virtual_output) ? "present" : "missing");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
		return 2;
	}
	if (strcmp(argv[1], "up") == 0)
		return cmd_up(argc, argv);
	if (strcmp(argv[1], "down") == 0 && argc == 2)
		return cmd_down();
	if (strcmp(argv[1], "status") == 0 && argc == 2)
		return cmd_status();
	usage();
	return 2;
}
