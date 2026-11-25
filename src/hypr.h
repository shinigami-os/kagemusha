#ifndef KAGEMUSHA_HYPR_H
#define KAGEMUSHA_HYPR_H

#include <stddef.h>

#define HYPR_NAME_MAX 32
#define HYPR_MAX_MONITORS 16

struct hypr_monitor {
	char name[HYPR_NAME_MAX];
	char description[128];
	int width, height, x, y;
	double refresh, scale;
};

struct hypr_rule {
	const char *output;
	const char *mode;
	const char *mirror;
	int has_position;
	int x, y;
	int has_scale;
	double scale;
	int disabled;
};

int hypr_request(const char *command, char *reply, size_t cap);
int hypr_parse_monitors(const char *text, struct hypr_monitor *out, int max);
int hypr_build_rule(const struct hypr_rule *rule, char *out, size_t cap);
int hypr_apply_rule(const struct hypr_rule *rule, char *err, size_t err_cap);
int hypr_reload(char *err, size_t err_cap);

#endif
