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
#include "polyfix.h"
#include "decompose.h"
#include "math.h"
#include "ntt.h"
#include "params.h"
#include "reduce.h"
#include "sampler.h"
#include "symmetric.h"
#include <stdint.h>

/*************************************************
 * Name:        polyfix_add
 *
 * Description: Add double polynomial and integer polynomial.
 *              No modular reduction is performed.
 *
 * Arguments:   - polyfix *c: pointer to output double polynomial
 *              - const polyfix *a: pointer to first summand
 *              - const poly *b: pointer to second summand
 **************************************************/
void polyfix_add(polyfix *c, const polyfix *a, const poly *b) {
    unsigned int i;

    for (i = 0; i < N; ++i)
        c->coeffs[i] = a->coeffs[i] + LN * b->coeffs[i];
}

/*************************************************
 * Name:        polyfixfix_sub
 *
 * Description: Subtract fixed polynomial and fixed polynomial.
 *              No modular reduction is performed.
 *
 * Arguments:   - polyfix *c: pointer to output fixed polynomial
 *              - const polyfix *a: pointer to first summand
 *              - const polyfix *b: pointer to second summand
 **************************************************/
void polyfixfix_sub(polyfix *c, const polyfix *a, const polyfix *b) {
    unsigned int i;

    for (i = 0; i < N; ++i)
        c->coeffs[i] = a->coeffs[i] - b->coeffs[i];
}

int32_t fix_round(int32_t num) {
    return (num + LNHALF) >> LNBITS;
} 

/*************************************************
 * Name:        polyfix_round
 *
 * Description: rounds a fixed polynomial to integer polynomial
 *
 * Arguments:   - poly *a: output integer polynomial
 *              - poly *b: input fixed polynomial
 **************************************************/
void polyfix_round(poly *a, const polyfix *b) {
    unsigned i;

    for (i = 0; i < N; ++i)
        a->coeffs[i] = fix_round(b->coeffs[i]);
}

void polyfix_round_inplace(polyfix *a) {
    unsigned i;
    for (i = 0; i < N; ++i) {
        a->coeffs[i] = fix_round(a->coeffs[i]);
    }
}

void polyfixvecl_round_inplace(polyfixvecl *v) {
    unsigned i;
    for (i = 0; i < L; ++i) {
        polyfix_round_inplace(&v->vec[i]);
    }
}

void polyfixveck_round_inplace(polyfixveck *v) {
    unsigned i;
    for (i = 0; i < K; ++i) {
        polyfix_round_inplace(&v->vec[i]);
    }
}

/**************************************************************/
/********* Double Vectors of polynomials of length K **********/
/**************************************************************/

/*************************************************
 * Name:        polyfixveck_add
 *
 * Description: Add vector to a vector of double polynomials of length K.
 *              No modular reduction is performed.
 *
 * Arguments:   - polyveck *w: pointer to output vector
 *              - const polyveck *u: pointer to first summand
 *              - const polyveck *v: pointer to second summand
 **************************************************/
void polyfixveck_add(polyfixveck *w, const polyfixveck *u, const polyveck *v) {
    unsigned int i;

    for (i = 0; i < K; ++i)
        polyfix_add(&w->vec[i], &u->vec[i], &v->vec[i]);
}

/*************************************************
 * Name:        polyfixfixveck_sub
 *
 * Description: subtract vector to a vector of fixed polynomials of length k.
 *              No modular reduction is performed.
 *
 * Arguments:   - polyveck *w: pointer to output vector
 *              - const polyfixveck *u: pointer to first summand
 *              - const polyfixveck *v: pointer to second summand
 **************************************************/
void polyfixfixveck_sub(polyfixveck *w, const polyfixveck *u,
                        const polyfixveck *v) {
    unsigned int i;

    for (i = 0; i < K; ++i)
        polyfixfix_sub(&w->vec[i], &u->vec[i], &v->vec[i]);
}

/*************************************************
 * Name:        polyfixveck_double
 *
 * Description: Double vector of polynomials of length K.
 *
 * Arguments:   - polyveck *b: pointer to output vector
 *              - polyveck *a: pointer to input vector
 **************************************************/
void polyfixveck_double(polyfixveck *b, const polyfixveck *a) {
    unsigned int i, j;

    for (i = 0; i < K; ++i)
        for (j = 0; j < N; ++j)
            b->vec[i].coeffs[j] = 2 * a->vec[i].coeffs[j];
}

