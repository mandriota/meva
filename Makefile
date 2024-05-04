EXEC := mewa

CC := gcc
LIBS := -lreadline -DHAVE_LIBREADLINE -lm
CFLAGS := -std=gnu2x
WARNINGS := -Wall -Wextra -Wpedantic -Wno-multichar -Wformat-security

ifeq ($(DEBUG),1)
	WARNINGS += -Werror

	CFLAGS += -O0
	ifeq ($(CC),clang)
		CFLAGS += -fsanitize=address -fsanitize=undefined            \
		-fno-sanitize-recover=all -fsanitize=float-divide-by-zero    \
		-fsanitize=float-cast-overflow -fno-sanitize=null            \
		-fno-sanitize=alignment -g
	endif
else
	CFLAGS += -DNDEBUG -O2
endif

ifeq ($(COLOR),0)
	CFLAGS += -DNCOLORS
endif

ifeq ($(OS),Windows_NT)
	EXEC := $(EXEC).exe
endif

build: mewa.c util.c
	@echo "BUILDING EXECUTABLE"

	@[ -d "./bin" ] || mkdir bin

	$(CC) $(CFLAGS) $(WARNINGS) -o bin/$(EXEC) mewa.c util.c $(LIBS)

run: build
	@echo "RUNNING EXECUTABLE"
	./bin/mewa
