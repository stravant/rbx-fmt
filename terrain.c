
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "terrain.h"

// static void printbytes(uint8_t *ptr, size_t length) {
// 	for (int i = 0; i < length; ++i) {
// 		uint8_t c = ptr[i];
// 		if (isprint(c)) {
// 			printf("%02X ", c);
// 		} else {
// 			printf("%02X ", c);
// 		}
// 	}
// 	printf("\n");
// }

int32_t read_int32(uint8_t **ptr) {
	int32_t value = *((int32_t*)(*ptr));
	*ptr += 4;
	return value;
}
int16_t read_int16(uint8_t **ptr) {
	int16_t value = *((int16_t*)(*ptr));
	*ptr += 2;
	return value;
}
int8_t read_int8(uint8_t **ptr) {
	int8_t value = *((int8_t*)(*ptr));
	*ptr += 1;
	return value;
}
static uint8_t read_uint8(uint8_t **ptr) {
	uint8_t value = *((uint8_t*)(*ptr));
	*ptr += 1;
	return value;
}
static uint16_t read_uint16(uint8_t **ptr) {
	uint16_t value = *((uint16_t*)(*ptr));
	*ptr += 2;
	return value;
}

uint16_t reverse_uint16(uint16_t value) {
	return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

const char *byte_to_binary(uint8_t x) {
	static char b[9];
	b[0] = '\0';
	int z;
	for (z = 128; z > 0; z >>= 1) {
		strcat(b, ((x & z) == z) ? "1" : "0");
	}
	return b;
}

uint8_t block_type(uint8_t d0) {
	// 0x38 mask -> 0011 1000
	return (d0 & 0x38) >> 3;
}

uint8_t block_rot(uint8_t d0) {
	// 0xC0 mask -> 1100 0000
	return (d0 & 0xC0) >> 6;
}

void read_segment_d0(uint8_t **ptr, uint16_t *left) {
	uint8_t tag = read_uint8(ptr);
	if (tag == 0x28) {
		uint16_t toskip = read_uint8(ptr);
		if (toskip == 0xFF) {	
			toskip = reverse_uint16(read_uint16(ptr));
		}
		printf("Empty  Segment [0x%04X]\n", toskip);
		assert(*left >= toskip);
		*left -= toskip;
	} else {
		uint8_t d0 = tag;
		uint16_t length_or_tag = read_uint8(ptr);
		uint16_t length;
		if (length_or_tag == 0xFF) {
			length = reverse_uint16(read_uint16(ptr));
		} else {
			length = length_or_tag;
		}
		printf("Normal Segment [0x%04X] %s: (block: %u, rot: %u)\n", 
			length, 
			byte_to_binary(d0),
			block_type(d0),
			block_rot(d0));
		assert(*left >= length);
		*left -= length;
	}
}

void read_segment_d1(uint8_t **ptr, uint16_t *left) {
	uint8_t tag = read_uint8(ptr);
	if (tag == 0x11) {
		uint16_t toskip = read_uint8(ptr);
		if (toskip == 0xFF) {	
			toskip = reverse_uint16(read_uint16(ptr));
		}
		printf("Empty  Segment [0x%04X]\n", toskip);
		assert(*left >= toskip);
		*left -= toskip;
	} else {
		uint8_t d0 = tag;
		uint16_t length_or_tag = read_uint8(ptr);
		uint16_t length;
		if (length_or_tag == 0xFF) {
			length = reverse_uint16(read_uint16(ptr));
		} else {
			length = length_or_tag;
		}
		printf("Normal Segment [0x%04X] %s: (material: %u)\n", 
			length, 
			byte_to_binary(d0),
			d0);
		assert(*left >= length);
		*left -= length;
	}
}

void read_chunk(uint8_t **ptr) {
	struct terrain_chunk chunk = {0};

	chunk.position_x = read_int16(ptr);
	chunk.position_y = read_int16(ptr);
	chunk.position_z = read_int16(ptr);

	uint16_t left = 0x4000;

	//printf("Pos: %d, %d, %d\n", 
	//	chunk.position_x,
	//	chunk.position_y,
	//	chunk.position_z);

	while (left > 0) {
		read_segment_d0(ptr, &left);
	}

	left = 0x4000;
	while (left > 0) {
		read_segment_d1(ptr, &left);
	}

	printf("\n");
}

struct rbx_terrain *translate_terrain(struct rbx_string *source) {
	uint8_t *ptr = source->data;
	uint8_t *start = ptr;

	// While there's data left, read chunks
	uint32_t chunk_count = 0;
	while ((ptr - start) < source->length) {
		read_chunk(&ptr);
		++chunk_count;
	}

	printf("Read %u chunks\n", chunk_count);
	printf("Read: %zx / %zx\n", (ptr - start), source->length);

	return NULL;
}