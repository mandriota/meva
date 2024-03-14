CC := clang
CFLAGS := -std=c2x -fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment -g -Wall -Wextra -Werror -Wpedantic

run: build
	./meva

build: mewa.c arena.c
	$(CC) $(CFLAGS) -o mewa mewa.c arena.c