/*************************************************
 * Name:        polyfixveck_round
 *
 * Description: rounds a fixed polynomial vector of length K
 *
 * Arguments:   - polyveck *a: output integer polynomial vector
 *              - polyfixveck *b: input fixed polynomial vector
 **************************************************/
void polyfixveck_round(polyveck *a, const polyfixveck *b) {
    unsigned i;

    for (i = 0; i < K; ++i)
        polyfix_round(&a->vec[i], &b->vec[i]);
}

/**************************************************************/
/********* Double Vectors of polynomials of length L **********/
/**************************************************************/

/*************************************************
 * Name:        polyfixvecl_add
 *
 * Description: Add vector to a vector of double polynomials of length L.
 *              No modular reduction is performed.
 *
 * Arguments:   - polyvecl *w: pointer to output vector
 *              - const polyfixvecl *u: pointer to first summand
 *              - const polyvecl *v: pointer to second summand
 **************************************************/
void polyfixvecl_add(polyfixvecl *w, const polyfixvecl *u, const polyvecl *v) {
    unsigned int i;

    for (i = 0; i < L; ++i)
        polyfix_add(&w->vec[i], &u->vec[i], &v->vec[i]);
}

/*************************************************
 * Name:        polyfixfixvecl_sub
 *
 * Description: subtract vector to a vector of fixed polynomials of length l.
 *              No modular reduction is performed.
 *
 * Arguments:   - polyvecl *w: pointer to output vector
 *              - const polyfixvecl *u: pointer to first summand
 *              - const polyfixvecl *v: pointer to second summand
 **************************************************/
void polyfixfixvecl_sub(polyfixvecl *w, const polyfixvecl *u,
                        const polyfixvecl *v) {
    unsigned int i;

    for (i = 0; i < L; ++i)
        polyfixfix_sub(&w->vec[i], &u->vec[i], &v->vec[i]);
}
/*************************************************
 * Name:        polyfixvecl_double
 *
 * Description: Double vector of polynomials of length L.
 *
 * Arguments:   - polyveck *b: pointer to output vector
 *              - polyveck *a: pointer to input vector
 **************************************************/
void polyfixvecl_double(polyfixvecl *b, const polyfixvecl *a) {
    unsigned int i, j;

    for (i = 0; i < L; ++i)
        for (j = 0; j < N; ++j)
            b->vec[i].coeffs[j] = 2 * a->vec[i].coeffs[j];
}

/*************************************************
 * Name:        polyfixvecl_round
 *
 * Description: rounds a fixed polynomial vector of length L
 *
 * Arguments:   - polyvecl *a: output integer polynomial vector
 *              - polyfixvecl *b: input fixed polynomial vector
 **************************************************/
void polyfixvecl_round(polyvecl *a, const polyfixvecl *b) {
    unsigned i;

    for (i = 0; i < L; ++i)
        polyfix_round(&a->vec[i], &b->vec[i]);
}

/*************************************************
 * Name:        polyfixveclk_norm2
 *
 * Description: Calculates L2 norm of a fixed point polynomial vector with
 *length L + K The result is L2 norm * LN similar to the way polynomial is
 *usually stored
 *
 * Arguments:   - polyfixvecl *a: polynomial vector with length L to calculate
 *                norm
 *              - polyfixveck *a: polynomial vector with length K to calculate
 *                norm
 **************************************************/
uint64_t polyfixveclk_sqnorm2(const polyfixvecl *a, const polyfixveck *b) {
    unsigned int i, j;
    uint64_t ret = 0;

    for (i = 0; i < L; ++i) {
        for (j = 0; j < N; ++j)
            ret += (int64_t)a->vec[i].coeffs[j] * a->vec[i].coeffs[j];
    }

    for (i = 0; i < K; ++i) {
        for (j = 0; j < N; ++j)
            ret += (int64_t)b->vec[i].coeffs[j] * b->vec[i].coeffs[j];
    }

    return ret;
}

