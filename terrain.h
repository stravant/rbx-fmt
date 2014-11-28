
#pragma once

#include <stdint.h>

#include "rbx_types.h"

struct terrain_chunk {
	int16_t position_x;
	int16_t position_y;
	int16_t position_z;
};

struct rbx_terrain {

};

struct rbx_terrain *translate_terrain(struct rbx_string *source);