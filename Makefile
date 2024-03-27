CC := clang

ifeq ($(DEBUG),1)
	CFLAGS := -std=gnu2x -lreadline -O0 -fsanitize=address -fsanitize=undefined       \
		-fno-sanitize-recover=all -fsanitize=float-divide-by-zero    \
		-fsanitize=float-cast-overflow -fno-sanitize=null            \
		-fno-sanitize=alignment -g -Wall -Wextra -Werror -Wpedantic
else
	CFLAGS := -std=gnu2x -lreadline -DNDEBUG -O2 -Wall -Wextra -Werror -Wpedantic
endif

build: mewa.c arena.c util.c
	test -d "./bin" || mkdir bin

	$(CC) $(CFLAGS) -o bin/mewa mewa.c arena.c util.c

run: build
	./bin/mewa

install: build
	if [ -f /usr/local/bin/mewa ]; then \
		rm /usr/local/bin/mewa; 		\
	fi

	ln -n ./bin/mewa /usr/local/bin

