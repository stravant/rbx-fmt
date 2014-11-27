
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <alloca.h>
#include <assert.h>

#include "rbx_types.h"
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
		value = -((int32_t)((little + 1) >> 1));
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

/* Read a normal float */
float read_float32(uint8_t **ptr) {
	uint32_t integer = read_uint32(ptr);

	// Reinterpret cast
	return *(float*)&integer;
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

	// Free the buffer
	free(tmp);
}

/* Read in bytes of padding */
void read_padding(uint8_t **ptr, size_t count) {
	*ptr += count;
}

/* Read in text that should have a constant known value */
int read_const(uint8_t **ptr, const char *text) {
	int status = (0 == strncmp((char*)(*ptr), text, strlen(text)));
	if (status) {
		*ptr += strlen(text);
	}
	return status;
}

/* Free an rbx_string */
void free_string(struct rbx_string *string) {
	// string->data may be null, but that's okay
	free(string->data);
	string->data = NULL;
}

/* Free a compression record chunk */
void free_compressed(struct lz4_data *chunk) {
	// chunk->data may be NULL but that's okay
	free(chunk->data);
	chunk->data = NULL;
}

/* Read in compressed data */
int read_compressed(uint8_t **ptr, struct lz4_data *output) {
	// Read in the compression header
	uint32_t compressed_length = read_uint32(ptr);
	uint32_t decompressed_length = read_uint32(ptr);
	uint32_t padding = read_uint32(ptr);
	assert(padding == 0x0);

	// Try to decompress
	uint8_t *buffer = (uint8_t*)malloc(decompressed_length);
	int res = LZ4_decompress_safe((char*)(*ptr), (char*)buffer, 
		compressed_length, decompressed_length);

	// Advance read pointer
	*ptr += compressed_length;

	if (res < 0) {
		// Write out a failure and free the temp buffer
		output->data = NULL;
		output->length = 0;
		free(buffer);

		return 0;
	} else {
		// Write out the result
		output->data = buffer;
		output->length = decompressed_length;

		return 1;
	}
}

/* Read in a file record */
int read_file_record(uint8_t **ptr, const char *tag, struct lz4_data *output) {
	// Read / check the tag
	if (!read_const(ptr, tag)) {
		return 0;
	}

	// Decompress the stuff
	if (!read_compressed(ptr, output)) {
		return 0;
	}

	return 1;
}

/* Read a type record */
int read_type_record(uint8_t **ptr, struct rbx_object_class *type_info) {
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

	// Write type id
	type_info->type_id = type_id;

	// Write out the name
	type_info->name.data = (uint8_t*)malloc(name_length + 1);
	memcpy(type_info->name.data, name, name_length);
	type_info->name.data[name_length] = '\0';
	type_info->name.length = name_length;

	// Has additional data?
	uint8_t has_additional_data = read_uint8(&recordptr);

	// Instance count
	uint32_t instance_count = read_uint32(&recordptr);

	// Unmix the referent array
	unmix_32_array(recordptr, instance_count*4);

	// Prepare the referent array output
	type_info->object_count = instance_count;
	type_info->object_referent_array = 
		(uint32_t*)malloc(sizeof(uint32_t)*instance_count);

	// Referent array
	int32_t referent = 0;
	for (int i = 0; i < instance_count; ++i) {
		referent += read_folded_int(&recordptr);
		type_info->object_referent_array[i] = referent;
	}

	// Additional data
	if (has_additional_data) {
		for (int i = 0; i < instance_count; ++i) {
			uint8_t extra = read_uint8(&recordptr);
			UNUSED(extra);
		}
	}

	// Prepare additional fields in the output
	type_info->prop_count = 0;
	type_info->prop_list = NULL;

	// Free compression record
	free_compressed(&record);

	return 1;
}

/* Read in a values of a given property type */
struct rbx_value **read_values(uint8_t type, uint8_t **ptr, size_t length, uint32_t value_count) {
	uint8_t *after = (*ptr) + length;

