#pragma once

#include <stddef.h>
#include <stdint.h>

#define str(s) (struct string){.data = (s), .size = sizeof(s) - 1}

struct string {
  const char *data;
  int32_t size;
};

extern void message_parse(struct string input);
