
#pragma once

#include <stdio.h>
#include <stdint.h>

/* See: 
 * http://developer.roblox.com/forum/development-discussion/10719-binary-file-format?limitstart=0#116475
 * For descriptions of the currently unused fields, to quote:
 *  "0x6 tag is for UDim (no properties of this type exist atm)
 *   0xF tag is for Vector2int16 (e.g. Scale9Frame.ScaleEdgeSize)
 *   0x11 tag is for CFrame serialization using quaternions - it's much more space efficient but does not result in perfect roundtrip so it is not enabled right now
 *   0x14 tag is for Vector3int16 (no properties of this type exist atm)"
 */

/* Types that a Roblox value can have */
#define RBX_TYPE_STRING     0x1
#define RBX_TYPE_BOOLEAN    0x2
#define RBX_TYPE_INT32      0x3
#define RBX_TYPE_FLOAT      0x4
#define RBX_TYPE_REAL       0x5
                         /* 0x6  Vector2int16 unused */
#define RBX_TYPE_UDIM2      0x7
#define RBX_TYPE_RAY        0x8
#define RBX_TYPE_FACES      0x9
#define RBX_TYPE_AXIS       0xA
#define RBX_TYPE_BRICKCOLOR 0xB
#define RBX_TYPE_COLOR3     0xC
#define RBX_TYPE_VECTOR2    0xD
#define RBX_TYPE_VECTOR3    0xE
                         /* 0xF  Vector3int16 unused */
#define RBX_TYPE_CFRAME     0x10
                         /* 0x11 Network CFrame serialization format */
#define RBX_TYPE_TOKEN      0x12
#define RBX_TYPE_REFERENT   0x13

/* Special type that we use for translated object referents */
#define RBX_TYPE_OBJECT     0xFF

/* Value types */
struct rbx_string {
	uint8_t *data;
	size_t length;
};
struct rbx_boolean {
	uint8_t data;
};
struct rbx_int32 {
	int32_t data;
};
struct rbx_float {
	float data;
};
struct rbx_real {
	double data;
};
struct rbx_udim {
	int32_t offset;
	float scale;
};
struct rbx_udim2 {
	struct rbx_udim x, y;
};
struct rbx_faces {
	uint8_t right;
	uint8_t top;
	uint8_t back;
	uint8_t left;
	uint8_t bottom;
	uint8_t front;
};
struct rbx_axis {
	uint8_t x, y, z;
};
struct rbx_brickcolor {
	uint32_t data;
};
struct rbx_color3 {
	float r, g, b;
};
struct rbx_vector2 {
	float x, y;
};
struct rbx_vector3 {
	float x, y, z;
};
struct rbx_ray {
	struct rbx_vector3 origin;
	struct rbx_vector3 direction;
};
struct rbx_cframe {
	float rotation[9]; /* = <0>R00 <1>R01 <2>R02 
	                        <3>R10 <4>R11 ... */
	struct rbx_vector3 position;
};
struct rbx_token {
	uint32_t data;
};
struct rbx_referent {
	int32_t data;
};
struct rbx_object_ref {
	void *data;
};

/* roblox value tagged union */
struct rbx_value {
	uint8_t type;
	union {
		struct rbx_string string_value;
		struct rbx_boolean boolean_value;
		struct rbx_int32 int32_value;
		struct rbx_float float_value;
		struct rbx_real real_value;
		struct rbx_udim2 udim2_value;
		struct rbx_faces faces_value;
		struct rbx_axis axis_value;
		struct rbx_brickcolor brickcolor_value;
		struct rbx_color3 color3_value;
		struct rbx_vector2 vector2_value;
		struct rbx_vector3 vector3_value;
		struct rbx_ray ray_value;
		struct rbx_cframe cframe_value;
		struct rbx_token token_value;
		struct rbx_referent referent_value;
		struct rbx_object_ref object_value;
	};
};

/* Property of a roblox object
 * - Properties are stored as a linked list. Since we don't know how many
 *   there are ahead of time, we allocate them one at a time and add them to
 *   their owning classes linked lists.
 */
struct rbx_object_prop {
	uint8_t value_type;
	struct rbx_object_class *parent_type; /* Type that this prop is for */
	struct rbx_string name;               /* Name of the property */
	struct rbx_value **value_array;       /* List of values, length =
	                                         parent_type.object_count */
	struct rbx_object_prop *next;         /* Next prop in linked list */
};

/* A type of roblox object 
 * - Types are stored in a flat array allocated ahead of time, since we know
 *   exactly how many type records there are in a file from it's header.
 */
struct rbx_object_class {
	uint32_t type_id;
	struct rbx_string name; /* Name of the type */
	uint32_t object_count;
	uint32_t *object_referent_array; /* Referents of the objects of this type */
	uint32_t prop_count; /* Updated as entries are added to the prop_list */
	struct rbx_object_prop *prop_list; /* linked list */
};

/* A roblox object
 * - An object, that is a bag of property -> rbx_value mappings
 *   with a referent id.
 */
struct rbx_object_propentry {
	struct rbx_object_prop *prop;
	struct rbx_value *value;
};
struct rbx_object {
	struct rbx_object_class *type;
	uint32_t prop_value_count;
	struct rbx_object_propentry *prop_value_array;
	uint32_t referent;
};