	// Allocate space to store the translated values in
	struct rbx_value **values = (struct rbx_value**)malloc(value_count*sizeof(void*));
	struct rbx_value **output = values;
	memset(values, 0x0, value_count*sizeof(void*));

	if (type == RBX_TYPE_STRING) {
		// Read list of strings
		while (*ptr < after) {
			// Read a string
			size_t length = read_uint32(ptr);
			uint8_t *data = *ptr;
			*ptr += length;

			// Write into a value
			// (Allocate space for both the value and the string data in the same
			//  memory chunk, that way the string value can be freed with a
			//  single free call rather than requiring multiple ones.)
			struct rbx_value *value = 
				(struct rbx_value*)malloc(sizeof(struct rbx_value) + length + 1);
			uint8_t *str_storage = (uint8_t*)(value + 1);

			// Copy the string data into the chunk and null terminate it
			memcpy(str_storage, data, length);
			str_storage[length] = '\0';

			// Write to the chunk fields
			value->type = RBX_TYPE_STRING;
			value->string_value.data = str_storage;
			value->string_value.length = length;

			// Store back
			*(output++) = value;
		}
	} else if (type == RBX_TYPE_BOOLEAN) {
		// Array of booleans
		while (*ptr < after) {
			// Read the bool
			uint8_t bvalue = read_uint8(ptr);

			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_BOOLEAN;
			value->boolean_value.data = bvalue;
			*(output++) = value;
		}
	} else if (type == RBX_TYPE_INT32) {
		// Integer values
		unmix_32_array(*ptr, length);
		for (int i = 0; i < value_count; ++i) {
			int32_t ivalue = read_folded_int(ptr);
			
			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_INT32;
			value->int32_value.data = ivalue;
			*(output++) = value;
		}
	} else if (type == RBX_TYPE_FLOAT) {
		// Float values
		unmix_32_array(*ptr, length);
		for (int i = 0; i < value_count; ++i) {
			float fvalue = read_roblox_float(ptr);
			
			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_FLOAT;
			value->float_value.data = fvalue;
			*(output++) = value;
		}
	} else if (type == RBX_TYPE_REAL) {
		// Lua_Number values
		for (int i = 0; i < value_count; ++i) {
			uint64_t ivalue = read_uint64(ptr);
			double d = *(double*)&ivalue;
			
			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_REAL;
			value->real_value.data = d;
			*(output++) = value;
		}
	} else if (type == 0x6) {
		// Vector2int16, format unknown
	} else if (type == RBX_TYPE_UDIM2) {
		// UDim2 values
		size_t block_length = length / 4;

		// Pointers
		uint8_t *scalexptr = *ptr + 0*block_length;
		uint8_t *scaleyptr = *ptr + 1*block_length;
		uint8_t *offsetxptr = *ptr + 2*block_length;
		uint8_t *offsetyptr = *ptr + 3*block_length;
		*ptr += length;

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
			
			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_UDIM2;
			value->udim2_value.x.scale = scalex;
			value->udim2_value.x.offset = offsetx;
			value->udim2_value.y.scale = scaley;
			value->udim2_value.y.offset = offsety;
			*(output++) = value;
		}
	} else if (type == RBX_TYPE_RAY) {
		// Ray value
		// TODO:
	} else if (type == RBX_TYPE_FACES) {
		// Faces
		// TODO:
	} else if (type == RBX_TYPE_AXIS) {
		// Axis
		// TODO:
	} else if (type == RBX_TYPE_BRICKCOLOR) {
		// BrickColor
		unmix_32_array(*ptr, length);
		for (int i = 0; i < value_count; ++i) {
			uint32_t color_code = reverse_endianness(read_uint32(ptr));

			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_BRICKCOLOR;
			value->brickcolor_value.data = color_code;
			*(output++) = value;
		}
	} else if (type == RBX_TYPE_COLOR3) {
		// Color3
		size_t block_length = length / 3;

		// Pointers to components
		uint8_t *rptr = *ptr + 0*block_length;
		uint8_t *gptr = *ptr + 1*block_length;
		uint8_t *bptr = *ptr + 2*block_length;
		*ptr += length;

		// Unmix
		unmix_32_array(rptr, block_length);
		unmix_32_array(gptr, block_length);
		unmix_32_array(bptr, block_length);

		// Read
		for (int i = 0; i < value_count; ++i) {
			float r = read_roblox_float(&rptr);
			float g = read_roblox_float(&gptr);
			float b = read_roblox_float(&bptr);

			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_COLOR3;
			value->color3_value.r = r;
			value->color3_value.g = g;
			value->color3_value.b = b;
			*(output++) = value;			
		}

	} else if (type == RBX_TYPE_VECTOR2) {
		// Vector2
		size_t block_length = length / 2;

		// Element pointers
		uint8_t *x_ptr = *ptr + 0*block_length;
		uint8_t *y_ptr = *ptr + 1*block_length;

		// Unmix
		unmix_32_array(x_ptr, block_length);
		unmix_32_array(y_ptr, block_length);

		// Read
		for (int i = 0; i < value_count; ++i) {
			float x = read_roblox_float(&x_ptr);
			float y = read_roblox_float(&y_ptr);

			// Create value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_VECTOR2;
			value->vector2_value.x = x;
			value->vector2_value.y = y;
			*(output++) = value;	
		}

	} else if (type == RBX_TYPE_VECTOR3) {
		// Vector3
		size_t block_length = length / 3;

		// Element pointers
		uint8_t *x_ptr = *ptr + 0*block_length;
		uint8_t *y_ptr = *ptr + 1*block_length;
		uint8_t *z_ptr = *ptr + 2*block_length;

		// Unmix
		unmix_32_array(x_ptr, block_length);
		unmix_32_array(y_ptr, block_length);
		unmix_32_array(z_ptr, block_length);

		// Read
		for (int i = 0; i < value_count; ++i) {
			float x = read_roblox_float(&x_ptr);
			float y = read_roblox_float(&y_ptr);
			float z = read_roblox_float(&z_ptr);

			// Create value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_VECTOR3;
			value->vector3_value.x = x;
			value->vector3_value.y = y;
			value->vector3_value.z = z;
			*(output++) = value;	
		}

	} else if (type == 0xF) {
		// ???
	} else if (type == RBX_TYPE_CFRAME) {
		// Cframe

		// Unmix position data
		uint8_t *pos_ptr = *ptr + length - value_count*12;
		uint8_t *x_ptr = pos_ptr + 0*value_count;
		uint8_t *y_ptr = pos_ptr + 4*value_count;
		uint8_t *z_ptr = pos_ptr + 8*value_count;
		unmix_32_array(x_ptr, value_count*4);
		unmix_32_array(y_ptr, value_count*4);
		unmix_32_array(z_ptr, value_count*4);

		// Loop over main data
		for (int i = 0; i < value_count; ++i) {
			uint8_t tag = read_uint8(ptr);

			// Create value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_CFRAME;
			*(output++) = value;				

			// Rotation part
			if (tag == 0x0) {
				// Whole rotation matrix
				for (int j = 0; j < 9; ++j) {
					value->cframe_value.rotation[j] = read_float32(ptr);
				}
			} else if (tag == 0x1) {
				assert(0); // Unknown tag
			} else if (tag >= 0x2 && tag <= 0x23) {
				// Read special combinations
				// TODO: Implement
				for (int j = 0; j < 9; ++j) {
					value->cframe_value.rotation[j] = 0;
				}
			} else {
				assert(0); // Unknown tag
			}

			// Position part
			value->cframe_value.position.x = read_roblox_float(&x_ptr);
			value->cframe_value.position.y = read_roblox_float(&y_ptr);
			value->cframe_value.position.z = read_roblox_float(&z_ptr);
		}
	} else if (type == 0x11) {
		// ???
	} else if (type == RBX_TYPE_TOKEN) {
		// Token
		unmix_32_array(*ptr, length);

		for (int i = 0; i < value_count; ++i) {
			uint32_t tvalue = reverse_endianness(read_uint32(ptr));

			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_TOKEN;
			value->token_value.data = tvalue;
			*(output++) = value;		
		}
	} else if (type == RBX_TYPE_REFERENT) {
		// Referent
		unmix_32_array(*ptr, length);

		int32_t rvalue = 0;
		for (int i = 0; i < value_count; ++i) {
			int32_t my_value;
			int32_t diff = read_folded_int(ptr);
			if (diff != 0) {
				rvalue += diff;
				my_value = rvalue;
			} else {
				my_value = 0;
			}

			// Create the value
			struct rbx_value *value = malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_REFERENT;
			value->referent_value.data = my_value;
			*(output++) = value;	
		}
	} else {
		// ??
	}

	return values;
}

