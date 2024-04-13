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

typedef struct Chunk {
  struct Chunk *next;

  bool used : 1;

  size_t size : sizeof(size_t) - 1;
  size_t data[];
} Chunk;

Chunk *chunk_from_data(void *data) { return (Chunk *)data - 1; }

Chunk *chunk_alloc(size_t sz) {
  size_t data_sz = align(sz, _Alignof(Chunk));
  size_t total = data_sz + sizeof(Chunk);

  Chunk *chunk = malloc(total);
  assert(chunk != NULL);

  chunk->next = NULL;
  chunk->size = data_sz / sizeof(chunk->data[0]);
  chunk->used = true;

  return chunk;
}

void *arena_acquire(Arena *arena, size_t sz) {
  if (arena->head == NULL) {
    arena->head = chunk_alloc(sz);
    return arena->head->data;
  }

  Chunk *chunk_p = arena->head;

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

void arena_release(void *data) { chunk_from_data(data)->used = false; }

void arena_reset(Arena *arena) {
  Chunk *chunk = arena->head;

  while (chunk != NULL) {
    chunk->used = false;
    chunk = chunk->next;
  }
}

void arena_dealloc(Arena *arena) {
  Chunk *chunk = arena->head, *next_chunk;

  while (chunk != NULL) {
    next_chunk = chunk->next;
    free(chunk);
    chunk = next_chunk;
  }

  arena->head = NULL;
}
