#include "http_server.h"

#include <stdio.h>
#include <string.h>

constexpr struct string CRLF = str("\x0D\x0A");

struct context {
  struct string input;
  const char *current;
  bool error;
};

static bool is_token_character(uint32_t ch) {
  return ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&' || ch == '\'' || ch == '*' || ch == '+' ||
         ch == '-' || ch == '.' || ch == '^' || ch == '_' || ch == '`' || ch == '|' || ch == '~' ||
         (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static bool is_field_character(uint32_t ch) {
  return (ch >= '!' && ch <= '~') || (ch >= 0x80 && ch <= 0xff);
}

static void expect_char(struct context *ctx, uint32_t ch) {
  if ((uint32_t)(*ctx->current) == ch)
    ctx->current++;
  else
    ctx->error = true;
}

static struct string expect_token(struct context *ctx) {
  if (is_token_character(*ctx->current)) {
    const char *start = ctx->current++;
    while (is_token_character(*ctx->current))
      ctx->current++;
    return (struct string){.data = start, .size = ctx->current - start};
  }

  ctx->error = true;
  return (struct string){};
}

static bool accept_string(struct context *ctx, struct string str) {
  int32_t input_remaining = ctx->input.size - (ctx->current - ctx->input.data);

  if (input_remaining < str.size) return false;
  if (strncmp(ctx->current, str.data, str.size) != 0) return false;

  ctx->current += str.size;
  return true;
}

static void expect_string(struct context *ctx, struct string str) {
  if (!accept_string(ctx, str)) ctx->error = true;
}

static uint32_t expect_digit(struct context *ctx) {
  uint32_t ch = *ctx->current;

  if (ch >= '0' && ch <= '9') {
    ctx->current++;
    return ch - '0';
  } else {
    ctx->error = true;
    return 0;
  }
}

static void accept_whitespace(struct context *ctx) {
  while (*ctx->current == ' ' || *ctx->current == '\t')
    ctx->current++;
}

// rfc9112: 3. Request Line
static void parse_request_line(struct context *ctx) {
  struct string method = expect_token(ctx);
  expect_char(ctx, ' ');

  // TODO: actually parse request target
  expect_char(ctx, '/');

  expect_char(ctx, ' ');

  expect_string(ctx, str("HTTP/"));
  uint32_t major = expect_digit(ctx);
  expect_char(ctx, '.');
  uint32_t minor = expect_digit(ctx);

  printf("method: %.*s\n", method.size, method.data);
  printf("http version: %u.%u\n", major, minor);
}

// rfc9110: 5. Fields
static void parse_field_line(struct context *ctx) {
  struct string name = expect_token(ctx);
  expect_char(ctx, ':');
  accept_whitespace(ctx);

  struct string value = {};

  if (is_field_character(*ctx->current)) {
    value.data = ctx->current++;
    while (is_field_character(*ctx->current) || (*ctx->current == ' ') || (*ctx->current == '\t'))
      ctx->current++;
    value.size = ctx->current - value.data;
  }

  accept_whitespace(ctx);

  printf("field: '%.*s' = '%.*s'\n", name.size, name.data, value.size, value.data);
}

// rfc9112: 2. Message
void message_parse(struct string input) {
  struct context ctx = {
    .input = input,
    .current = input.data,
  };

  printf("%.*s\n", (int)(input.size), input.data);

  parse_request_line(&ctx);
  expect_string(&ctx, CRLF);

  while (!ctx.error && !accept_string(&ctx, CRLF)) {
    parse_field_line(&ctx);
    expect_string(&ctx, CRLF);
  }

  printf("parse result: %s\n", ctx.error ? "failed" : "OK");
}
