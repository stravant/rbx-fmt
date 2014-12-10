
#include <stdio.h>

#include "rbx_read.h"
#include "rbx_write.h"

int main(int argc, char *argv[]) {
	FILE *infile = fopen(argv[1], "rb");
	FILE *outfile = fopen(argv[2], "wb");

	// Read in data
	struct rbx_file file;
	
	// Load data in
	rbx_file_load(&file, infile);

	// Write data out
	rbx_file_save(&file, outfile);

	// Create fresh rbx_file struct with no data in it
	rbx_file_new(&file);

	// Iterate the objects in the file
	for (int i = 0; i < rbx_file_length(&file); ++i) {
		struct rbx_object *obj = rbx_file_get(&file, i);

		// Get a property of the object
		const char *str = 
			rbx_obj_getprop_string(obj, "Name");

		// Set a property of the object
		struct rbx_udim2 position = {0};
		position.x.scale = 0.5f;
		position.y.scale = 1.0f;
		position.y.offset = -10;
		rbx_obj_setprop_udim2(obj, "Position", position);
	}

	// Create / delete objects
	struct rbx_object *part = rbx_obj_new(&file, "Part");

	// Create object with parent
	struct rbx_object *obj = rbx_obj_new(&file, "SpecialMesh", part);

	

	rbx_file_free(&file);
}