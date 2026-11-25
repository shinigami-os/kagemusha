#ifndef KAGEMUSHA_STATE_H
#define KAGEMUSHA_STATE_H

#include <stddef.h>

struct kage_state {
	long pid;
	char virtual_output[32];
	char real_output[32];
	char mode[32];
};

int state_path(char *out, size_t cap);
int state_save(const char *path, const struct kage_state *s);
int state_load(const char *path, struct kage_state *s);
void state_remove(const char *path);

#endif
