EXEC := mewa

CC := gcc
LIBS := -lreadline -DHAVE_LIBREADLINE -lm
CFLAGS := -std=gnu2x
WARNINGS := -Wall -Wextra -Werror -Wpedantic

ifeq ($(DEBUG),1)
	CFLAGS += -O0
	ifeq ($(CC),clang)
		CFLAGS += -fsanitize=address -fsanitize=undefined            \
		-fno-sanitize-recover=all -fsanitize=float-divide-by-zero    \
		-fsanitize=float-cast-overflow -fno-sanitize=null            \
		-fno-sanitize=alignment -g
	else
		@echo "SANITIZER IS NOT SUPPORTED BY COMPILER"
	endif
else
	CFLAGS += -DNDEBUG -O2
endif

ifeq ($(OS),Windows_NT)
	EXEC := $(EXEC).exe
endif

build: mewa.c arena.c util.c
	@echo "BUILDING EXECUTABLE"

	@[ -d "./bin" ] || mkdir bin

	$(CC) $(CFLAGS) $(WARNINGS) -o bin/$(EXEC) mewa.c arena.c util.c $(LIBS)

run: build
	@echo "RUNNING EXECUTABLE"
	./bin/mewa
