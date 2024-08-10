#ifndef HMAP_H
#define HMAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

static uint64_t murmur_hash64(uint64_t key) {
  key ^= key >> 33;
  key *= 0xff51afd7ed558ccd;
  key ^= key >> 33;
  key *= 0xc4ceb9fe1a85ec53;
  key ^= key >> 33;
  return key;
}

typedef struct {
  uint64_t key;
  uint64_t val[];
} Map_Entry;

static inline bool map_get(Map_Entry *restrict entries, size_t cap,
                           uint64_t key, size_t sz, void *restrict val) {
  size_t index = murmur_hash64(key) % cap;
  size_t steps = 0;

  size_t entry_len =
      align(sizeof(Map_Entry) + sz, sizeof(Map_Entry)) / sizeof(Map_Entry);

  while (steps < cap) {
    Map_Entry *entry = &entries[index * entry_len];

    if (entry->key == key) {
      memcpy(val, entry->val, sz);
      return true;
    }

    index = (index + 1) % cap;
    ++steps;
  }

  return false;
}

#define MAP_GET(entries, cap, key, val)                                        \
  map_get(entries, cap, key, sizeof(*val), val)

static inline bool map_set(Map_Entry *restrict entries, size_t cap,
                           uint64_t key, size_t sz, void *restrict val) {
  size_t index = murmur_hash64(key) % cap;
  size_t steps = 0;

  size_t entry_len =
      align(sizeof(Map_Entry) + sz, sizeof(Map_Entry)) / sizeof(Map_Entry);

  while (steps < cap) {
    Map_Entry *entry = &entries[index * entry_len];

    if (entry->key == key || entry->key == 0) {
      entry->key = key;
      memcpy(entry->val, val, sz);
      return true;
    }

    index = (index + 1) % cap;
    ++steps;
  }

  return false;
}

#define MAP_SET(entries, cap, key, val)                                        \
  map_set(entries, cap, key, sizeof(*val), val)

#endif