/* Read a property record */
int read_prop_record(uint8_t **ptr, struct rbx_object_class *type_array) {
	// Get the record
	struct lz4_data record;
	if (!read_file_record(ptr, "PROP", &record)) {
		return 0;
	}
	uint8_t *recordptr = record.data;

	// Object belonging to
	uint32_t type_containing_id = read_uint32(&recordptr);

	// Get the parent type record
	struct rbx_object_class *parent_type = type_array + type_containing_id;
	assert(parent_type != NULL);
	if (parent_type == NULL) {
		free_compressed(&record);
		return 0;
	}

	// Create a property in it
	struct rbx_object_prop *prop = 
		(struct rbx_object_prop*)malloc(sizeof(struct rbx_object_prop));
	prop->parent_type = parent_type;
	++parent_type->prop_count;
	prop->next = parent_type->prop_list;
	parent_type->prop_list = prop;

	// Name
	uint32_t name_length = read_uint32(&recordptr);
	uint8_t *name = recordptr;
	recordptr += name_length;

	// Write out the name
	prop->name.data = (uint8_t*)malloc(name_length + 1);
	prop->name.data[name_length] = '\0';
	memcpy(prop->name.data, name, name_length);
	prop->name.length = name_length;

	// Property type
	uint8_t prop_type = read_uint8(&recordptr);

	// Write out the property type
	prop->value_type = prop_type;

	// Read in values
	uint8_t *after = record.data + record.length;
	size_t space_left = after - recordptr;
	prop->value_array =
		read_values(prop_type, &recordptr, space_left, parent_type->object_count);

	// Free the compression record
	free_compressed(&record);

	return 1;
}

