
#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "rbx_types.h"

struct rbx_file {
	uint32_t type_count;
	struct rbx_object_class *type_array;
	uint32_t object_count;
	struct rbx_object *object_array;
};

struct rbx_file *read_rbx_file(void *data, size_t length);

void free_rbx_file(struct rbx_file *file);