#pragma once

#include <stddef.h>
#include <stdint.h>

#define str(s) (struct string){.data = (s), .size = sizeof(s) - 1}

struct string {
  const char *data;
  int32_t size;
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
};

extern bool message_parse(struct string input, struct message *message);
