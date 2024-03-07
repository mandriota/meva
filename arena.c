#include "arena.h"

#include <assert.h>
#include <stdbool.h>
#include <sys/mman.h>

size_t align(size_t sz) {
  return sz + (sizeof sz - ((sz - 1) & (sizeof sz - 1))) - 1;
}

struct Chunk {
  struct Chunk *next;
  size_t size;

  bool used;

  size_t data[];
};

struct Chunk *chunk_from_data(void *data) { return (struct Chunk *)data - 1; }

struct Chunk *chunk_alloc(size_t sz) {
  size_t sz_aligned = align(sz);
  size_t total = sz_aligned + sizeof(struct Chunk);

  struct Chunk *chunk = (struct Chunk *)mmap(
      NULL, total, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(chunk != MAP_FAILED);

  chunk->size = sz_aligned / sizeof(chunk->data[0]);
  chunk->used = true;

  return chunk;
}

void chunk_dealloc(struct Chunk *chunk) {
  assert(munmap(chunk, chunk->size + sizeof(struct Chunk)) == 0);
}

void *arena_acquire(struct Arena *arena, size_t sz) {
  if (arena->head == NULL) {
    arena->head = chunk_alloc(sz);
    return arena->head->data;
  }

  struct Chunk *chunk = arena->head;

  while ((chunk->used || chunk->size * sizeof(size_t) < sz) &&
         chunk->next != NULL)
    chunk = chunk->next;

  if (!chunk->used) {
    chunk->used = true;
    return chunk->data;
  }

  chunk->next = chunk_alloc(sz);
  return chunk->next->data;
}

void *arena_reacquire(struct Arena *arena, void *data, size_t sz) {
  struct Chunk *dst_chunk = chunk_from_data(arena_acquire(arena, sz));
  size_t dst_sz = align(sz) * sizeof(struct Chunk);

  struct Chunk *src_chunk = chunk_from_data(data);
  size_t min = dst_sz < src_chunk->size ? dst_sz : src_chunk->size;
  for (size_t i = 0; i < min; ++i)
    dst_chunk->data[i] = src_chunk->data[i];

  return dst_chunk->data;
}

void arena_release(void *data) { chunk_from_data(data)->used = false; }

void arena_reset(struct Arena *arena) {
  struct Chunk *chunk = arena->head;

  while (chunk != NULL) {
    chunk->used = false;
    chunk = chunk->next;
  }
}

void arena_dealloc(struct Arena *arena) {
  struct Chunk *chunk = arena->head, *next_chunk;

  while (chunk != NULL) {
    next_chunk = chunk->next;
    chunk_dealloc(chunk);
    chunk = next_chunk;
  }

  arena->head = NULL;
}