// Parent records
struct prnt_record {
	int32_t object;
	int32_t parent;
};

/* Read the PRNT record */
int read_parent_record(uint8_t **ptr, struct prnt_record *parents) {
	// Get the record
	struct lz4_data record;
	if (!read_file_record(ptr, "PRNT", &record)) {
		return 0;
	}
	uint8_t *recordptr = record.data;

	// Zero byte
	uint8_t parent_data_version = read_uint8(&recordptr);
	assert(parent_data_version == 0x0);

	// Get the object count
	uint32_t obj_count = read_uint32(&recordptr);

	size_t block_length = 4*obj_count;

	// Get pointers into data blocks, and unmix the data blocks
	uint8_t *refarray = recordptr + 0*block_length;
	uint8_t *pararray = recordptr + 1*block_length;
	unmix_32_array(refarray, block_length);
	unmix_32_array(pararray, block_length);

	// Read in the object, parent pairs (Stored differentially)
	int32_t object_ref = 0;
	int32_t parent_ref = 0;
	for (int i = 0; i < obj_count; ++i) {
		object_ref += read_folded_int(&refarray);
		parent_ref += read_folded_int(&pararray);
		parents[i].object = object_ref;
		parents[i].parent = parent_ref;
	}

	// Free the compression record
	free_compressed(&record);
	
	return 1;
}