uint16_t polyfixveclk_two_pass_sample_hyperball(polyfixvecl *y1, polyfixveck *y2, uint8_t *b,
                                       const uint8_t seed[CRHBYTES],
                                       const uint16_t nonce) {
    uint16_t ni = nonce;
    fp96_76 sqsum, invsqrt;
    unsigned int i, j;

    /*
     * Low-stack two-pass hyperball sampler (streaming, zero BSS).
     *
     * Pass 1: stream Gaussian samples one-at-a-time, accumulate sqsum only.
     * Pass 2: regenerate the same stream, scale and write to (y1,y2).
     */

    gauss_stream_ctx gctx;

    do {
        const uint16_t start_nonce = ni;
        uint16_t end_nonce = ni;

        /* ---- pass 1: norm-only (discard samples) ---- */
        sqsum.limb48[0] = 0;
        sqsum.limb48[1] = 0;

        for (i = 0; i < L + K; i++) {
            const size_t len = (i < 2) ? (N + 1) : N;
            gauss_stream_init(&gctx, seed, end_nonce++, len);
            for (j = 0; j < len; j++) {
                uint64_t samp;
                fp96_76 sqr;
                gauss_stream_sample(&gctx, &samp, &sqr);
                sqsum.limb48[0] += sqr.limb48[0];
                sqsum.limb48[1] += sqr.limb48[1];
            }
        }
        renormalize(&sqsum);

        /* Compute scaling factor (same arithmetic as the baseline sampler). */
        sqsum.limb48[0] += 1; /* rounding */
        sqsum.limb48[0] >>= 1;
        sqsum.limb48[0] += (sqsum.limb48[1] & 1) << 47;
        sqsum.limb48[1] >>= 1;
        sqsum.limb48[1] += sqsum.limb48[0] >> 48;
        sqsum.limb48[0] &= (1ULL << 48) - 1;
        fixpoint_newton_invsqrt(&invsqrt, &sqsum);
        fixpoint_mul_high(&sqsum, &invsqrt,
                          (uint64_t)(B0 * LN + SQNM / 2) << (28 - 13));

        /* ---- pass 2: regenerate and write scaled coefficients ---- */
        {
            uint16_t regen_nonce = start_nonce;

            for (i = 0; i < L + K; i++) {
                const size_t len = (i < 2) ? (N + 1) : N;
                gauss_stream_init(&gctx, seed, regen_nonce++, len);

                for (j = 0; j < N; j++) {
                    uint64_t samp;
                    fp96_76 sqr;
                    gauss_stream_sample(&gctx, &samp, &sqr);
                    uint8_t sign = (gctx.signs[j / 8] >> (j % 8)) & 1;
                    if (i < L) {
                        y1->vec[i].coeffs[j] = fixpoint_mul_rnd13(samp, &sqsum, sign);
                    } else {
                        y2->vec[i - L].coeffs[j] = fixpoint_mul_rnd13(samp, &sqsum, sign);
                    }
                }

                /* If len == N+1, consume the (N+1)-th sample (sqsum-only). */
                if (len > N) {
                    uint64_t samp;
                    fp96_76 sqr;
                    gauss_stream_sample(&gctx, &samp, &sqr);
                }
            }
        }

        /* Advance nonce once per attempt (pass 2 is regeneration). */
        ni = end_nonce;

    } while (polyfixveclk_sqnorm2(y1, y2) > B0SQ * LN * LN);

    {
        uint8_t tmp[CRHBYTES + 2];
        for (i = 0; i < CRHBYTES; i++) {
            tmp[i] = seed[i];
        }
        tmp[CRHBYTES + 0] = ni >> 0;
        tmp[CRHBYTES + 1] = ni >> 8;
        shake256(b, 1, tmp, CRHBYTES + 2);
    }

    return ni;
}


/*
 * Streaming hyperball helpers for full-stream signing.
 */

uint16_t hyperball_pass1_scaling(fp96_76 *scaling_factor_out,
                                 const uint8_t seed[CRHBYTES],
                                 uint16_t nonce_start) {
    fp96_76 sqsum;
    unsigned int i, j;
    uint16_t ni = nonce_start;
    gauss_stream_ctx gctx;

    sqsum.limb48[0] = 0;
    sqsum.limb48[1] = 0;

    /* Stream Gaussian samples one-at-a-time, accumulate sqsum only. */
    for (i = 0; i < L + K; i++) {
        const size_t len = (i < 2) ? (N + 1) : N;
        gauss_stream_init(&gctx, seed, ni++, len);
        for (j = 0; j < len; j++) {
            uint64_t samp;
            fp96_76 sqr;
            gauss_stream_sample(&gctx, &samp, &sqr);
            sqsum.limb48[0] += sqr.limb48[0];
            sqsum.limb48[1] += sqr.limb48[1];
        }
    }
    renormalize(&sqsum);

    /* Compute scaling factor (same arithmetic as the baseline sampler). */
    sqsum.limb48[0] += 1; /* rounding */
    sqsum.limb48[0] >>= 1;
    sqsum.limb48[0] += (sqsum.limb48[1] & 1) << 47;
    sqsum.limb48[1] >>= 1;
    sqsum.limb48[1] += sqsum.limb48[0] >> 48;
    sqsum.limb48[0] &= (1ULL << 48) - 1;

    fp96_76 invsqrt;
    fixpoint_newton_invsqrt(&invsqrt, &sqsum);
    fixpoint_mul_high(&sqsum, &invsqrt,
                      (uint64_t)(B0 * LN + SQNM / 2) << (28 - 13));

    *scaling_factor_out = sqsum;
    return ni; /* nonce after all L+K blocks */
}

