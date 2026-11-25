#include "vmon.h"

#include <errno.h>
#include <evdi_lib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_RECTS 16
#define BYTES_PER_PIXEL 4

struct vmon {
	evdi_handle h;
	struct evdi_event_context ev;
	struct evdi_buffer buf;
	int has_buf;
	int connected;
	int failed;
	int verbose;
	struct vmon_stats stats;
};

static int log_verbose;

static void evdi_logger(void *user_data, const char *fmt, ...)
{
	va_list ap;

	(void)user_data;
	if (!log_verbose)
		return;
	fputs("libevdi: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static double now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static void free_buffer(struct vmon *v)
{
	if (!v->has_buf)
		return;
	evdi_unregister_buffer(v->h, v->buf.id);
	free(v->buf.buffer);
	free(v->buf.rects);
	memset(&v->buf, 0, sizeof(v->buf));
	v->has_buf = 0;
}

static void grab(struct vmon *v)
{
	int n = MAX_RECTS;
	double t0 = now_ms(), dt;
	int i;

	evdi_grab_pixels(v->h, v->buf.rects, &n);
	dt = now_ms() - t0;
	v->stats.frames++;
	v->stats.grab_ms += dt;
	if (dt > v->stats.grab_ms_max)
		v->stats.grab_ms_max = dt;
	for (i = 0; i < n && i < MAX_RECTS; i++)
		v->stats.bytes += (unsigned long)(v->buf.rects[i].x2 - v->buf.rects[i].x1) *
				  (v->buf.rects[i].y2 - v->buf.rects[i].y1) * BYTES_PER_PIXEL;
}

static void pump(struct vmon *v)
{
	while (evdi_request_update(v->h, v->buf.id))
		grab(v);
}

static void on_mode_changed(struct evdi_mode m, void *user_data)
{
	struct vmon *v = user_data;
	size_t stride, size;

	fprintf(stderr, "kagemushad: mode %dx%d@%d\n", m.width, m.height, m.refresh_rate);
	free_buffer(v);
	if (m.width <= 0 || m.height <= 0) {
		v->failed = 1;
		return;
	}

	stride = (size_t)m.width * BYTES_PER_PIXEL;
	if (stride > (size_t)-1 / (size_t)m.height) {
		v->failed = 1;
		return;
	}
	size = stride * (size_t)m.height;

	v->buf.id = 0;
	v->buf.width = m.width;
	v->buf.height = m.height;
	v->buf.stride = (int)stride;
	v->buf.buffer = calloc(1, size);
	v->buf.rect_count = MAX_RECTS;
	v->buf.rects = calloc(MAX_RECTS, sizeof(struct evdi_rect));
	if (!v->buf.buffer || !v->buf.rects) {
		fprintf(stderr, "kagemushad: cannot allocate %zu bytes for the frame buffer\n", size);
		free(v->buf.buffer);
		free(v->buf.rects);
		memset(&v->buf, 0, sizeof(v->buf));
		v->failed = 1;
		return;
	}
	v->has_buf = 1;
	evdi_register_buffer(v->h, v->buf);
	pump(v);
}

static void on_update_ready(int buffer_id, void *user_data)
{
	struct vmon *v = user_data;

	(void)buffer_id;
	if (!v->has_buf)
		return;
	grab(v);
	pump(v);
}

static void on_dpms(int mode, void *user_data)
{
	struct vmon *v = user_data;

	if (v->verbose)
		fprintf(stderr, "kagemushad: dpms %d\n", mode);
}

struct vmon *vmon_up(const struct kg_edid_params *params, int verbose, char *err, size_t err_len)
{
	uint8_t edid[KG_EDID_MAX_SIZE];
	struct evdi_logging logging = { .function = evdi_logger };
	struct vmon *v;
	int len;

	len = kg_edid_build(params, edid);
	if (len < 0) {
		snprintf(err, err_len, "cannot build the EDID: %s", strerror(-len));
		return NULL;
	}

	log_verbose = verbose;
	evdi_set_logging(logging);

	v = calloc(1, sizeof(*v));
	if (!v) {
		snprintf(err, err_len, "out of memory");
		return NULL;
	}
	v->verbose = verbose;
	v->h = evdi_open_attached_to(NULL);
	if (v->h == EVDI_INVALID_HANDLE) {
		snprintf(err, err_len,
			 "no evdi device: is the evdi module loaded, and can this user open /dev/dri and write /sys/devices/evdi/add?");
		free(v);
		return NULL;
	}

	v->ev.dpms_handler = on_dpms;
	v->ev.mode_changed_handler = on_mode_changed;
	v->ev.update_ready_handler = on_update_ready;
	v->ev.user_data = v;

	evdi_connect2(v->h, edid, (unsigned int)len, 0, 0);
	v->connected = 1;
	return v;
}

int vmon_fd(const struct vmon *v)
{
	return evdi_get_event_ready(v->h);
}

int vmon_failed(const struct vmon *v)
{
	return v->failed;
}

void vmon_dispatch(struct vmon *v)
{
	evdi_handle_events(v->h, &v->ev);
}

void vmon_take_stats(struct vmon *v, struct vmon_stats *out)
{
	*out = v->stats;
	memset(&v->stats, 0, sizeof(v->stats));
}

void vmon_down(struct vmon *v)
{
	if (!v)
		return;
	if (v->connected)
		evdi_disconnect(v->h);
	free_buffer(v);
	evdi_close(v->h);
	free(v);
}
