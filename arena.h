#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>

struct Arena {
  struct Chunk *head;
};

void *arena_acquire(struct Arena *arena, size_t sz);

void *arena_reacquire(struct Arena *arena, void *data, size_t sz);

void arena_release(void *data);

void arena_reset(struct Arena *arena);

void arena_dealloc(struct Arena *arena);

#endif
