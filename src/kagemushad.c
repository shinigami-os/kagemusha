#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <time.h>
#include <unistd.h>

#include "edid.h"
#include "mode.h"
#include "vmon.h"

static void usage(const char *prog)
{
	fprintf(stderr, "usage: %s WxH[@HZ] [-n NAME] [-t SECONDS] [-v]\n", prog);
}

static void print_stats(struct vmon *v)
{
	struct vmon_stats s;

	vmon_take_stats(v, &s);
	fprintf(stderr, "kagemushad: %lu fps, grab avg %.2f ms max %.2f ms, %.1f MB/s\n", s.frames,
		s.frames ? s.grab_ms / s.frames : 0.0, s.grab_ms_max, s.bytes / 1e6);
}

int main(int argc, char **argv)
{
	struct kg_edid_params params = { 0 };
	char err[256];
	struct vmon *v;
	sigset_t mask;
	int sfd, i, verbose = 0, status = 0;
	long seconds = 0;
	time_t deadline = 0, last_stats = 0;
	uint32_t w, h, hz;

	if (argc < 2 || kg_parse_mode(argv[1], &w, &h, &hz) < 0) {
		usage(argv[0]);
		return 2;
	}
	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
			params.name = argv[++i];
		} else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
			seconds = strtol(argv[++i], NULL, 10);
		} else if (strcmp(argv[i], "-v") == 0) {
			verbose = 1;
		} else {
			usage(argv[0]);
			return 2;
		}
	}
	params.width = w;
	params.height = h;
	params.refresh_hz = hz;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	sfd = signalfd(-1, &mask, SFD_CLOEXEC);
	if (sfd < 0) {
		perror("signalfd");
		return 1;
	}

	v = vmon_up(&params, verbose, err, sizeof(err));
	if (!v) {
		fprintf(stderr, "kagemushad: %s\n", err);
		return 1;
	}
	fprintf(stderr, "kagemushad: up %ux%u@%u\n", w, h, hz);

	if (seconds > 0)
		deadline = time(NULL) + seconds;
	last_stats = time(NULL);

	for (;;) {
		struct pollfd pf[2] = {
			{ .fd = sfd, .events = POLLIN },
			{ .fd = vmon_fd(v), .events = POLLIN },
		};
		time_t now;

		if (poll(pf, 2, 250) < 0 && errno != EINTR)
			break;
		if (pf[0].revents & POLLIN) {
			struct signalfd_siginfo si;

			if (read(sfd, &si, sizeof(si)) == sizeof(si))
				break;
		}
		if (pf[1].revents & POLLIN)
			vmon_dispatch(v);
		if (vmon_failed(v)) {
			status = 1;
			break;
		}
		now = time(NULL);
		if (verbose && now != last_stats) {
			last_stats = now;
			print_stats(v);
		}
		if (deadline && now >= deadline)
			break;
	}

	vmon_down(v);
	fprintf(stderr, "kagemushad: down\n");
	return status;
}
