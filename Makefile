
CC=clang -std=c99 -Wall -Werror

INCLUDE=-Ilz4

LINK=-Llz4 

all: main

lz4:
	make -C lz4

fmt_terrain: terrain.h terrain.c
	$(CC) $(INCLUDE) -c terrain.c

fmt_rbx: fmt_rbx.h fmt_rbx.c
	$(CC) $(LINK) $(INCLUDE) -c fmt_rbx.c

rbx_types: rbx_types.h rbx_types.c
	$(CC) $(INCLUDE) -c rbx_types.c

xml_writer: xml_writer.h xml_writer.c 
	$(CC) $(INCLUDE) -c xml_writer.c -I/usr/include/libxml2 

main: main.c fmt_rbx rbx_types fmt_terrain xml_writer lz4
	$(CC) $(LINK) $(INCLUDE) -I/usr/include/libxml2 -o main main.c fmt_rbx.o rbx_types.o xml_writer.o terrain.o -llz4 -lxml2

debug: CC += -g
debug: main

test: debug
	rm -rf test_file.dump
	./main test_file.rbxl > test_file.dump