/* Free an rbx_object_class */
void free_type(struct rbx_object_class *type) {
	// Free name
	free_string(&type->name);

	// Free each of the properties
	struct rbx_object_prop *prop = type->prop_list;
	while (prop != NULL) {
		// Save a reference to the next
		struct rbx_object_prop *next = prop->next;

		// Free the name
		free_string(&prop->name);

		// Don't free the property values here. Free them from the objects
		// using the values when they are freed, as the objects may have added
		// more properties that weren't parsed from a file, and aren't in the
		// value array.

		// But do free the property value array itself, all of it's data is
		// referenced in objects.
		free(prop->value_array);

		// Free the prop itself
		free(prop);

		// Go to the saved next
		prop = next;
	}
	type->prop_list = NULL;
	type->prop_count = 0;

	// Free the referent array (may be null)
	free(type->object_referent_array);
	type->object_referent_array = NULL;
}

/* Free an array of rbx_object_class-es */
void free_type_array(struct rbx_object_class *types, uint32_t count) {
	// Free each type
	for (uint32_t i = 0; i < count; ++i) {
		free_type(types + i);
	}

	// Free the array
	free(types);
}

/* Free an rbx_object */
void free_object(struct rbx_object *obj) {
	// Free the values that we have
	for (uint32_t i = 0; i < obj->prop_value_count; ++i) {
		free(obj->prop_value_array[i].value);
	}

	// Free the prop_value array
	free(obj->prop_value_array);
	obj->prop_value_array = NULL;
}

/* Free an array of rbx_object-s */
void free_object_array(struct rbx_object *array, uint32_t count) {
	// Free each type
	for (uint32_t i = 0; i < count; ++i) {
		free_object(array + i);
	}

	// Free the array
	free(array);	
}

/* Free an rbx_file struct */
void free_rbx_file(struct rbx_file *file) {
	// Free the arrays and clear out the data structure
	free_object_array(file->object_array, file->object_count);
	free_type_array(file->type_array, file->type_count);
	file->object_array = NULL;
	file->object_count = 0;
	file->type_array = NULL;
	file->type_count = 0;
}

struct rbx_file *read_rbx_file(void *data, size_t length) {
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

	// 8 bytes of 0x0
	uint64_t padding = read_uint64(&ptr);
	assert(padding == 0);
	if (padding != 0) {
		return NULL;
	}

	// Allocate space for the type info and zero it for debugging
	struct rbx_object_class *type_array = 
		malloc(sizeof(struct rbx_object_class) * typecount);
	memset(type_array, 0x0, sizeof(struct rbx_object_class*) * typecount);

	// Read in type info
	for (int i = 0; i < typecount; ++i) {
		if (!read_type_record(&ptr, type_array + i)) {
			// Free the types we read in so far
			free_type_array(type_array, i);
			return NULL;
		}
	}

	// Property records
	for (;;) {
		if (!read_prop_record(&ptr, type_array)) {
			break;
		}
	}

	// Parent records
	struct prnt_record *parents = 
		(struct prnt_record*)malloc(sizeof(struct prnt_record)*objectcount);
	if (!read_parent_record(&ptr, parents)) {
		return NULL;
	}

	// // End block
	// struct lz4_data record;
	// if (!read_file_record(&ptr, "END\0", &record)) {
	// 	return NULL;
	// }

	// Objects
	struct rbx_object *object_array = malloc(sizeof(struct rbx_object)*objectcount);

