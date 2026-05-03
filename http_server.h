#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define str(s) (struct string){.data = (s), .size = sizeof(s) - 1}

struct string {
  const char *data;
  int32_t size;
};

struct hashmap_item {
  struct string key;
  struct string value;
};

struct hashmap {
  size_t capacity;
  size_t used;
  struct hashmap_item *data;
};

struct request_target {
  struct string path;
  struct string query;
};

struct message {
  struct string method;
  struct request_target request_target;
  struct {
    uint8_t major;
    uint8_t minor;
  } version;
  struct hashmap header_fields;
};

extern bool message_parse(struct string input, struct message *message);

static inline bool equals(struct string a, struct string b) {
  return a.size == b.size && memcmp(a.data, b.data, a.size) == 0;
}

extern void hashmap_insert(struct hashmap *hashmap, struct string key, struct string value);
extern struct hashmap_item* hashmap_lookup(struct hashmap *hashmap, struct string key);
