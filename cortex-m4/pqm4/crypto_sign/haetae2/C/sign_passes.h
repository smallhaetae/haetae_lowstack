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
#ifndef HAETAE_SIGN_PASSES_H
#define HAETAE_SIGN_PASSES_H

#include "config.h"
#include "params.h"
#include "poly.h"
#include "polyfix.h"
#include "fixpoint.h"
#include <stdint.h>
#include <stddef.h>

/* Sparse challenge-multiplication helpers used by the full-streaming
 * passes (sign_passes.c). */
void poly_mul_c_binary(poly *out, const poly *cbin, const poly *s);
uint64_t polyfix_addmul_dense_binary_inplace_acc2(polyfix *z, const poly *s,
                                                  const poly *cbin, int32_t k,
                                                  uint64_t acc,
                                                  const uint64_t bound);
uint64_t polyfix_addmul_ntt_inplace_acc2(polyfix *z, poly *s, const poly *c_ntt,
                                         int32_t k, uint64_t acc,
                                         const uint64_t bound);

#if FULL_STREAM_SIGN
void fs_passA_compute_challenge(poly *c, poly *shared, uint8_t *b_out,
                                uint16_t *accepted_start_out,
                                uint16_t *next_counter_out,
                                fp96_76 *scaling_out,
                                const uint8_t seedbuf[CRHBYTES],
                                const uint8_t mu[CRHBYTES],
                                const uint8_t *pkbytes);

int fs_passB_check_streaming(const poly *c, poly *shared, uint8_t b,
                             const uint8_t seedbuf[CRHBYTES],
                             uint16_t counter_start, const fp96_76 *scaling,
                             const uint8_t *sk_s1_packed,
                             const uint8_t *sk_s2_packed);

int fs_passC_hint_and_pack(uint8_t *sig, size_t *siglen, poly *shared,
                           const poly *c, uint8_t b,
                           const uint8_t seedbuf[CRHBYTES],
                           uint16_t counter_start, const fp96_76 *scaling,
                           const uint8_t *pkbytes, const uint8_t *sk_s1_packed,
                           const uint8_t *sk_s2_packed);
#endif

#endif
