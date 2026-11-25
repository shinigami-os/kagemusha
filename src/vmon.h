#ifndef KAGEMUSHA_VMON_H
#define KAGEMUSHA_VMON_H

#include <stddef.h>

#include "edid.h"

struct vmon;

struct vmon_stats {
	unsigned long frames;
	unsigned long bytes;
	double grab_ms;
	double grab_ms_max;
};

struct vmon *vmon_up(const struct kg_edid_params *params, int verbose, char *err, size_t err_len);
int vmon_fd(const struct vmon *v);
int vmon_failed(const struct vmon *v);
void vmon_dispatch(struct vmon *v);
void vmon_take_stats(struct vmon *v, struct vmon_stats *out);
void vmon_down(struct vmon *v);

#endif
