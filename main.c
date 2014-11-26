
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "fmt_rbx.h"

int main(int argc, char *argv[]) {
	/* Check args */
	if (argc != 2) {
		printf("Bad arguments, usage: main filename\n");
		exit(EXIT_FAILURE);
	}

	/* Open input file */
	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		printf("Could not open the file.\n");
		exit(EXIT_FAILURE);
	}

	/* Get the file length */
	off_t file_length = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	/* Map the file */
	void *data = mmap(NULL, file_length, PROT_READ, MAP_PRIVATE, fd, 0x0);
	if (data == MAP_FAILED) {
		const char *s = strerror(errno);
		printf("Error mapping the file because %s.\n", s);
		exit(EXIT_FAILURE);
	}

	/* Do the thing */
	struct rbx_file *file = read_rbx_file(data, file_length);

	// Dump out the resulting data
	if (file != NULL) {
		printf("Success, details:\n");

		// Print out a dump of the info
		for (int i = 0; i < file->type_count; ++i) {
			struct rbx_object_class *type_info = (file->type_array + i);

			printf("Type <%u> '%s':\n", type_info->type_id, type_info->name.data);
			printf(" | Total of %u instances with %u properties\n",
				type_info->object_count, type_info->prop_count);
			struct rbx_object_prop *prop_info;
			for (prop_info = type_info->prop_list; prop_info; prop_info = prop_info->next) {
				printf(" | Property '%s'\n", prop_info->name.data);
			}
			printf(" '-------\n\n");
		}
		for (int i = 0; i < file->object_count; ++i) {
			struct rbx_object *object = (file->object_array + i);

			printf("Object <%u> '%s': \n", 
				object->referent,
				object->type->name.data);
			for (int i = 0; i < object->prop_value_count; ++i) {
				struct rbx_object_propentry *prop_entry = (object->prop_value_array + i);
				printf(" | %s = ", prop_entry->prop->name.data);
				uint8_t type = prop_entry->prop->value_type;
				switch (type) {
				case RBX_TYPE_STRING:
					if (prop_entry->value->string_value.length > 50) {
						printf("[%zu] \"%.*s\"...", 
							prop_entry->value->string_value.length,
							50, 
							prop_entry->value->string_value.data);
					} else {
						printf("\"%s\"", prop_entry->value->string_value.data);
					}
					break;
				case RBX_TYPE_BOOLEAN:
					if (prop_entry->value->boolean_value.data) {
						printf("true");
					} else {
						printf("false");
					}
					break;
				case RBX_TYPE_INT32:
					printf("%u", prop_entry->value->int32_value.data);
					break;
				case RBX_TYPE_FLOAT:
					printf("%f", prop_entry->value->float_value.data);
					break;
				case RBX_TYPE_REAL:
					printf("%f", prop_entry->value->real_value.data);
					break;
				case RBX_TYPE_UDIM2:
					printf("{(%f, %d), (%f, %d)}",
						prop_entry->value->udim2_value.x.scale,
						prop_entry->value->udim2_value.x.offset,
						prop_entry->value->udim2_value.y.scale,
						prop_entry->value->udim2_value.y.offset);
					break;
				case RBX_TYPE_BRICKCOLOR:
					printf("BrickColor(%u)", prop_entry->value->brickcolor_value.data);
					break;
				case RBX_TYPE_COLOR3:
					printf("Color3(%f, %f, %f)",
						prop_entry->value->color3_value.r,
						prop_entry->value->color3_value.g,
						prop_entry->value->color3_value.b);
					break;
				case RBX_TYPE_VECTOR2:
					printf("Vector2(%f, %f)",
						prop_entry->value->vector2_value.x,
						prop_entry->value->vector2_value.y);
					break;
				case RBX_TYPE_VECTOR3:
					printf("Vector3(%f, %f, %f)",
						prop_entry->value->vector3_value.x,
						prop_entry->value->vector3_value.y,
						prop_entry->value->vector3_value.z);
					break;
				case RBX_TYPE_CFRAME:
					printf("CFrame((%f, %f, %f))", //, (%f, %f, %f, %f, %f, %f, %f, %f, %f))",
						prop_entry->value->cframe_value.position.x,
						prop_entry->value->cframe_value.position.y,
						prop_entry->value->cframe_value.position.z);
						/*prop_entry->value->cframe_value.rotation[0],
						prop_entry->value->cframe_value.rotation[1],
						prop_entry->value->cframe_value.rotation[2],
						prop_entry->value->cframe_value.rotation[3],
						prop_entry->value->cframe_value.rotation[4],
						prop_entry->value->cframe_value.rotation[5],
						prop_entry->value->cframe_value.rotation[6],
						prop_entry->value->cframe_value.rotation[7],
						prop_entry->value->cframe_value.rotation[8]);*/
					break;
				case RBX_TYPE_TOKEN:
					printf("EnumValue(%u)", prop_entry->value->token_value.data);
					break;
				case RBX_TYPE_REFERENT:
					printf("Referent(%d)", prop_entry->value->referent_value.data);
					break;
				case RBX_TYPE_OBJECT:
					if (prop_entry->value->object_value.data == NULL) {
						printf("nil");
					} else {
						printf("Object at %p", prop_entry->value->object_value.data);
					}
					break;
				}
				printf("\n");
			}
			printf(" '------\n\n");
		}

		free_rbx_file(file);
	} else {
		printf("Failure, exiting.\n");
	}

	return EXIT_SUCCESS;
}