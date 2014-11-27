
CC=clang -std=c99 -Wall -Werror -static

INCLUDE=-Ilz4

LINK=-Llz4

all: main

lz4:
	make -C lz4

fmt_rbx: fmt_rbx.h fmt_rbx.c
	$(CC) $(LINK) $(INCLUDE) -c fmt_rbx.c

rbx_types: rbx_types.h rbx_types.c
	$(CC) $(INCLUDE) -c rbx_types.c

main: main.c fmt_rbx rbx_types lz4
	$(CC) $(LINK) $(INCLUDE) -o main main.c fmt_rbx.o rbx_types.o -llz4

debug: CC += -g
debug: main

test: debug
	rm -rf test_file.dump
	./main test_file.rbxl > test_file.dump