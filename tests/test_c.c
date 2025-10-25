// tests/smoke_c.c
// Minimal C smoke test for V4-front C API.
// Verifies that integer tokens emit [LIT imm32]* and that RET is appended.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <v4/opcodes.h>

#include "v4front/front_api.h"

static uint32_t rd32(const uint8_t* p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static int check_empty(void)
{
  V4FrontBuf buf = {0};
  char err[128] = {0};
  int rc = v4front_compile("", &buf, err, sizeof(err));
  if (rc != 0)
  {
    fprintf(stderr, "empty: rc=%d err=%s\n", rc, err);
    return 1;
  }
  if (buf.size != 1 || buf.data[0] != (uint8_t)V4_OP_RET)
  {
    fprintf(stderr, "empty: expected RET only\n");
    v4front_free(&buf);
    return 1;
  }
  v4front_free(&buf);
  return 0;
}

static int check_single_literal(void)
{
  V4FrontBuf buf = {0};
  char err[128] = {0};
  int rc = v4front_compile("42", &buf, err, sizeof(err));
  if (rc != 0)
  {
    fprintf(stderr, "single: rc=%d err=%s\n", rc, err);
    return 1;
  }
  if (buf.size != 1 + 4 + 1)
  {
    fprintf(stderr, "single: bad size=%u\n", (unsigned)buf.size);
    v4front_free(&buf);
    return 1;
  }
  if (buf.data[0] != (uint8_t)V4_OP_LIT || rd32(&buf.data[1]) != 42u ||
      buf.data[5] != (uint8_t)V4_OP_RET)
  {
    fprintf(stderr, "single: bad encoding\n");
    v4front_free(&buf);
    return 1;
  }
  v4front_free(&buf);
  return 0;
}

static int check_multiple_and_negative(void)
{
  V4FrontBuf buf = {0};
  char err[128] = {0};
  int rc = v4front_compile("1 2 -3", &buf, err, sizeof(err));
  if (rc != 0)
  {
    fprintf(stderr, "multi: rc=%d err=%s\n", rc, err);
    return 1;
  }

  if (buf.size != (1 + 4) * 3 + 1)
  {
    fprintf(stderr, "multi: bad size=%u\n", (unsigned)buf.size);
    v4front_free(&buf);
    return 1;
  }
  size_t k = 0;
  if (buf.data[k++] != (uint8_t)V4_OP_LIT)
  {
    fprintf(stderr, "multi: missing LIT #1\n");
    v4front_free(&buf);
    return 1;
  }
  if (rd32(&buf.data[k]) != 1u)
  {
    fprintf(stderr, "multi: imm #1\n");
    v4front_free(&buf);
    return 1;
  }
  k += 4;

  if (buf.data[k++] != (uint8_t)V4_OP_LIT)
  {
    fprintf(stderr, "multi: missing LIT #2\n");
    v4front_free(&buf);
    return 1;
  }
  if (rd32(&buf.data[k]) != 2u)
  {
    fprintf(stderr, "multi: imm #2\n");
    v4front_free(&buf);
    return 1;
  }
  k += 4;

  if (buf.data[k++] != (uint8_t)V4_OP_LIT)
  {
    fprintf(stderr, "multi: missing LIT #3\n");
    v4front_free(&buf);
    return 1;
  }
  if ((int32_t)rd32(&buf.data[k]) != -3)
  {
    fprintf(stderr, "multi: imm #3\n");
    v4front_free(&buf);
    return 1;
  }
  k += 4;

  if (buf.data[k] != (uint8_t)V4_OP_RET)
  {
    fprintf(stderr, "multi: missing RET\n");
    v4front_free(&buf);
    return 1;
  }
  v4front_free(&buf);
  return 0;
}

static int check_hex_and_bounds(void)
{
  V4FrontBuf buf = {0};
  char err[128] = {0};
  int rc = v4front_compile("0x10 2147483647 -2147483648", &buf, err, sizeof(err));
  if (rc != 0)
  {
    fprintf(stderr, "bounds: rc=%d err=%s\n", rc, err);
    return 1;
  }

  if (buf.size != (1 + 4) * 3 + 1)
  {
    fprintf(stderr, "bounds: bad size=%u\n", (unsigned)buf.size);
    v4front_free(&buf);
    return 1;
  }
  size_t k = 0;
  if (buf.data[k++] != (uint8_t)V4_OP_LIT)
  {
    fprintf(stderr, "bounds: LIT #1\n");
    v4front_free(&buf);
    return 1;
  }
  if (rd32(&buf.data[k]) != 0x10u)
  {
    fprintf(stderr, "bounds: imm #1\n");
    v4front_free(&buf);
    return 1;
  }
  k += 4;

  if (buf.data[k++] != (uint8_t)V4_OP_LIT)
  {
    fprintf(stderr, "bounds: LIT #2\n");
    v4front_free(&buf);
    return 1;
  }
  if (rd32(&buf.data[k]) != 2147483647u)
  {
    fprintf(stderr, "bounds: imm #2\n");
    v4front_free(&buf);
    return 1;
  }
  k += 4;

  if (buf.data[k++] != (uint8_t)V4_OP_LIT)
  {
    fprintf(stderr, "bounds: LIT #3\n");
    v4front_free(&buf);
    return 1;
  }
  if ((int32_t)rd32(&buf.data[k]) != (int32_t)0x80000000u)
  {
    fprintf(stderr, "bounds: imm #3\n");
    v4front_free(&buf);
    return 1;
  }
  k += 4;

  if (buf.data[k] != (uint8_t)V4_OP_RET)
  {
    fprintf(stderr, "bounds: RET\n");
    v4front_free(&buf);
    return 1;
  }
  v4front_free(&buf);
  return 0;
}

static int check_unknown_token(void)
{
  V4FrontBuf buf = {0};
  char err[128] = {0};
  int rc = v4front_compile("HELLO", &buf, err, sizeof(err));
  if (rc >= 0)
  {
    fprintf(stderr, "unknown: expected error\n");
    v4front_free(&buf);
    return 1;
  }
  if (err[0] == '\0')
  {
    fprintf(stderr, "unknown: empty error message\n");
    return 1;
  }
  v4front_free(&buf);
  return 0;
}

int main(void)
{
  int fails = 0;
  fails += check_empty();
  fails += check_single_literal();
  fails += check_multiple_and_negative();
  fails += check_hex_and_bounds();
  fails += check_unknown_token();

  if (fails)
  {
    fprintf(stderr, "C smoke failed: %d case(s)\n", fails);
    return 1;
  }
  printf("C smoke passed\n");
  return 0;
}
