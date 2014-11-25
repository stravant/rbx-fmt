
CC=gcc -std=c99 -Wall -Werror -static

INCLUDE=-Ilz4

LINK=-Llz4

fmt_rbx: fmt_rbx.h fmt_rbx.c
	$(CC) $(LINK) $(INCLUDE) -c fmt_rbx.c

main: main.c fmt_rbx
	$(CC) $(LINK) $(INCLUDE) -o main main.c fmt_rbx.o -llz4

test: main
	rm -rf test_file.dump
	./main test_file.rbxl > test_file.dump