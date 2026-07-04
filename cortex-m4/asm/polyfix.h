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
#ifndef HAETAE_POLYFIX_H
#define HAETAE_POLYFIX_H

#include "params.h"
#include "poly.h"
#include "polyvec.h"
#include "reduce.h"
#include <stdint.h>

/*
 * NOTE: polyfix has the same in-memory layout as `poly`.
 *
 * In the low-SRAM signing path we frequently reuse NTT/polyvec routines on
 * rounded fixed-point vectors. To avoid strict-aliasing undefined behavior,
 * we model polyfix as an alias of `poly` (semantic tag only).
 */

typedef poly polyfix;

#define polyfix_norm2 HAETAE_NAMESPACE(polyfix_norm2)
double polyfix_norm2(const polyfix *a);

int32_t fix_round(int32_t num);

#define polyfix_round HAETAE_NAMESPACE(polyfix_round)
void polyfix_round(poly *a, const polyfix *b);

#define polyfix_add HAETAE_NAMESPACE(polyfix_add)
void polyfix_add(polyfix *c, const polyfix *a, const poly *b);

typedef polyveck polyfixveck;

#define polyfixveck_add HAETAE_NAMESPACE(polyfixveck_add)
void polyfixveck_add(polyfixveck *w, const polyfixveck *u, const polyveck *v);
#define polyfixfixveck_sub HAETAE_NAMESPACE(polyfixfixveck_sub)
void polyfixfixveck_sub(polyfixveck *w, const polyfixveck *u,
                        const polyfixveck *v);
#define polyfixveck_double HAETAE_NAMESPACE(polyfixveck_double)
void polyfixveck_double(polyfixveck *b, const polyfixveck *a);

#define polyfixveck_round HAETAE_NAMESPACE(polyfixveck_round)
void polyfixveck_round(polyveck *a, const polyfixveck *b);

typedef polyvecl polyfixvecl;


/* In-place rounding (safe because polyfix and poly share the same layout).
 * This is used to reduce stack pressure by avoiding an extra rounded copy.
 */
void polyfix_round_inplace(polyfix *a);
void polyfixvecl_round_inplace(polyfixvecl *v);
void polyfixveck_round_inplace(polyfixveck *v);

#define polyfixvecl_add HAETAE_NAMESPACE(polyfixvecl_add)
void polyfixvecl_add(polyfixvecl *w, const polyfixvecl *u, const polyvecl *v);
#define polyfixfixvecl_sub HAETAE_NAMESPACE(polyfixfixvecl_sub)
void polyfixfixvecl_sub(polyfixvecl *w, const polyfixvecl *u,
                        const polyfixvecl *v);
#define polyfixvecl_double HAETAE_NAMESPACE(polyfixvecl_double)
void polyfixvecl_double(polyfixvecl *b, const polyfixvecl *a);

#define polyfixvecl_round HAETAE_NAMESPACE(polyfixvecl_round)
void polyfixvecl_round(polyvecl *a, const polyfixvecl *b);

#define polyfixveclk_sqnorm2 HAETAE_NAMESPACE(polyfixveclk_sqnorm2)
uint64_t polyfixveclk_sqnorm2(const polyfixvecl *a, const polyfixveck *b);

#define polyfixveclk_sample_hyperball HAETAE_NAMESPACE(polyfixveclk_sample_hyperball)
uint16_t polyfixveclk_sample_hyperball(polyfixvecl *y1, polyfixveck *y2, uint8_t *b, const uint8_t seed[CRHBYTES], const uint16_t nonce);

#define polyfixveclk_one_pass_sample_hyperball HAETAE_NAMESPACE(polyfixveclk_one_pass_sample_hyperball)
uint16_t polyfixveclk_one_pass_sample_hyperball(polyfixvecl *y1, polyfixveck *y2, uint8_t *b, const uint8_t seed[CRHBYTES], const uint16_t nonce);

#define polyfixveclk_two_pass_sample_hyperball HAETAE_NAMESPACE(polyfixveclk_two_pass_sample_hyperball)
uint16_t polyfixveclk_two_pass_sample_hyperball(polyfixvecl *y1, polyfixveck *y2, uint8_t *b, const uint8_t seed[CRHBYTES], const uint16_t nonce);

/*
 * Streaming hyperball sampler helpers for full-stream signing.
 *
 * hyperball_pass1_scaling: Pass 1 only -- compute sqnorm of unscaled Gaussian
 *   vector and derive the scaling factor.  The Gaussian samples are NOT stored.
 *   Returns the nonce value after all L+K Gaussian blocks have been consumed.
 *
 * hyperball_regen_single: Regenerate the i-th poly from the same seed/nonce,
 *   apply the previously computed scaling factor, and write to `out`.
 */
#define hyperball_pass1_scaling HAETAE_NAMESPACE(hyperball_pass1_scaling)
uint16_t hyperball_pass1_scaling(fp96_76 *scaling_factor_out,
                                 const uint8_t seed[CRHBYTES],
                                 uint16_t nonce_start);

#define hyperball_regen_single HAETAE_NAMESPACE(hyperball_regen_single)
void hyperball_regen_single(polyfix *out,
                            const uint8_t seed[CRHBYTES],
                            uint16_t nonce_for_this_poly,
                            size_t gauss_len,
                            const fp96_76 *scaling_factor);

#endif
// clang-format on
