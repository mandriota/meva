/******************************************************************************\
*                                                                              *
*    Mewa. Math EWAluator.                                                     *
*    Copyright (C) 2024 Mark Mandriota                                         *
*                                                                              *
*    This program is free software: you can redistribute it and/or modify      *
*    it under the terms of the GNU General Public License as published by      *
*    the Free Software Foundation, either version 3 of the License, or         *
*    (at your option) any later version.                                       *
*                                                                              *
*    This program is distributed in the hope that it will be useful,           *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*    GNU General Public License for more details.                              *
*                                                                              *
*    You should have received a copy of the GNU General Public License         *
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.    *
*                                                                              *
\******************************************************************************/

#include "arena.h"

#include "util.h"

#include <assert.h>
#include <stdbool.h> // IWYU pragma: keep

size_t align(size_t sz, size_t alignment) {
  return sz + (alignment - ((sz - 1) & (alignment - 1))) - 1;
}

struct Chunk {
  struct Chunk *next;

  bool used : 1;

  size_t size : 63;
  size_t data[];
};

struct Chunk *chunk_from_data(void *data) { return (struct Chunk *)data - 1; }

struct Chunk *chunk_alloc(size_t sz) {
  size_t data_sz = align(sz, _Alignof(struct Chunk));
  size_t total = data_sz + sizeof(struct Chunk);

  struct Chunk *chunk = malloc(total);
  assert(chunk != NULL);

  chunk->next = NULL;
  chunk->size = data_sz / sizeof(chunk->data[0]);
  chunk->used = true;

  return chunk;
}

void *arena_acquire(struct Arena *arena, size_t sz) {
  DBG_PRINT("allocating %zu bytes\n", sz);
  if (arena->head == NULL) {
    arena->head = chunk_alloc(sz);
    return arena->head->data;
  }

  struct Chunk *chunk_p = arena->head;

  while ((chunk_p->used || chunk_p->size * sizeof(chunk_p->data[0]) < sz) &&
         chunk_p->next != NULL) {
    chunk_p = chunk_p->next;
  }

  if (!chunk_p->used && chunk_p->size * sizeof(chunk_p->data[0]) >= sz) {
    chunk_p->used = true;
    return chunk_p->data;
  }
  
  chunk_p->next = chunk_alloc(sz);
  return chunk_p->next->data;
}

void *arena_reacquire(struct Arena *arena, void *data, size_t sz) {
  struct Chunk *dst_chunk = chunk_from_data(arena_acquire(arena, sz));
  size_t dst_sz = align(sz, _Alignof(struct Chunk)) * sizeof(struct Chunk);

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
    free(chunk);
    chunk = next_chunk;
  }

  arena->head = NULL;
}
