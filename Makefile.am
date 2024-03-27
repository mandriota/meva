bin_PROGRAMS = mewa
mewa_SOURCES = mewa.c arena.c util.c
mewa_CFLAGS = -std=gnu2x -lreadline -DNDEBUG -O2 -Wall -Wextra -Wpedantic

clean-local:
	@rm config.status configure config.log
	@rm Makefile
	@rm -r autom4te.cache/
	@rm aclocal.m4
	@rm compile install-sh missing Makefile.in
