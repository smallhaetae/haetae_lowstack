/*
 * Copyright 2026 YoungBeom Kim, Christine van Vredendaal, Gustavo Banegas, and Seog Chung Seo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "randombytes.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#warning Using a non-random randombytes

#include <string.h>

static const uint32_t seed_init[32] = {
    3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3,
    2, 3, 8, 4, 6, 2, 6, 4, 3, 3, 8, 3, 2, 7, 9, 5};
static uint32_t seed[32] = {
    3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3,
    2, 3, 8, 4, 6, 2, 6, 4, 3, 3, 8, 3, 2, 7, 9, 5};
static uint32_t call_counter = 0;
static uint32_t in[12];
static uint8_t out_buf[sizeof(uint32_t) * 16];
static int32_t outleft = 0;

#define ROTATE(x, b) (((x) << (b)) | ((x) >> (32 - (b))))
#define MUSH(i, b) x = t[i] += (((x ^ seed[i]) + sum) ^ ROTATE(x, b));

static void surf(uint32_t out[8])
{
  uint32_t t[12];
  uint32_t x;
  uint32_t sum = 0;
  int32_t r;
  int32_t i;
  int32_t loop;

  for (i = 0; i < 12; ++i) {
    t[i] = in[i] ^ seed[12 + i];
  }
  for (i = 0; i < 8; ++i) {
    out[i] = seed[24 + i];
  }
  x = t[11];
  for (loop = 0; loop < 2; ++loop) {
    for (r = 0; r < 16; ++r) {
      sum += 0x9e3779b9;
      MUSH(0, 5)
      MUSH(1, 7)
      MUSH(2, 9)
      MUSH(3, 13)
      MUSH(4, 5)
      MUSH(5, 7)
      MUSH(6, 9)
      MUSH(7, 13)
      MUSH(8, 5)
      MUSH(9, 7)
      MUSH(10, 9)
      MUSH(11, 13)
    }
    for (i = 0; i < 8; ++i) {
      out[i] ^= t[i + 4];
    }
  }
}

void randombytes_regen(void);
void randombytes_regen(void)
{
  uint32_t out[8];
  if (!++in[0]) {
    if (!++in[1]) {
      if (!++in[2]) {
        ++in[3];
      }
    }
  }
  surf(out);
  memcpy(out_buf, out, sizeof(out));
  if (!++in[0]) {
    if (!++in[1]) {
      if (!++in[2]) {
        ++in[3];
      }
    }
  }
  surf(out);
  memcpy(out_buf + sizeof(out), out, sizeof(out));
  outleft = sizeof(out_buf);
}

int randombytes(uint8_t* buf, size_t xlen)
{
  /* Perturb seed each call so successive runs differ */
  seed[0] += ++call_counter;
  while (xlen > 0) {
    if (!outleft) {
      randombytes_regen();
    }
    *buf = out_buf[--outleft];
    ++buf;
    --xlen;
  }
  return 0;
}

void randombytes_reset(void)
{
  memcpy(seed, seed_init, sizeof(seed));
  call_counter = 0;
  memset(in, 0, sizeof(in));
  memset(out_buf, 0, sizeof(out_buf));
  outleft = 0;
}
