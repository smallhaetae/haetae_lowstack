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
#include "reduce.h"
#include "params.h"
#include <stdint.h>

/*************************************************
 * Name:        montgomery_reduce
 *
 * Description: For finite field element a with -2^{31}Q <= a <= Q*2^31,
 *              compute r \equiv a*2^{-32} (mod Q) such that -Q < r < Q.
 *
 * Arguments:   - int64_t: finite field element a
 *
 * Returns r.
 **************************************************/
#if (defined(__arm__) || defined(__thumb__)) && !defined(HAETAE_FORCE_C_MONTGOMERY_REDUCE)
/* Implemented in pointmul_asm.S. */
#else
int32_t montgomery_reduce(int64_t a) {
    int32_t t;

    t = (int64_t)(int32_t)a * QINV;
    t = (a - (int64_t)t * Q) >> 32;
    return t;
}
#endif

/*************************************************
 * Name:        caddq
 *
 * Description: Add Q if input coefficient is negative.
 *
 * Arguments:   - int32_t: finite field element a
 *
 * Returns r.
 **************************************************/
int32_t caddq(int32_t a) {
    a += (a >> 31) & Q;
    return a;
}

/*************************************************
 * Name:        freeze
 *
 * Description: For finite field element a, compute standard
 *              representative r = a mod^+ Q.
 *
 * Arguments:   - int32_t: finite field element a
 *
 * Returns r.
 **************************************************/
int32_t freeze(int32_t a) {
    int64_t t = (int64_t)a * QREC;
    t = t >> 32;
    t = a - t * Q;             // -2Q <  t < 2Q
    t += (t >> 31) & DQ;       //   0 <= t < 2Q
    t -= ~((t - Q) >> 31) & Q; //   0 <= t < Q
    return t;
}

/*************************************************
 * Name:        reduce32_2q
 *
 * Description: compute reduction with 2Q
 *
 * Arguments:   - int32_t: finite field element a
 *
 * Returns r.
 **************************************************/
int32_t reduce32_2q(int32_t a) {
    int64_t t = (int64_t)a * DQREC;
    t >>= 32;
    t = a - t * DQ;              // -4Q <  t < 4Q
    t += (t >> 31) & (DQ * 2);   //   0 <= t < 4Q
    t -= ~((t - DQ) >> 31) & DQ; //   0 <= t < Q
    t -= ~((t - Q) >> 31) & DQ;  // centered representation
    return (int32_t)t;
}

/*************************************************
 * Name:        freeze2q
 *
 * Description: For finite field element a, compute standard
 *              representative r = a mod^+ 2Q.
 *
 * Arguments:   - int32_t: finite field element a
 *
 * Returns r.
 **************************************************/
int32_t freeze2q(int32_t a) {
    int64_t t = (int64_t)a * DQREC;
    t >>= 32;
    t = a - t * DQ;              // -4Q <  t < 4Q
    t += (t >> 31) & (DQ * 2);   //   0 <= t < 4Q
    t -= ~((t - DQ) >> 31) & DQ; //   0 <= t < Q
    return (int32_t)t;
}
