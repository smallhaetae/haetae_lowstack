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
#include "verify.h"
#include "reduce.h"
#include "platform.h"

#define PK_BYTE(ptr, i) (ptr)[(i)]

/* Optional debug helpers for investigating signature failures.
 * Enable with -DDEBUG_MODE5_TRACE=1.
 */
#if defined(DEBUG_MODE5_TRACE) && DEBUG_MODE5_TRACE
#include <stdio.h>

static void dbg_hex(const char *label, const uint8_t *x, size_t n)
{
    printf("[DBG] %s (%zu): ", label, n);
    for (size_t i = 0; i < n; i++)
        printf("%02x", x[i]);
    printf("\n");
}

static void dbg_pack_c(uint8_t out[POLYC_PACKEDBYTES], const poly *c)
{
    for (size_t i = 0; i < POLYC_PACKEDBYTES; i++) out[i] = 0;
    for (size_t i = 0; i < N; i++) {
        const uint8_t bit = (uint8_t)(c->coeffs[i] & 1);
        out[i >> 3] |= (uint8_t)(bit << (i & 7));
    }
}
#endif

/* -------------------------- helpers (only used for STREAM_MATRIX && (D==0||D==1)) -------------------------- */
#if STREAM_MATRIX && ((D == 1) || (D == 0))
static inline uint8_t get_bit(const uint8_t *a, size_t i) {
    return (uint8_t)((a[i >> 3] >> (i & 7)) & 1u);
}

static inline void set_bit(uint8_t *a, size_t i, uint8_t b) {
    a[i >> 3] |= (uint8_t)((b & 1u) << (i & 7));
}

/*
 * poly_fromcrt() variant where the CRT "mod 2" component is provided
 * as packed bits (N/8 bytes).
 */
static void poly_fromcrt_bits(poly *w, const poly *u,
                              const uint8_t vbits[POLYC_PACKEDBYTES]) {
    for (size_t i = 0; i < N; i++) {
        int32_t xq = u->coeffs[i];
        int32_t x2 = (int32_t)get_bit(vbits, i);
        w->coeffs[i] = xq + (Q & -((xq ^ x2) & 1));
    }
}

/* Add a packed 0/1 polynomial (bits) into a poly. */
static void poly_add_bits(poly *a, const uint8_t bits[POLYC_PACKEDBYTES]) {
    for (size_t i = 0; i < N; i++) {
        a->coeffs[i] += (int32_t)get_bit(bits, i);
    }
}

/* Per-polynomial helpers to avoid polyveck temporaries in low-SRAM paths. */
static void poly_highbits_hint_one(poly *w, const poly *v) {
    for (size_t j = 0; j < N; j++) {
        decompose_hint(&w->coeffs[j], v->coeffs[j]);
    }
}

static void poly_csubDQ2ALPHA_one(poly *v) {
    for (size_t j = 0; j < N; j++) {
        v->coeffs[j] -= ~((v->coeffs[j] - (DQ - 2) / ALPHA_HINT) >> 31) &
                        ((DQ - 2) / ALPHA_HINT);
    }
}

static void poly_div2_one(poly *v) {
    for (size_t j = 0; j < N; j++) {
        v->coeffs[j] >>= 1;
    }
}

static uint64_t poly_sqnorm2_one(const poly *b) {
    uint64_t ret = 0;
    for (size_t j = 0; j < N; j++) {
        ret += (uint64_t)b->coeffs[j] * (uint64_t)b->coeffs[j];
    }
    return ret;
}
#endif

/*************************************************
 * Name:        crypto_sign_verify
 *
 * Description: Verifies signature.
 *
 * Arguments:   - const uint8_t *sig: pointer to signature
 *              - size_t siglen: length of signature
 *              - const uint8_t *m: pointer to message
 *              - size_t mlen: length of message
 *              - const uint8_t *pk: pointer to bit-packed public key
 *
 * Returns 0 if signature could be verified correctly and -1 otherwise
 **************************************************/
