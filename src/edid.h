#ifndef KAGEMUSHA_EDID_H
#define KAGEMUSHA_EDID_H

#include <stdint.h>

#define KG_EDID_MAX_SIZE 256
#define KG_DIM_MIN 64
#define KG_DIM_MAX 16384
#define KG_HZ_MAX 480
#define KG_NAME_MAX 13

struct kg_timing {
	uint32_t pixel_khz;
	uint32_t hactive, hblank, hsync_offset, hsync_width;
	uint32_t vactive, vblank, vsync_offset, vsync_width;
};

struct kg_edid_params {
	uint32_t width;
	uint32_t height;
	uint32_t refresh_hz;
	const char *name;
	uint32_t width_mm;
	uint32_t height_mm;
};

int kg_cvt_rb(uint32_t width, uint32_t height, uint32_t refresh_hz, struct kg_timing *out);

int kg_edid_build(const struct kg_edid_params *params, uint8_t out[KG_EDID_MAX_SIZE]);

#endif
