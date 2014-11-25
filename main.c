
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

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
	if (!data) {
		printf("Error mapping the file.\n");
		exit(EXIT_FAILURE);
	}

	/* Do the thing */
	struct rbx_file *file = read_rbx_file(data, file_length);

	if (file) {
		//printf("Success\n");
		free(file);
	} else {
		//printf("Failure\n");
	}

	return EXIT_SUCCESS;
}