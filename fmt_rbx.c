
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <alloca.h>

#include "fmt_rbx.h"
#include "lz4.h"

#define UNUSED(x) (void)(x)

typedef unsigned char uchar;

struct lz4_data {
	uint8_t *data;
	size_t length;
};

void printbytes(uint8_t *ptr, size_t length) {
	for (int i = 0; i < length; ++i) {
		uchar c = ptr[i];
		if (isprint(c)) {
			printf("%02X|%c ", c, (char)c);
		} else {
			printf("%02X|? ", c);
		}
	}
	printf("\n");
}

/* Check that the header is what we expect */
int checkheader(uint8_t *header) {
	return (0 == strncmp("<roblox!", (const char*)header, 8));
}

/* Read in an X bit integer (No interleaving or biasing) */
uint64_t read_uint64(uint8_t **ptr) {
	int64_t value = *((uint64_t*)(*ptr));
	*ptr += 8;
	return value;
}
uint32_t read_uint32(uint8_t **ptr) {
	int32_t value = *((uint32_t*)(*ptr));
	*ptr += 4;
	return value;
}
uint8_t read_uint8(uint8_t **ptr) {
	int8_t value = *((uint8_t*)(*ptr));
	*ptr += 1;
	return value;
}

uint32_t reverse_endianness(uint32_t value) {
	return ((value & 0xFF000000) >> 24) |
	       ((value & 0x00FF0000) >> 8 ) | 
	       ((value & 0x0000FF00) << 8 ) |
	       ((value & 0x000000FF) << 24);
}

/* Read "folded" signed int32s */
int32_t read_folded_int(uint8_t **ptr) {
	uint32_t raw = read_uint32(ptr);

	// Fix endianness
	uint32_t little = reverse_endianness(raw);

	// Unfold value
	int32_t value;
	if (little & 0x1) {
		value = -((int32_t)(little >> 1));
	} else {
		value = little >> 1;
	}
	return value;
}

/* Read a "roblox float" */
float read_roblox_float(uint8_t **ptr) {
	// Get the integer version
	uint32_t integer = reverse_endianness(read_uint32(ptr));

	// Move the sign bit to the start
	integer = (integer >> 1) | ((integer & 0x00000001) << 31);

	// Reinterpret cast to float
	float f = *(float*)&integer;

	return f;
}

/* De-interleave an array of interleaved 32 bit values */
void unmix_32_array(uint8_t *ptr, size_t length) {
	unsigned int count = length / 4;

	// Allocate space to write the unmixed elements into
	uint8_t *tmp = (uint8_t*)malloc(length);

	// De-interleave into the buffer
	for (int i = 0; i < count; ++i) {
		for (int j = 0; j < 4; ++j) {
			tmp[i*4 + j] = ptr[i + j*count];
		}
	}
	
	// Copy back to the buffer
	memcpy(ptr, tmp, length);
}

/* Read in bytes of padding */
void read_padding(uint8_t **ptr, size_t count) {
	*ptr += count;
}

/* Read in text that should have a constant known value */
int read_const(uint8_t **ptr, const char *text) {
	int status = (0 == strncmp((char*)(*ptr), text, strlen(text)));
	*ptr += strlen(text);
	return status;
}

/* Read in compressed data */
int read_compressed(uint8_t **ptr, struct lz4_data *output) {
	// Read in the compression header
	uint32_t compressed_length = read_uint32(ptr);
	uint32_t decompressed_length = read_uint32(ptr);
	printf("Ratio: %f\n", ((float)compressed_length)/((float)decompressed_length));
	read_padding(ptr, 4);

	// Try to decompress
	uint8_t *buffer = (uint8_t*)malloc(decompressed_length);
	int res = LZ4_decompress_safe((char*)(*ptr), (char*)buffer, 
		compressed_length, decompressed_length);

	// Advance read pointer
	*ptr += compressed_length;

	// Write out the result
	output->data = buffer;
	output->length = decompressed_length;

	if (res < 0) {
		return 0;
	} else {
		return 1;
	}
}

/* Read in a file record */
int read_file_record(uint8_t **ptr, const char *tag, struct lz4_data *output) {
	// Read / check the tag
	if (!read_const(ptr, tag)) {
		printf("Bad file record tag\n");
		return 0;
	}

	// Decompress the stuff
	if (!read_compressed(ptr, output)) {
		printf("Error decompressing.\n");
		return 0;
	}

	return 1;
}

/* Read a type record */
int read_type_record(uint8_t **ptr) {
	// Get the record
	struct lz4_data record;
	if (!read_file_record(ptr, "INST", &record)) {
		return 0;
	}
	uint8_t *recordptr = record.data;

	// Get the type ID and type name
	uint32_t type_id = read_uint32(&recordptr);
	uint32_t name_length = read_uint32(&recordptr);
	uint8_t *name = recordptr;
	recordptr += name_length;

	printf("(%zu) Type <%u> '%.*s'\n", record.length, type_id, name_length, name);

	// Has referent array?
	uint8_t has_additional_data = read_uint8(&recordptr);
	printf(" |has_additional_data: %d\n", (int)has_additional_data);

	// Instance count
	uint32_t instance_count = read_uint32(&recordptr);
	printf(" |instance_count: %d\n", (int)instance_count);

	// Unmix the referent array
	unmix_32_array(recordptr, instance_count*4);

	// Referent array
	printf(" |referents:\n | ");
	int32_t referent = 0;
	for (int i = 0; i < instance_count; ++i) {
		referent += read_folded_int(&recordptr);
		printf("%u ", referent);
	}
	printf("\n");

	// Additional data
	if (has_additional_data) {
		printf(" |additional data:\n | ");
		for (int i = 0; i < instance_count; ++i) {
			uint8_t extra = read_uint8(&recordptr);
			printf("%d ", (int)extra);
		}
		printf("\n");
	}
	printf(" '---\n\n");

	return 1;
}

