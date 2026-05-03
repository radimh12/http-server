#include "http_server.h"

#include <stdint.h>
#include <stdlib.h>

#define HASHMAP_INITIAL_SIZE 32
#define HASHMAP_MAX_SIZE 128
#define HASHMAP_GROW_FACTOR 2
#define HASHMAP_MAX_LOAD 0.75f

static uint32_t hash_string(struct string str) {
  // Uses Fowler–Noll–Vo hash function.
  uint32_t hash = 2166136261u;
  for (intptr_t i = 0; i < str.size; i++) {
    hash ^= (uint32_t)(str.data[i]);
    hash *= 16777619;
  }
  return hash;
}

void hashmap_insert(struct hashmap *hashmap, struct string key, struct string value) {
  // First try to overwrite an existing item.
  struct hashmap_item *existing_item = hashmap_lookup(hashmap, key);
  if (existing_item) {
    existing_item->value = value;
    return;
  }

  // Ignore the inserted item if at max capacity. Since the hashmap is used to store message header fields this will
  // not happen in practice as there are only a few per message.
  if (hashmap->used == HASHMAP_MAX_SIZE) return;

  // Ignore empty strings.
  if (key.size == 0) return;

  // Resize the hashmap if necessary (depending on load).
  float load = (float)(hashmap->used) / (float)(hashmap->capacity);

  if (hashmap->capacity == 0 || load >= HASHMAP_MAX_LOAD) {
    size_t curr_capacity = hashmap->capacity;
    size_t new_capacity = (curr_capacity == 0) ? HASHMAP_INITIAL_SIZE : curr_capacity * HASHMAP_GROW_FACTOR;
    size_t memory_size = sizeof(struct hashmap_item) * new_capacity;
    size_t clear_size = (new_capacity - curr_capacity) * sizeof(struct hashmap_item);

    struct hashmap_item *new_data = realloc(hashmap->data, memory_size);

    // Ignore the inserted item if realloc fails (this will not happen in practice).
    if (new_data == nullptr) return;

    memset(new_data + curr_capacity, 0, clear_size);

    hashmap->capacity = new_capacity;
    hashmap->data = new_data;
  }

  uint32_t hash = hash_string(key);
  size_t slot = hash % hashmap->capacity;

  // This loop is guaranteed to terminate as there are always free slots.
  for (;;) {
    if (hashmap->data[slot].key.size == 0) {
      hashmap->used++;
      hashmap->data[slot] = (struct hashmap_item){.key = key, .value = value};
      break;
    }
    slot++;
  }
}

struct hashmap_item *hashmap_lookup(struct hashmap *hashmap, struct string key) {
  if (hashmap->used == 0) return nullptr;

  uint32_t hash = hash_string(key);
  size_t preferred_slot = hash % hashmap->capacity;
  size_t actual_slot = preferred_slot;

  do {
    if (equals(hashmap->data[actual_slot].key, key)) return &hashmap->data[actual_slot];
    actual_slot = (actual_slot + 1) % hashmap->capacity;
  } while (actual_slot != preferred_slot);

  return nullptr;
}
