CC := clang

ifeq ($(DEBUG),1)
	CFLAGS := -std=c2x -fsanitize=address -fsanitize=undefined       \
		-fno-sanitize-recover=all -fsanitize=float-divide-by-zero    \
		-fsanitize=float-cast-overflow -fno-sanitize=null            \
		-fno-sanitize=alignment -g -Wall -Wextra -Werror -Wpedantic
else
	CFLAGS := -std=c2x -O3 -Wall -Wextra -Werror -Wpedantic
endif

run: build
	./mewa

build: mewa.c arena.c
	$(CC) $(CFLAGS) -o mewa mewa.c arena.c