/* Read in a values of a given property type */
int read_values(uint8_t type, uint8_t **ptr, size_t length) {
	uint8_t *after = (*ptr) + length;
	int value_count = 0;

	if (type == 0x1) {
		// Read list of strings
		while (*ptr < after) {
			// Read a string
			size_t length = read_uint32(ptr);
			uint8_t *data = *ptr;
			*ptr += length;

			++value_count;

			// Print it
			if (length < 32) {
				printf(" |   '%.*s'\n", (int)length, data);
			} else {
				printf(" |   '%.*s [%d characters]'\n", 32, data, (int)length);
			}
		}
	} else if (type == 0x2) {
		// Array of booleans
		while (*ptr < after) {
			// Read the bool
			uint8_t value = read_uint8(ptr);

			++value_count;

			if (value) {
				printf(" |   true\n");
			} else {
				printf(" |   false\n");	
			}
		}
	} else if (type == 0x3) {
		// Integer values
		value_count = length / 4;
		unmix_32_array(*ptr, length);
		for (int i = 0; i < value_count; ++i) {
			int32_t value = read_folded_int(ptr);
			printf(" |   %d\n", value);
		}
	} else if (type == 0x4) {
		// Float values
		value_count = length / 4;
		unmix_32_array(*ptr, length);
		for (int i = 0; i < value_count; ++i) {
			float value = read_roblox_float(ptr);
			printf(" |   %f\n", value);
		}
	} else if (type == 0x5) {
		// Lua_Number values
		value_count = length / 8;
		for (int i = 0; i < value_count; ++i) {
			uint64_t ivalue = read_uint64(ptr);
			double d = *(double*)&ivalue;
			printf(" |   %f\n", d);
		}
	} else if (type == 0x6) {
		// UDim value ???
	} else if (type == 0x7) {
		// UDim2 values
		value_count = length / 16;
		size_t block_length = length / 4;

		// Pointers
		uint8_t *scalexptr = *ptr + 0*block_length;
		uint8_t *scaleyptr = *ptr + 1*block_length;
		uint8_t *offsetxptr = *ptr + 2*block_length;
		uint8_t *offsetyptr = *ptr + 3*block_length;

		// Unmix the arrays for each of the components
		unmix_32_array(scalexptr, block_length);
		unmix_32_array(scaleyptr, block_length);
		unmix_32_array(offsetxptr, block_length);
		unmix_32_array(offsetyptr, block_length);

		// Get the values
		for (int i = 0; i < value_count; ++i) {
			float scalex = read_roblox_float(&scalexptr);
			float scaley = read_roblox_float(&scaleyptr);
			int32_t offsetx = read_folded_int(&offsetxptr);
			int32_t offsety = read_folded_int(&offsetyptr);
			printf(" |   {(%.2f, %d), (%.2f, %d)}\n",
				scalex, offsetx, scaley, offsety);
		}
	} else if (type == 0x8) {
		// Ray value
	} else if (type == 0x9) {
		// Faces
	} else if (type == 0xA) {
		// Axis
	} else if (type == 0xB) {
		// BrickColor
	} else if (type == 0xC) {
		// Color3
	} else if (type == 0xD) {
		// Vector2
	} else if (type == 0xE) {
		// Vector3
	} else if (type == 0xF) {
		// ???
	} else if (type == 0x10) {
		// Cframe
	} else if (type == 0x11) {
		// ???
	} else if (type == 0x12) {
		// Token
	} else if (type == 0x13) {
		// Referent
	} else {
		return 0;
	}
	return value_count;
}

/* Read a property record */
int read_prop_record(uint8_t **ptr) {
	// Get the record
	struct lz4_data record;
	if (!read_file_record(ptr, "PROP", &record)) {
		return 0;
	}
	uint8_t *recordptr = record.data;

	// Object belonging to
	uint32_t type_containing = read_uint32(&recordptr);

	// Name
	uint32_t name_length = read_uint32(&recordptr);
	uint8_t *name = recordptr;
	recordptr += name_length;

	// Property type
	uint8_t prop_type = read_uint8(&recordptr);

	// Read in values
	uint8_t *after = record.data + record.length;
	size_t space_left = after - recordptr;

	printf("Property of <%u> '%.*s'\n", type_containing, name_length, name);
	printf(" | Type: %x\n", prop_type);
	printf(" | Values:\n");
	int value_count = read_values(prop_type, &recordptr, space_left);
	printf(" | (Value Count = %d)\n", value_count);
	printf(" '----\n\n");

	return 1;
}

struct rbx_file *read_rbx_file(void *data, size_t length) {
	printf("Read: %p <%#zx>\n", data, length);

	// Current position in data
	uint8_t *ptr = (uchar*)data;

	// 16 byte header
	uint8_t *header = ptr;
	ptr += 16;

	if (!checkheader(header)) {
		printf("Bad Header\n");
		return NULL;
	}

	// Number of types and objects
	uint32_t typecount = read_uint32(&ptr);
	uint32_t objectcount = read_uint32(&ptr);

	printf("Type Count: %u\nObject Count: %u\n", typecount, objectcount);

	// 8 bytes of 0x0
	read_padding(&ptr, 8);

	// Type headers
	for (int i = 0; i < typecount; ++i) {
		read_type_record(&ptr);
	}

	// Property records
	for (;;) {
		if (!read_prop_record(&ptr)) {
			break;
		}
	}

	return NULL;
}