#include "http_server.h"

#include <stdio.h>
#include <string.h>

constexpr struct string CRLF = str("\x0D\x0A");

struct context {
  struct string input;
  const char *current;
  struct message message;
  bool error;
};

static bool is_alpha(uint32_t ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static bool is_digit(uint32_t ch) {
  return (ch >= '0' && ch <= '9');
}

static bool is_hexdigit(uint32_t ch) {
  return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z');
}

static bool is_token_character(uint32_t ch) {
  return ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&' || ch == '\'' || ch == '*' || ch == '+' ||
         ch == '-' || ch == '.' || ch == '^' || ch == '_' || ch == '`' || ch == '|' || ch == '~' || is_alpha(ch) ||
         is_digit(ch);
}

static bool is_field_character(uint32_t ch) {
  return (ch >= '!' && ch <= '~') || (ch >= 0x80 && ch <= 0xff);
}

static bool accept_char(struct context *ctx, uint32_t ch) {
  if ((uint32_t)(*ctx->current) == ch) {
    ctx->current++;
    return true;
  }
  return false;
}

static void expect_char(struct context *ctx, uint32_t ch) {
  if (!accept_char(ctx, ch)) ctx->error = true;
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

static bool accept_pchar(struct context *ctx) {
  uint32_t ch = ctx->current[0];

  if (ch == '%' && is_hexdigit(ctx->current[1]) && is_hexdigit(ctx->current[2])) {
    ctx->current += 3;
    return true;
  }

  if (is_alpha(ch) || is_digit(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~' || ch == '!' || ch == '$' ||
      ch == '&' || ch == '\'' || ch == '(' || ch == ')' || ch == '*' || ch == '+' || ch == ',' || ch == ';' ||
      ch == '=' || ch == '-' || ch == '.' || ch == '_' || ch == '~') {
    ctx->current++;
    return true;
  }

  return false;
}

static struct request_target parse_request_target(struct context *ctx) {
  struct string path = {};
  struct string query = {};

  if (accept_char(ctx, '/')) { // origin form
    path.data = ctx->current - 1;

    while (accept_pchar(ctx))
      ;

    if (accept_char(ctx, '/'))
      while (accept_pchar(ctx))
        ;

    path.size = ctx->current - path.data;

    // optional query
    if (accept_char(ctx, '?')) {
      query.data = ctx->current - 1;
      while (accept_pchar(ctx) || accept_char(ctx, '/') || accept_char(ctx, '?'))
        ;
      query.size = ctx->current - query.data;
    }
  } else {
    ctx->error = true;
    // TODO: implement absolute-form / authority-form / asterisk-form
  }

  return (struct request_target){.path = path, .query = query};
}

// rfc9112: 3. Request Line
static void parse_request_line(struct context *ctx) {
  ctx->message.method = expect_token(ctx);
  expect_char(ctx, ' ');
  ctx->message.request_target = parse_request_target(ctx);
  expect_char(ctx, ' ');
  expect_string(ctx, str("HTTP/"));
  ctx->message.version.major = expect_digit(ctx);
  expect_char(ctx, '.');
  ctx->message.version.minor = expect_digit(ctx);
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
bool message_parse(struct string input, struct message *message) {
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

  if (ctx.error) return false;

  *message = ctx.message;
  return true;
}
