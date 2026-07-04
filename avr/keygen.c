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
#include "keygen.h"
#include "fft.h"

#ifdef __AVR__
/* keygen not used on AVR (verify-only build) */
int crypto_sign_keypair(uint8_t *pk, uint8_t *sk) { (void)pk; (void)sk; return -1; }
#else

static inline uint16_t nonce_a(const size_t row)
{
    return (K << 8) + M + row;
}
static inline uint16_t nonce_A_gen(const size_t row, const size_t column)
{
    return (row << 8) + column;
}
static inline uint16_t nonce_s_gen(const size_t nonce_offset,
                                   const size_t column)
{
    return nonce_offset + column;
}
static inline uint16_t nonce_e_gen(const size_t nonce_offset, const size_t row)
{
    return nonce_offset + M + row;
}

#if FROZEN_A == 1
/* Reusable scratch for streamed A_{row,col} expansion.
 * Keeping it at file scope avoids putting it on the stack.
 * (This code path is single-threaded.)
 */
static poly_frozen g_A_frozen_tmp;
#endif
/* FROZEN_A >= 2: fused sample+multiply eliminates g_A_frozen_tmp entirely. */

static void pointwise_montgomery_by_A_gen_elem_frozen_set(poly *dest,
                                                          const uMatrixPointerM_frozen agenptr, const size_t row,
                                                          const size_t column, const poly *src_hat)
{
#ifdef ENABLE_KEYPAIR_MATRIX_BUFFER
    poly_pointwise_montgomery_mixed(dest, &agenptr.vec[row].vec[column], src_hat);
#else
#if FROZEN_A >= 2
    poly_uniform_frozen_pointwise_montgomery_set(dest, agenptr.seed, nonce_A_gen(row, column), src_hat);
#elif FROZEN_A == 1
    poly_uniform_frozen(&g_A_frozen_tmp, agenptr.seed, nonce_A_gen(row, column));
    poly_pointwise_montgomery_mixed(dest, &g_A_frozen_tmp, src_hat);
#else
    (void)agenptr;
    (void)row;
    (void)column;
    (void)src_hat;
#endif
#endif /* ENABLE_KEYPAIR_MATRIX_BUFFER */
}

static void pointwise_montgomery_by_A_gen_elem_frozen_addto(poly *acc,
                                                            const uMatrixPointerM_frozen agenptr, const size_t row,
                                                            const size_t column, const poly *src_hat)
{
#ifdef ENABLE_KEYPAIR_MATRIX_BUFFER
    poly_pointwise_montgomery_mixed_addto(acc, &agenptr.vec[row].vec[column], src_hat);
#else
#if FROZEN_A >= 2
    poly_uniform_frozen_pointwise_montgomery_addto(acc, agenptr.seed, nonce_A_gen(row, column), src_hat);
#elif FROZEN_A == 1
    poly_uniform_frozen(&g_A_frozen_tmp, agenptr.seed, nonce_A_gen(row, column));
    poly_pointwise_montgomery_mixed_addto(acc, &g_A_frozen_tmp, src_hat);
#else
    (void)agenptr;
    (void)row;
    (void)column;
    (void)src_hat;
#endif
#endif /* ENABLE_KEYPAIR_MATRIX_BUFFER */
}


/* Inlines FFT via caller-frame union, eliminating the
 * sqsing callee frame entirely. Best when sqsing is the deepest call path.
 * s1 is re-sampled after in-place FFT (same nonce → deterministic). */
static inline int HAETAE23_streamed(uint8_t *pk, uint8_t *sk)
{
    uint8_t seedbuf[2 * SEEDBYTES + CRHBYTES] = {0};
    uint16_t counter = 0;
    const uint8_t *rhoprime, *sigma, *key;
    uMatrixPointerM_frozen A_gen_ptr = {.seed = seedbuf};

    poly b;
    union {
        complex_fp32_16 fft_input[FFT_N]; /* FFT scratch */
        poly s;                           /* sampling */
    } u;
    int32_t sum[N];

    randombytes(seedbuf, SEEDBYTES);
    generate_seed_from_one_source(seedbuf, 2 * SEEDBYTES + CRHBYTES, seedbuf, SEEDBYTES);

    rhoprime = seedbuf;
    sigma = rhoprime + SEEDBYTES;
    key = sigma + CRHBYTES;

reject:
    memset(sum, 0, sizeof(int32_t) * N);
    for (size_t row = 0; row < K; row++)
    {
        for (size_t column = 0; column < M; column++)
        {
            poly_uniform_eta(&u.s, sigma, nonce_s_gen(counter, column));
            if (row == 0)
            {
                pack_sk_stream_s1(sk, u.s, column);
                fft_init_and_bitrev_inplace(u.fft_input);
                fft(u.fft_input);
                for (size_t j = 0; j < N; j++)
                    sum[j] += complex_fp_sqabs(u.fft_input[j]);
                poly_uniform_eta(&u.s, sigma, nonce_s_gen(counter, column));
            }
            poly_ntt(&u.s);

            if (column == 0)
                pointwise_montgomery_by_A_gen_elem_frozen_set(&b, A_gen_ptr, row, column, &u.s);
            else
                pointwise_montgomery_by_A_gen_elem_frozen_addto(&b, A_gen_ptr, row, column, &u.s);
        }

        poly_invntt_tomont(&b);
        poly_uniform_eta(&u.s, sigma, nonce_e_gen(counter, row));
        poly_add(&b, &b, &u.s);

        poly_uniform_add(&b, rhoprime, nonce_a(row));
        poly_freeze(&b);

        poly_decompose_stream_pack(pk, &b, row);
        poly_sub(&u.s, &u.s, &b);

        pack_sk_stream_s2(sk, u.s, row);
        fft_init_and_bitrev_inplace(u.fft_input);
        fft(u.fft_input);
        for (size_t j = 0; j < N; j++)
            sum[j] += complex_fp_sqabs(u.fft_input[j]);
    }

    int64_t squared_singular_value = polyvecmk_sqsing_value_min(sum, &u.s);

    counter += M + K;
    if (squared_singular_value > GAMMA * GAMMA * N)
        goto reject;

    pack_pk_stream_remain(pk, rhoprime);
    pack_sk_stream_remain(sk, pk, key);

    return 0;
}

