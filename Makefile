CC := clang
CFLAGS := -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment -g -Wall -Wextra -Werror -Wpedantic

run: build
	./main

build: main.c arena.c
	$(CC) $(CFLAGS) -o main main.c arena.c
