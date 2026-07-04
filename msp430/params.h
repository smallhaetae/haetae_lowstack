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
// clang-format off
#ifndef HAETAE_PARAMS_H
#define HAETAE_PARAMS_H

#include "config.h"

#define SEEDBYTES 32
#define CRHBYTES 64
#define N 256
#define ROOT_OF_UNITY 3

#define Q 64513L
#define DQ (Q << 1)// 2Q

#if HAETAE_MODE == 2
#define K 2
#define L 4
#define TAU 58
#define LN 8192 // Large N
#define LNHALF 4096
#define LNBITS 13 
#define D 1
#define CRYPTO_BYTES 1474

/* Fixed integer bounds derived from the original decimal constants.
 * Keep these as integers so 16-bit MSP builds do not depend on float width.
 */
#define B0SQ 96944109ULL
#define B1SQ 96805527ULL
#define B2SQ 163265017ULL
#define GAMMASQ_N 611098LL
#define B0LN_PLUS_HALF_SQNM 80658615ULL

#define BASE_ENC_HB_Z1 132
#define BASE_ENC_H 7

#define ALPHA_HINT 512
#define LOG_ALPHA_HINT 9

#define POLYB1_PACKEDBYTES 480     // 15bits * N / 8bits
#define POLYQ_PACKEDBYTES 480  // 16bits * N / 8bits

#elif HAETAE_MODE == 3
#define K 3
#define L 6
#define TAU 80
#define LN 8192 // Large N
#define LNHALF 4096
#define LNBITS 13 
#define D 1
#define CRYPTO_BYTES 2349

#define B0SQ 335438492ULL
#define B1SQ 335171879ULL
#define B2SQ 479901314ULL
#define GAMMASQ_N 852505LL
#define B0LN_PLUS_HALF_SQNM 150036340ULL

#define BASE_ENC_HB_Z1 376
#define BASE_ENC_H 127

#define ALPHA_HINT 512
#define LOG_ALPHA_HINT 9

#define POLYB1_PACKEDBYTES 480     // 15bits * N / 8bits
#define POLYQ_PACKEDBYTES 480  // 16bits * N / 8bits

#elif HAETAE_MODE == 5
#define K 4
#define L 7
#define TAU 128
#define LN 8192 // Large N
#define LNHALF 4096
#define LNBITS 13 
#define D 0
#define CRYPTO_BYTES 2948

#define B0SQ 499239142ULL
#define B1SQ 498849991ULL
#define B2SQ 597386433ULL
#define GAMMASQ_N 778065LL
#define B0LN_PLUS_HALF_SQNM 183039289ULL

#define BASE_ENC_HB_Z1 501
#define BASE_ENC_H 358

#define ALPHA_HINT 256
#define LOG_ALPHA_HINT 8

#define POLYB1_PACKEDBYTES 512     // 16bits * N / 8bits
#define POLYQ_PACKEDBYTES 512  // 16bits * N / 8bits

#endif // HAETAE_MODE

#define HALF_ALPHA_HINT (ALPHA_HINT >> 1) // ALPHA / 2

#define M (L-1)

#define ETA 1
#define POLYETA_PACKEDBYTES 64
#if D == 1
#define POLY2ETA_PACKEDBYTES 96
#elif D == 0
#define POLY2ETA_PACKEDBYTES 64
#endif

#define POLYC_PACKEDBYTES 32       // 1bit * N / 8bits
#define POLY_HIGHBITS_PACKEDBYTES (N * 9 / 8)
#define POLYVECK_HIGHBITS_PACKEDBYTES (POLY_HIGHBITS_PACKEDBYTES * K)
#define POLYVECK_BYTES (K * N * sizeof(int32_t))
#define POLYVECL_BYTES (L * N * sizeof(int32_t))

#define CRYPTO_PUBLICKEYBYTES (SEEDBYTES + K * POLYQ_PACKEDBYTES)                                      // seed + b
#define CRYPTO_SECRETKEYBYTES (CRYPTO_PUBLICKEYBYTES + M * POLYETA_PACKEDBYTES + K * POLY2ETA_PACKEDBYTES + SEEDBYTES)  // pk + s + K

#endif
// clang-format on
