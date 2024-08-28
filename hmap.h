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

#ifndef HMAP_H
#define HMAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//=:util:memory

// align - returns aligned memory;
/*@ requires alignment > 0;
  @ ensures \result % alignment == 0;
  @ ensures \result >= sz;
  @ ensures \result - sz < alignment;
  @ ensures sz % alignment == 0 <==> sz == \old(sz);
  @ assigns \nothing;
 */
static inline size_t align(size_t sz, size_t alignment) {
  return (sz + alignment - 1) & ~(alignment - 1);
}

//=:util:hash

// murmur_hash64 - returns hash of key;
/*@ ensures \result == 0 <==> key == 0;
  @ ensures \result != 0 <==> key != 0;
  @ assigns \nothing;
 */
static uint64_t murmur_hash64(uint64_t key) {
  key ^= key >> 33;
  key *= 0xff51afd7ed558ccd;
  key ^= key >> 33;
  key *= 0xc4ceb9fe1a85ec53;
  key ^= key >> 33;
  return key;
}

//=:hmap:hmap

typedef struct {
  uint64_t key;
  uint64_t val[];
} Map_Entry;

//@ ghost size_t entry_last = sizeof(Map_Entry) - 1;
//@ logic integer entry_len(size_t val_sz) = 1 + ((val_sz + entry_last) & ~entry_last) / (entry_last + 1);

/*@ predicate is_key_match_or_empty(Map_Entry *entries, integer entry_len, integer i, uint64_t key) =
  @   entries[i * entry_len].key == key || entries[i * entry_len].key == 0;
*/

// map_get - sets \*val to \*entries.val where key = entries.key;
/*@ requires entries_cap != 0;
	@ requires \valid(entries + (0 .. entries_cap*entry_len(val_sz) - 1));
  @ requires key != 0;
  @ requires \valid((char*) val + (0..val_sz-1));
  @ behavior key_found:
  @   assumes \exists integer i;
  @     0 <= i < entries_cap && entries[i * entry_len(val_sz)].key == key;
  @   ensures \result == true;
  @   assigns ((char*)val)[0 .. val_sz-1];
  @ behavior key_not_found:
  @   assumes \forall integer i;
  @     0 <= i < entries_cap ==> entries[i * entry_len(val_sz)].key != key;
  @   ensures \result == false;
  @   assigns \nothing;
  @ complete behaviors;
	@ disjoint behaviors;
*/
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

#define SET_GET(entries, cap, key) map_get(entries, cap, key, NULL, 0)

// map_set - sets \*entries.val to \*val where key = entries.key;
/*@ requires entries_cap != 0;
	@ requires \valid(entries + (0 .. entries_cap*entry_len(val_sz) - 1));
  @ requires key != 0;
  @ requires \valid((char*) val + (0 .. val_sz-1));
  @ behavior key_found:
  @   assumes \exists integer i;
  @      0 <= i < entries_cap && is_key_match_or_empty(entries, entry_len(val_sz), i, key);
  @   ensures \result;
  @   assigns ((char*) entries[0 .. entries_cap*entry_len(val_sz) - 1].val)[0..val_sz-1];
  @ behavior key_not_found:
  @   assumes \forall integer i;
  @     0 <= i < entries_cap ==> !is_key_match_or_empty(entries, entry_len(val_sz), i, key);
  @   ensures !\result;
  @   assigns \nothing;
  @ complete behaviors;
	@ disjoint behaviors;
*/
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

#define SET_SET(entries, cap, key) map_set(entries, cap, key, NULL, 0)

#endif
