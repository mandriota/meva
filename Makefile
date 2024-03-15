CC := clang
CFLAGS := -std=c2x -g -Wall -Wextra -Werror -Wpedantic

run: build
	./mewa

build: mewa.c arena.c
	$(CC) $(CFLAGS) -o mewa mewa.c arena.c
