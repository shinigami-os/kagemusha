#ifndef KAGEMUSHA_MODE_H
#define KAGEMUSHA_MODE_H

#include <stdint.h>

int kg_parse_mode(const char *text, uint32_t *width, uint32_t *height, uint32_t *refresh_hz);

#endif
