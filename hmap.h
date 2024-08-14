#ifndef HMAP_H
#define HMAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

// murmur_hash64 - returns hash of key;
// **Returns**:
// - 0 if key = 0;
// - [1;2^64) if key != 0;
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

// map_get - sets \*val to \*entries.val where key = entries.key;
// **Defining**:
// - T as underlying type of val and entries.val;
// **Requires**:
// - entries is valid;
// - entries.key = 0 for all entries where key is not set;
// - entries.key is unique;
// - entries_cap != 0;
// - entries_cap = real count of entries;
// - key != 0;
// - ((T\*) val) is valid;
// - val != &entries.val where key = entries.key;
// - val_sz = real size of (\*(\*T) val) in bytes;
// **Modifies**:
// - (\*(T\*) val) if key is found;
// **Returns**:
// - true if key is found;
// - false if key is not found;
static inline bool map_get(Map_Entry *restrict entries, size_t entries_cap,
                           uint64_t key, void *restrict val, size_t val_sz) {
  size_t index = murmur_hash64(key) % entries_cap;
  size_t steps = 0;

  size_t entry_len =
      align(sizeof(Map_Entry) + val_sz, sizeof(Map_Entry)) / sizeof(Map_Entry);

  do {
    Map_Entry *entry = &entries[index * entry_len];

    if (entry->key == key) {
      memcpy(val, entry->val, val_sz);
      return true;
    }

    index = (index + 1) % entries_cap;
    ++steps;
  } while (steps < entries_cap);

  return false;
}

#define MAP_GET(entries, cap, key, val)                                        \
  map_get(entries, cap, key, val, sizeof(*val))

// map_set - sets \*entries.val to \*val where key = entries.key;
// **Defining**:
// - T as underlying type of val and entries.val;
// **Requires**:
// - entries is valid;
// - entries.key = 0 for all entries where key is not set;
// - entries.key is unique;
// - entries_cap != 0;
// - entries_cap = real count of entries;
// - key != 0;
// - ((T\*) val) is valid;
// - val != &entries.val where key = entries.key;
// - val_sz = real size of (\*(\*T) val) in bytes;
// **Modifies**:
// - ((T\*) entries.val) if key is found;
// **Returns**:
// - true if key is found;
// - false if key is not found;
static inline bool map_set(Map_Entry *restrict entries, size_t entries_cap,
                           uint64_t key, void *restrict val, size_t val_sz) {
  size_t index = murmur_hash64(key) % entries_cap;
  size_t steps = 0;

  size_t entry_len =
      align(sizeof(Map_Entry) + val_sz, sizeof(Map_Entry)) / sizeof(Map_Entry);

  do {
    Map_Entry *entry = &entries[index * entry_len];

    if (entry->key == key || entry->key == 0) {
      entry->key = key;
      memcpy(entry->val, val, val_sz);
      return true;
    }

    index = (index + 1) % entries_cap;
    ++steps;
  } while (steps < entries_cap);

  return false;
}

#define MAP_SET(entries, cap, key, val)                                        \
  map_set(entries, cap, key, val, sizeof(*val))

#endif
