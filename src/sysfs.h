#ifndef KAGEMUSHA_SYSFS_H
#define KAGEMUSHA_SYSFS_H

#include <stddef.h>

int kg_find_virtual_connector(const char *drm_root, char *out, size_t cap);

#endif