/* Inlines FFT via caller-frame union, eliminating the
 * sqsing callee frame entirely. Pass 1 and Pass 2 share memory via union. */
static inline int HAETAE5_streamed(uint8_t *pk, uint8_t *sk)
{
    uint8_t seedbuf[2 * SEEDBYTES + CRHBYTES] = {0};
    uint16_t counter = 0;
    const uint8_t *rhoprime, *sigma, *key;
    uMatrixPointerM_frozen A_gen_ptr = {.seed = seedbuf};

    union {
        complex_fp32_16 fft_input[FFT_N]; /* Pass 1 FFT scratch */
        poly s;                           /* both passes */
    } u;
    union {
        int32_t sum[N];  /* Pass 1: sv accumulator */
        poly    b;       /* Pass 2: row accumulator */
    } scratch;

    randombytes(seedbuf, SEEDBYTES);
    generate_seed_from_one_source(seedbuf, 2 * SEEDBYTES + CRHBYTES, seedbuf, SEEDBYTES);

    rhoprime = seedbuf;
    sigma = rhoprime + SEEDBYTES;
    key = sigma + CRHBYTES;

reject:
    memset(scratch.sum, 0, sizeof(scratch.sum));
    for (size_t column = 0; column < M; column++)
    {
        poly_uniform_eta(&u.s, sigma, nonce_s_gen(counter, column));
        fft_init_and_bitrev_inplace(u.fft_input);
        fft(u.fft_input);
        for (size_t j = 0; j < N; j++)
            scratch.sum[j] += complex_fp_sqabs(u.fft_input[j]);
    }
    for (size_t row = 0; row < K; row++)
    {
        poly_uniform_eta(&u.s, sigma, nonce_e_gen(counter, row));
        fft_init_and_bitrev_inplace(u.fft_input);
        fft(u.fft_input);
        for (size_t j = 0; j < N; j++)
            scratch.sum[j] += complex_fp_sqabs(u.fft_input[j]);
    }
    counter += M + K;
    if (polyvecmk_sqsing_value_min(scratch.sum, &u.s) > GAMMA * GAMMA * N)
        goto reject;

    counter -= M + K;
    for (size_t row = 0; row < K; row++)
    {
        for (size_t column = 0; column < M; column++)
        {
            poly_uniform_eta(&u.s, sigma, nonce_s_gen(counter, column));
            if (row == 0)
                pack_sk_stream_s1(sk, u.s, column);
            poly_ntt(&u.s);

            if (column == 0)
                pointwise_montgomery_by_A_gen_elem_frozen_set(&scratch.b, A_gen_ptr, row, column, &u.s);
            else
                pointwise_montgomery_by_A_gen_elem_frozen_addto(&scratch.b, A_gen_ptr, row, column, &u.s);
        }

        poly_frommont(&scratch.b);

        poly_uniform_eta(&u.s, sigma, nonce_e_gen(counter, row));
        pack_sk_stream_s2(sk, u.s, row);
        poly_ntt(&u.s);
        poly_add(&scratch.b, &scratch.b, &u.s);

        poly_double_negate(&scratch.b);
        poly_caddq(&scratch.b);

        pack_pk_stream(pk, &scratch.b, row);
    }
    counter += M + K;

    pack_pk_stream_remain(pk, rhoprime);
    pack_sk_stream_remain(sk, pk, key);

    return 0;
}

/*************************************************
 * Name:        crypto_sign_keypair
 *
 * Description: Generates public and private key.
 *
 * Arguments:   - uint8_t *pk: pointer to output public key (allocated
 *                             array of CRYPTO_PUBLICKEYBYTES bytes)
 *              - uint8_t *sk: pointer to output private key (allocated
 *                             array of CRYPTO_SECRETKEYBYTES bytes)
 *
 * Returns 0 (success)
 **************************************************/

int crypto_sign_keypair(uint8_t *pk, uint8_t *sk)
{
#if D > 0
    // return HAETAE-2 and HAETAE-3
    return HAETAE23_streamed(pk, sk);
#else // D == 0 HAETAE-5
    return HAETAE5_streamed(pk, sk);
#endif
}
#endif /* __AVR__ */
