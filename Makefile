CC := gcc
CFLAGS :=

run: build
	./main

build: main.c
	gcc -o main main.c
