run: build
	./main

build:
	gcc -I/opt/local/include/ -L/opt/local/lib/ -o main main.c -lgmp