int crypto_sign_verify(const uint8_t *sig, size_t siglen, const uint8_t *m,
                       size_t mlen, const uint8_t *pk) {

    if (siglen != CRYPTO_BYTES) {
        return -1;
    }

#if STREAM_MATRIX && ((D == 1) || (D == 0))
    /*
     * Low-SRAM verification (BRS-style) for D==0/1:
     *   - decode HB(z1) to int8_t
     *   - compose/NTT z1 column-by-column
     *   - keep w' as packed bits
     *   - stream A1 multiplication
     */
    uint8_t wprime_bits[POLYC_PACKEDBYTES] = {0};
    uint8_t rhoprime[SEEDBYTES] = {0};
    xof256_state challenge_xof;  /* incremental transcript hash */

    /* Row-streamed with 2-row batching: decode hb_z1 once per pair of rows. */
    union {
        int8_t  hb_col[N];   /* used in ell loop */
        uint16_t h_row[N];   /* used after ell loop */
    } u;
    poly trow;               /* accumulator for row r; also col0 scratch for D==1 */
    poly trow1;              /* accumulator for row r+1 */
    poly z;                  /* reused: z1 column / cprime / a scratch */

    rans_stream_ctx hb_ctx;  /* streaming rANS for hb_z1 */
    rans_stream_ctx h_ctx;   /* streaming rANS for h */

    uint64_t sqnorm2 = 0;
    xof256_state state;

    /* ---- parse signature (robust) ---- */
    const uint8_t *sp = sig;
    const uint8_t *c_bytes = sp;
    sp += POLYC_PACKEDBYTES;

    const uint8_t *lowbits_ptr = sp;
    sp += (size_t)L * (size_t)N;

    uint16_t size_enc_hb_z1 = (uint16_t)sp[0] + (uint16_t)BASE_ENC_HB_Z1;
    uint16_t size_enc_h = (uint16_t)sp[1] + (uint16_t)BASE_ENC_H;
    sp += 2;

    const size_t used = (size_t)POLYC_PACKEDBYTES + (size_t)L * (size_t)N + 2u +
                        (size_t)size_enc_hb_z1 + (size_t)size_enc_h;
    if (used > CRYPTO_BYTES) {
        return -1;
    }

    const uint8_t *hb_ptr = sp;
    const uint8_t *h_ptr = sp + size_enc_hb_z1;
    const uint8_t *pad_ptr = h_ptr + size_enc_h;

    /* Zero padding check. */
    for (size_t i = 0; i < (CRYPTO_BYTES - used); i++) {
        if (pad_ptr[i] != 0) {
            return -1;
        }
    }

    /* === Unified row-streamed verification for D==0 and D==1 === */

    /* Extract seed from pk. */
    memcpy(rhoprime, pk, SEEDBYTES);

    /*
     * Pre-pass: compute w' bits (only depends on ell==0) and ||z1||^2.
     * Decode hb_z1 column-by-column via streaming rANS.
     */
    if (decode_hb_z1_stream_init(&hb_ctx, hb_ptr, size_enc_hb_z1)) {
        return -1;
    }
    for (size_t ell = 0; ell < L; ell++) {
        if (decode_hb_z1_stream_col_i8(u.hb_col, &hb_ctx)) {
            return -1;
        }
        for (size_t j = 0; j < N; j++) {
            const int32_t hb = (int32_t)u.hb_col[j];
            const int32_t lo = (int32_t)(int8_t)lowbits_ptr[ell * N + j];
            const int32_t zc = (int32_t)((uint32_t)hb << 8) + lo;
            sqnorm2 += (uint64_t)((int64_t)zc * (int64_t)zc);

            if (ell == 0) {
                const uint8_t cbit = get_bit(c_bytes, j);
                const uint8_t wbit = (uint8_t)((zc - (int32_t)cbit) & 1);
                set_bit(wprime_bits, j, wbit);
            }
        }
    }
    if (decode_hb_z1_stream_finish(&hb_ctx)) {
        return -1;
    }

    /*
     * Row-wise streamed multiplication: recompute each output row trow of
     * t = A1 * round(z1) without keeping polyveck temporaries.
     * hb_z1 is re-decoded from the compressed stream for each row.
     */
    if (decode_h_stream_init(&h_ctx, h_ptr, size_enc_h)) {
        return -1;
    }

    uint64_t z2_sqnorm2 = 0;
    shake256_init(&challenge_xof);  /* begin incremental transcript hash */

    /* 2-row batched loop: decode hb_z1 once per pair of rows,
     * accumulate into trow (row r0) and trow1 (row r1) simultaneously. */
    for (size_t rr = 0; rr < K; rr += 2) {
        const size_t r0 = rr;
        const size_t r1 = rr + 1;
        const int have_r1 = (r1 < K);

        /* Re-init hb_z1 streaming ONCE for both rows. */
        if (decode_hb_z1_stream_init(&hb_ctx, hb_ptr, size_enc_hb_z1)) {
            return -1;
        }

#if D == 1
        /* Build col0 for row r0. */
        {
            polyq_unpack(&trow, pk + SEEDBYTES + r0 * (size_t)POLYQ_PACKEDBYTES);
            const uint16_t nonce_a = (uint16_t)((K << 8) + M + (uint16_t)r0);
            poly_uniform(&z, rhoprime, nonce_a);
            for (size_t j = 0; j < N; j++) {
                trow.coeffs[j] = (z.coeffs[j] << 1) - (trow.coeffs[j] << 2);
            }
            poly_ntt(&trow);
        }
        /* Build col0 for row r1. */
        if (have_r1) {
            polyq_unpack(&trow1, pk + SEEDBYTES + r1 * (size_t)POLYQ_PACKEDBYTES);
            const uint16_t nonce_a = (uint16_t)((K << 8) + M + (uint16_t)r1);
            poly_uniform(&z, rhoprime, nonce_a);
            for (size_t j = 0; j < N; j++) {
                trow1.coeffs[j] = (z.coeffs[j] << 1) - (trow1.coeffs[j] << 2);
            }
            poly_ntt(&trow1);
        }
#endif

        for (size_t ell = 0; ell < L; ell++) {
            if (decode_hb_z1_stream_col_i8(u.hb_col, &hb_ctx)) {
                return -1;
            }
            for (size_t j = 0; j < N; j++) {
                const int32_t hb = (int32_t)u.hb_col[j];
                const int32_t lo = (int32_t)(int8_t)lowbits_ptr[ell * N + j];
                z.coeffs[j] = (int32_t)((uint32_t)hb << 8) + lo;
            }

            poly_ntt(&z);

            /* Accumulate into trow (row r0). */
            if (ell == 0) {
#if D == 0
                polyq_unpack_pointwise_montgomery(&trow,
                    pk + SEEDBYTES + r0 * (size_t)POLYQ_PACKEDBYTES, &z);
#elif D == 1
                poly_pointwise_montgomery(&trow, &trow, &z);
#else
#error "Not yet implemented."
#endif
            } else {
                const uint16_t nonce = (uint16_t)((r0 << 8) + (uint16_t)(ell - 1));
#if FROZEN_A >= 2
                poly_uniform_frozen_double_pointwise_montgomery_addto(&trow, rhoprime, nonce, &z);
#elif FROZEN_A == 1
                {
                    poly_frozen af;
                    poly_uniform_frozen(&af, rhoprime, nonce);
                    poly_frozen_double(&af);
                    poly_pointwise_montgomery_mixed_addto(&trow, &af, &z);
                }
#else
                {
                    poly a;
                    poly_uniform(&a, rhoprime, nonce);
                    poly_double(&a);
                    poly_pointwise_montgomery_addto(&trow, &a, &z);
                }
#endif
            }

            /* Accumulate into trow1 (row r1) using same NTT(z). */
            if (have_r1) {
                if (ell == 0) {
#if D == 0
                    polyq_unpack_pointwise_montgomery(&trow1,
                        pk + SEEDBYTES + r1 * (size_t)POLYQ_PACKEDBYTES, &z);
#elif D == 1
                    poly_pointwise_montgomery(&trow1, &trow1, &z);
#else
#error "Not yet implemented."
#endif
                } else {
                    const uint16_t nonce = (uint16_t)((r1 << 8) + (uint16_t)(ell - 1));
#if FROZEN_A >= 2
                    poly_uniform_frozen_double_pointwise_montgomery_addto(&trow1, rhoprime, nonce, &z);
#elif FROZEN_A == 1
                    {
                        poly_frozen af;
                        poly_uniform_frozen(&af, rhoprime, nonce);
                        poly_frozen_double(&af);
                        poly_pointwise_montgomery_mixed_addto(&trow1, &af, &z);
                    }
#else
                    {
                        poly a;
                        poly_uniform(&a, rhoprime, nonce);
                        poly_double(&a);
                        poly_pointwise_montgomery_addto(&trow1, &a, &z);
                    }
#endif
                }
            }
        }

        /* ---- Process row r0 ---- */
        poly_invntt_tomont(&trow);
        if (r0 == 0) {
            poly_fromcrt_bits(&trow, &trow, wprime_bits);
        } else {
            poly_fromcrt0(&trow, &trow);
        }
        poly_freeze2q(&trow);

        if (decode_h_stream_row_u16(u.h_row, &h_ctx)) {
            return -1;
        }
        poly_highbits_hint_one(&z, &trow);
        for (size_t j = 0; j < N; j++) {
            z.coeffs[j] += (int32_t)u.h_row[j];
        }
        poly_csubDQ2ALPHA_one(&z);
        {
            uint8_t row_buf[POLY_HIGHBITS_PACKEDBYTES];
            poly_pack_highbits(row_buf, &z);
            shake256_absorb(&challenge_xof, row_buf, POLY_HIGHBITS_PACKEDBYTES);
        }
        for (size_t j = 0; j < N; j++) {
            trow.coeffs[j] = (int32_t)(ALPHA_HINT * (int64_t)z.coeffs[j]) - trow.coeffs[j];
        }
        if (r0 == 0) {
            poly_add_bits(&trow, wprime_bits);
        }
        poly_reduce2q(&trow);
        poly_div2_one(&trow);
        z2_sqnorm2 += poly_sqnorm2_one(&trow);
        if (sqnorm2 + z2_sqnorm2 > B2SQ) {
            return -1;
        }

        /* ---- Process row r1 (if exists) ---- */
        if (have_r1) {
            poly_invntt_tomont(&trow1);
            poly_fromcrt0(&trow1, &trow1);  /* r1 >= 1 always */
            poly_freeze2q(&trow1);

            if (decode_h_stream_row_u16(u.h_row, &h_ctx)) {
                return -1;
            }
            poly_highbits_hint_one(&z, &trow1);
            for (size_t j = 0; j < N; j++) {
                z.coeffs[j] += (int32_t)u.h_row[j];
            }
            poly_csubDQ2ALPHA_one(&z);
            {
                uint8_t row_buf[POLY_HIGHBITS_PACKEDBYTES];
                poly_pack_highbits(row_buf, &z);
                shake256_absorb(&challenge_xof, row_buf, POLY_HIGHBITS_PACKEDBYTES);
            }
            for (size_t j = 0; j < N; j++) {
                trow1.coeffs[j] = (int32_t)(ALPHA_HINT * (int64_t)z.coeffs[j]) - trow1.coeffs[j];
            }
            poly_reduce2q(&trow1);
            poly_div2_one(&trow1);
            z2_sqnorm2 += poly_sqnorm2_one(&trow1);
            if (sqnorm2 + z2_sqnorm2 > B2SQ) {
                return -1;
            }
        }
    }

    if (decode_h_stream_finish(&h_ctx)) {
        return -1;
    }

    /* Finalize incremental transcript hash: absorb w' bits and mu.
     * Recompute c' — reuse rhoprime for mu (no longer needed as seed). */
    xof256_absorbe_twice(&state, pk, CRYPTO_PUBLICKEYBYTES, m, mlen);
    xof256_squeeze(rhoprime, SEEDBYTES, &state);

    shake256_absorb(&challenge_xof, wprime_bits, POLYC_PACKEDBYTES);
    shake256_absorb(&challenge_xof, rhoprime, SEEDBYTES);
    shake256_finalize(&challenge_xof);
    poly_challenge_from_xof(&z, &challenge_xof);

    for (size_t j = 0; j < N; j++) {
        if (get_bit(c_bytes, j) != (uint8_t)z.coeffs[j]) {
            return -1;
        }
    }

    return 0;

#else
    /* Fallback reference path (kept for completeness). */
    unsigned int i;
    uint8_t buf[POLYVECK_HIGHBITS_PACKEDBYTES + POLYC_PACKEDBYTES] = {0};
    uint8_t rhoprime[SEEDBYTES] = {0}, mu[SEEDBYTES];
    uint64_t sqnorm2;
#if !STREAM_MATRIX
    polyvecl A1[K];
#endif
    polyvecl lowbits_z1, z1;
    polyveck b, highbits, h, z2, w;
#if D > 0
    polyveck a;
#endif
    poly c, cprime, wprime;

    xof256_state state;

    /* Unpack public key */
    unpack_pk(&b, rhoprime, pk);

    /* Unpack signature */
    if (unpack_sig(&c, &lowbits_z1, &z1, &h, sig)) {
        return -1;
    }

    /* Compose z1 out of HighBits(z1) and LowBits(z1) */
    for (i = 0; i < L; ++i) {
        poly_compose(&z1.vec[i], &z1.vec[i], &lowbits_z1.vec[i]);
    }

    /* Recover A1 */
#if !STREAM_MATRIX
    polymatkl_expand(A1, rhoprime);
    polymatkl_double(A1);
#if D == 1
    polyveck_expand(&a, rhoprime);
    polyveck_double(&b);
    polyveck_sub(&b, &a, &b);
    polyveck_double(&b);
    polyveck_ntt(&b);
#elif D == 0
    /* pk already stores the NTT-domain column */
#else
#error "Not yet implemented."
#endif
    for (i = 0; i < K; ++i) {
        A1[i].vec[0] = b.vec[i];
    }
#endif

    /* Compute z2 */
    sqnorm2 = polyvecl_sqnorm2(&z1);
    poly_sub(&wprime, &z1.vec[0], &c);
    poly_lsb(&wprime, &wprime);

    polyvecl_ntt(&z1);
#if STREAM_MATRIX
    {
        polyveck b1 = b;
#if D > 0
        polyveck_expand(&a, rhoprime);
#endif
#if D == 1
        polyveck_double(&b1);
        polyveck_sub(&b1, &a, &b1);
        polyveck_double(&b1);
        polyveck_ntt(&b1);
#elif D == 0
        /* pk already stores the NTT-domain column */
#else
#error "Not yet implemented."
#endif
        polymatkl_pointwise_montgomery_stream(&highbits, rhoprime, &b1, &z1);
    }
#else
    polymatkl_pointwise_montgomery(&highbits, A1, &z1);
#endif
    polyveck_invntt_tomont(&highbits);

    polyveck_poly_fromcrt(&highbits, &highbits, &wprime);
    polyveck_freeze2q(&highbits);

    polyveck_highbits_hint(&w, &highbits);
    polyveck_add(&w, &w, &h);
    polyveck_csubDQ2ALPHA(&w);

    polyveck_mul_alpha(&z2, &w);
    polyveck_sub(&z2, &z2, &highbits);
    poly_add(&z2.vec[0], &z2.vec[0], &wprime);
    polyveck_reduce2q(&z2);
    polyveck_div2(&z2);

    if (sqnorm2 + polyveck_sqnorm2(&z2) > B2SQ) {
        return -1;
    }

    polyveck_pack_highbits(buf, &w);
    poly_pack_lsb(buf + POLYVECK_HIGHBITS_PACKEDBYTES, &wprime);

    xof256_absorbe_twice(&state, pk, CRYPTO_PUBLICKEYBYTES, m, mlen);
    xof256_squeeze(mu, SEEDBYTES, &state);

    poly_challenge(&cprime, buf, mu);

    for (i = 0; i < N; ++i) {
        if (c.coeffs[i] != cprime.coeffs[i]) {
#if defined(DEBUG_MODE5_TRACE) && DEBUG_MODE5_TRACE
            uint8_t c_bytes[POLYC_PACKEDBYTES];
            uint8_t cp_bytes[POLYC_PACKEDBYTES];
            dbg_pack_c(c_bytes, &c);
            dbg_pack_c(cp_bytes, &cprime);
            dbg_hex("mu", mu, 32);
            dbg_hex("c", c_bytes, POLYC_PACKEDBYTES);
            dbg_hex("c'", cp_bytes, POLYC_PACKEDBYTES);
            dbg_hex("buf[0..31]", buf, 32);
            dbg_hex("buf[last 32]", buf + POLYVECK_HIGHBITS_PACKEDBYTES, 32);
#endif
            return -1;
        }
    }
    return 0;
#endif
}

/*************************************************
 * Name:        crypto_sign_open
 *
 * Description: Verify signed message.
 *
 * Returns 0 if signed message could be verified correctly and -1 otherwise
 **************************************************/
int crypto_sign_open(uint8_t *m, size_t *mlen, const uint8_t *sm, size_t smlen,
                     const uint8_t *pk) {
    size_t i;

    if (smlen < CRYPTO_BYTES)
        goto badsig;

    *mlen = smlen - CRYPTO_BYTES;
    if (crypto_sign_verify(sm, CRYPTO_BYTES, sm + CRYPTO_BYTES, *mlen, pk))
        goto badsig;
    else {
        /* All good, copy msg, return 0 */
        for (i = 0; i < *mlen; ++i)
            m[i] = sm[CRYPTO_BYTES + i];
        return 0;
    }

badsig:
    /* Signature verification failed */
    *mlen = (size_t)-1;
    for (i = 0; i < smlen; ++i)
        m[i] = 0;

    return -1;
}