	// For each type
	for (int i = 0; i < typecount; ++i) {
		struct rbx_object_class *type_info = (type_array + i);

		// For each object of this type create the object
		for (uint32_t j = 0; j < type_info->object_count; ++j) {
			uint32_t referent = type_info->object_referent_array[j];

			// Get and set up the object
			struct rbx_object *object = (object_array + referent);
			object->type = type_info;
			object->referent = referent;
			object->prop_value_count = type_info->prop_count;
			// Note, here we make the length of the property value array equal
			// to the type's prop count + 1, since we are going to add a parent
			// property later.
			object->prop_value_array = 
				(struct rbx_object_propentry*)
					malloc(sizeof(struct rbx_object_propentry)*(type_info->prop_count + 1));

			// Write in the props
			struct rbx_object_prop *prop = type_info->prop_list;
			for (uint32_t k = 0; prop != NULL; prop = prop->next, ++k) {
				struct rbx_object_propentry *prop_entry = 
					&object->prop_value_array[k];	

				// Fill in the property entry on this object
				prop_entry->prop = prop;
				prop_entry->value = prop->value_array[j];

				// Referent translation
				//  Turn referent props into object props with pointers to the
				//  actual objects.
				if (prop->value_type == RBX_TYPE_REFERENT) {
					// Set to an object value type
					prop_entry->value->type = RBX_TYPE_OBJECT;

					// Translate the thing that it's referring to
					int32_t other_referent = prop_entry->value->referent_value.data;
					if (other_referent == -1) {
						// -1 => No object
						prop_entry->value->object_value.data = NULL;
					} else {
						// Otherwise, translate object
						prop_entry->value->object_value.data = &object_array[other_referent];
					}
				}
			}
		}

		// We already translated the referent values, but we haven't actually
		// changed the referent property definitions' types, do that now.
		//  For each TYPE_REFERENT property rewrite -> TYPE_OBJECT
		struct rbx_object_prop *prop = type_info->prop_list;
		for (; prop != NULL; prop = prop->next) {
			if (prop->value_type == RBX_TYPE_REFERENT) {
				prop->value_type = RBX_TYPE_OBJECT;
			}
		}
	}

	// For each type, we should add a parent property to it, and decode
	// the PRNT references.
	for (int i = 0; i < typecount; ++i) {
		struct rbx_object_class *type_info = (type_array + i);

		// Create parent property
		struct rbx_object_prop *parent_prop = malloc(sizeof(struct rbx_object_prop));
		parent_prop->value_type = RBX_TYPE_OBJECT;
		parent_prop->parent_type = type_info;
		parent_prop->value_array = NULL;

		// Name
		static const char *parent_name = "Parent";
		parent_prop->name.data = (uint8_t*)malloc(strlen(parent_name) + 1);
		memcpy(parent_prop->name.data, parent_name, strlen(parent_name));
		parent_prop->name.data[strlen(parent_name)] = '\0';
		parent_prop->name.length = strlen(parent_name);

		// Add to list
		parent_prop->next = type_info->prop_list;
		type_info->prop_list = parent_prop;

		// For each object, add the parent prop
		for (uint32_t j = 0; j < type_info->object_count; ++j) {
			int32_t referent = type_info->object_referent_array[j];
			struct rbx_object *object = (object_array + referent);

			// Add parent prop to count
			++object->prop_value_count;

			// Find the parent
			int32_t parent_referent;
			for (int k = 0; k < objectcount; ++k) {
				if (parents[k].object == referent) {
					parent_referent = parents[k].parent;
					break;
				}
			}

			// Get the parent object
			struct rbx_object *parent_object;
			if (parent_referent == -1) {
				parent_object = NULL;
			} else {
				parent_object = (object_array + parent_referent);
			}

			// Create the value
			struct rbx_value *value = 
				(struct rbx_value*)malloc(sizeof(struct rbx_value));
			value->type = RBX_TYPE_OBJECT;
			value->object_value.data = parent_object;

			// Set up the last property as the parent property
			// (Note: We have one extra space allocated after the
			//        normal prop_value_array for this property)
			struct rbx_object_propentry *entry = 
				(object->prop_value_array + type_info->prop_count);
			entry->prop = parent_prop;
			entry->value = value;
		}

		// Increment the prop count on the type
		++type_info->prop_count;
	}	 

	struct rbx_file *output = 
		(struct rbx_file*)malloc(sizeof(struct rbx_file));

	output->type_count = typecount;
	output->type_array = type_array;
	output->object_count = objectcount;
	output->object_array = object_array;

	return output;
}