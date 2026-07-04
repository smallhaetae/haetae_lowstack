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
#ifndef CONST_H
#define CONST_H

#include <stdint.h>
#include "params.h"
#include "fft.h"
#include "rans_byte.h"
#include "platform.h"

/* ---- ntt.c tables ---- */
extern const int32_t zetas[N];
extern const int32_t zeta_len[8];

/* ---- fft.c tables ---- */
extern const complex_fp32_16 roots[256];
extern const uint16_t brv8[256];

/* ---- fips202.c table ---- */
/* Already declared in fips202.h via extern; not duplicated here. */

/* ---- sampler.c table ---- */
#define CDTLEN 64
extern const uint32_t CDT[CDTLEN];

/* ---- encoding.c tables ---- */
#define SCALE_BITS 10
#define SCALE (1u << SCALE_BITS)

#if HAETAE_MODE == 2
#define M_H 13
#define OFFSET_H 239
#define M_HB_Z1 13
#define OFFSET_HB_Z1 6
#elif HAETAE_MODE == 3
#define M_H 17
#define OFFSET_H 235
#define M_HB_Z1 17
#define OFFSET_HB_Z1 8
#elif HAETAE_MODE == 5
#define M_H 33
#define OFFSET_H 471
#define M_HB_Z1 19
#define OFFSET_HB_Z1 9
#endif

extern const RansEncSymbol esyms_h[M_H];
extern const RansDecSymbol dsyms_h[M_H];
extern const uint16_t symbol_h[SCALE];
extern const RansEncSymbol esyms_hb_z1[M_HB_Z1];
extern const RansDecSymbol dsyms_hb_z1[M_HB_Z1];
extern const uint16_t symbol_hb_z1[SCALE];

#endif /* CONST_H */
