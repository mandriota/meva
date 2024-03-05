CC := gcc
CFLAGS := -Wall -Wextra -Werror -Wpedantic

run: build
	./main

build: main.c
	$(CC) $(CFLAGS) -o main main.c