void hyperball_regen_single(polyfix *out,
                            const uint8_t seed[CRHBYTES],
                            uint16_t nonce_for_this_poly,
                            size_t gauss_len,
                            const fp96_76 *scaling_factor) {
    gauss_stream_ctx gctx;
    unsigned int j;

    gauss_stream_init(&gctx, seed, nonce_for_this_poly, gauss_len);

    for (j = 0; j < N; j++) {
        uint64_t samp;
        fp96_76 sqr;
        gauss_stream_sample(&gctx, &samp, &sqr);
        uint8_t sign = (gctx.signs[j / 8] >> (j % 8)) & 1;
        out->coeffs[j] = fixpoint_mul_rnd13(samp, scaling_factor, sign);
    }

    /* If gauss_len == N+1, consume the (N+1)-th sample (discard). */
    if (gauss_len > N) {
        uint64_t samp;
        fp96_76 sqr;
        gauss_stream_sample(&gctx, &samp, &sqr);
    }
}

uint16_t polyfixveclk_one_pass_sample_hyperball(polyfixvecl *y1, polyfixveck *y2, uint8_t *b,
                                       const uint8_t seed[CRHBYTES],
                                       const uint16_t nonce) {
    uint16_t ni = nonce;
    uint64_t samples[N * (L + K)];
    fp96_76 sqsum, invsqrt;
    unsigned int i, j;
    uint8_t signs[N * (L + K) / 8];

    do {
        sqsum.limb48[0] = 0;
        sqsum.limb48[1] = 0;
        
        sample_gauss_N(&samples[0], &signs[0], &sqsum, seed, ni++, N + 1);
        sample_gauss_N(&samples[N], &signs[N / 8], &sqsum, seed, ni++, N + 1);

        for (i = 2; i < L + K; i++)
            sample_gauss_N(&samples[N * i], &signs[N / 8 * i], &sqsum, seed,
                           ni++, N);

        // divide sqsum by 2 and approximate inverse square root
        sqsum.limb48[0] += 1; // rounding
        sqsum.limb48[0] >>= 1;
        sqsum.limb48[0] += (sqsum.limb48[1] & 1) << 47;
        sqsum.limb48[1] >>= 1;
        sqsum.limb48[1] += sqsum.limb48[0] >> 48;
        sqsum.limb48[0] &= (1ULL << 48) - 1;
        fixpoint_newton_invsqrt(&invsqrt, &sqsum);
        fixpoint_mul_high(&sqsum, &invsqrt,
                          (uint64_t)(B0 * LN + SQNM / 2) << (28 - 13));

        for (i = 0; i < L; i++) {
            for (j = 0; j < N; j++)
                y1->vec[i].coeffs[j] = fixpoint_mul_rnd13(
                    samples[(i * N + j)], &sqsum,
                    (signs[(i * N + j) / 8] >> ((i * N + j) % 8)) & 1);
        }
        for (i = L; i < K + L; i++) {
            for (j = 0; j < N; j++)
                y2->vec[i - L].coeffs[j] = fixpoint_mul_rnd13(
                    samples[(i * N + j)], &sqsum,
                    (signs[(i * N + j) / 8] >> ((i * N + j) % 8)) & 1);
        }
    } while (polyfixveclk_sqnorm2(y1, y2) > B0SQ * LN * LN);

    {
      uint8_t tmp[CRHBYTES + 2];
      for (i = 0; i < CRHBYTES; i++)
      {
        tmp[i] = seed[i];
      }
      tmp[CRHBYTES + 0] = ni >> 0;
      tmp[CRHBYTES + 1] = ni >> 8;
      shake256(b, 1, tmp, CRHBYTES+2);
    }

    return ni;
}


uint16_t polyfixveclk_sample_hyperball(polyfixvecl *y1, polyfixveck *y2, uint8_t *b,
                                       const uint8_t seed[CRHBYTES],
                                       const uint16_t nonce) {
#if SAMPLER == 1
    return polyfixveclk_one_pass_sample_hyperball(y1, y2, b, seed, nonce);
#else
    return polyfixveclk_two_pass_sample_hyperball(y1, y2, b, seed, nonce);
#endif
}
