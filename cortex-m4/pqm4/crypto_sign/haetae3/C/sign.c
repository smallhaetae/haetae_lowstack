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
#include "sign.h"
#include "sign_passes.h"
#include "packing.h"
#include "encoding.h"
#include "params.h"
#include "decompose.h"
#include "poly.h"
#include "polyfix.h"
#include "poly_sparsemul.h"
#include "polymat.h"
#include "polyvec.h"
#include "randombytes.h"
#include "symmetric.h"
#include "stack_profile.h"
#include <inttypes.h>
#include <stdint.h>
#include <string.h>


/* This port ships only the full-streaming signing path (FULL_STREAM_SIGN). */

/*******************************************************************************
 * Name:        crypto_sign_signature
 *
 * Description: Computes signature.
 *
 * Arguments:   - uint8_t *sig:   pointer to output signature (of length
 *                                CRYPTO_BYTES)
 *              - size_t *siglen: pointer to output length of signature
 *              - uint8_t *m:     pointer to message to be signed
 *              - size_t mlen:    length of message
 *              - uint8_t *sk:    pointer to bit-packed secret key
 *
 * Returns 0 (success)
 **************************************************/

/* -------------------------------------------------------------------------
 * Low-stack signing strategy:
 *   - Pass A: sample y, compute c = H(HighBits(A*round(y)), LSB(round(y0)), mu)
 *   - Pass B: re-sample the same y (same counter_start), compute z = y + (-1)^b * LN * (c*s)
 *             using sparse-c multiplication (no NTT), and apply rejection tests.
 *   - Pass C: re-sample y again (same counter_start), recompute Ay = A*round(y) mod 2q,
 *             build the hint using (Ay, z2), and pack the signature.
 *
 * This avoids keeping Ay or the full secret vectors alive across the rejection loop.
 * ------------------------------------------------------------------------- */

#if (HAETAE_MODE == 5)
/* Dense-binary challenge helpers (Mode 5)
 *
 * Mode 5 uses a dense binary challenge c with coefficients in {0,1}. The
 * sparse representation (idx/sign) used for modes 2/3 is not applicable.
 *
 * We therefore multiply c*s in the integer negacyclic ring Z[x]/(x^N+1)
 * (no modular wrap; values remain far below q), which keeps memory low.
 */

void
poly_mul_c_binary(poly *out, const poly *cbin, const poly *s) {
    memset(out->coeffs, 0, sizeof(out->coeffs));
    for (unsigned i = 0; i < N; ++i) {
        if (cbin->coeffs[i] == 0) continue;
        for (unsigned j = 0; j < N; ++j) {
            int k = (int)j - (int)i;
            if (k >= 0) out->coeffs[j] += s->coeffs[k];
            else        out->coeffs[j] -= s->coeffs[k + N];
        }
    }
}

uint64_t
polyfix_addmul_dense_binary_inplace_acc2(polyfix *z,
                                        const poly *s,
                                        const poly *cbin,
                                        int32_t k,
                                        uint64_t acc,
                                        const uint64_t bound) {
    poly cs;
    poly_mul_c_binary(&cs, cbin, s);
    for (unsigned j = 0; j < N; ++j) {
        const int32_t y = z->coeffs[j];
        const int32_t t = (int32_t)((int64_t)k * (int64_t)cs.coeffs[j]);
        const int32_t znew = y + t;
        const int32_t u = y + (int32_t)((uint32_t)t << 1);
        if (acc < bound) {
            uint64_t term2 = (uint64_t)((int64_t)u * (int64_t)u);
            if (term2 >= bound - acc) acc = bound;
            else acc += term2;
        }
        z->coeffs[j] = znew;
    }
    return acc;
}

/* NTT-based variant: s is modified in-place (NTT → pointwise → INTT).
 * c_ntt must already be NTT-transformed.  No extra poly allocation. */
uint64_t
polyfix_addmul_ntt_inplace_acc2(polyfix *z,
                                poly *s,
                                const poly *c_ntt,
                                int32_t k,
                                uint64_t acc,
                                const uint64_t bound) {
    poly_ntt(s);
    poly_pointwise_montgomery(s, c_ntt, s);
    poly_invntt_tomont(s);

    for (unsigned j = 0; j < N; ++j) {
        const int32_t y = z->coeffs[j];
        const int32_t t = (int32_t)((int64_t)k * (int64_t)s->coeffs[j]);
        const int32_t znew = y + t;
        const int32_t u = y + (int32_t)((uint32_t)t << 1);
        if (acc < bound) {
            uint64_t term2 = (uint64_t)((int64_t)u * (int64_t)u);
            if (term2 >= bound - acc) acc = bound;
            else acc += term2;
        }
        z->coeffs[j] = znew;
    }
    return acc;
}
#endif


/* =========================================================================
 * FULL_STREAM_SIGN: BSS-free full-streaming signing implementation.
 *
 * Design:
 *   - Column-streaming Ay accumulator: Ay[K] stays on stack, y1[j] is
 *     regenerated one poly at a time via hyperball_regen_single().
 *   - No static workspace is used.
 *   - Pass A/B/C structure where every poly is regenerated from seed on
 *     demand and discarded immediately after use.
 * ========================================================================= */
#if FULL_STREAM_SIGN

/* ---- full_stream_crypto_sign_signature: main entry point ---- */
static int
full_stream_crypto_sign_signature(uint8_t *sig, size_t *siglen,
                                  const uint8_t *m, size_t mlen,
                                  const uint8_t *sk) {
    /* Derive mu and per-signature seedbuf. */
    uint8_t seedbuf[CRHBYTES] = {0};
    uint8_t key[SEEDBYTES] = {0};
    uint8_t mu[CRHBYTES] = {0};
    xof256_state state;

    xof256_absorbe_twice(&state, sk, CRYPTO_PUBLICKEYBYTES, m, mlen);
    xof256_squeeze(mu, CRHBYTES, &state);

    const uint8_t *pkbytes = sk;
    const uint8_t *skp = sk + CRYPTO_PUBLICKEYBYTES;
    const uint8_t *sk_s1_packed = skp;
    skp += M * POLYETA_PACKEDBYTES;
#if D == 1
    const uint8_t *sk_s2_packed = skp;
    skp += K * POLY2ETA_PACKEDBYTES;
#elif D == 0
    const uint8_t *sk_s2_packed = skp;
    skp += K * POLYETA_PACKEDBYTES;
#else
#error "Not yet implemented."
#endif
    memcpy(key, skp, SEEDBYTES);

    xof256_absorbe_twice(&state, key, SEEDBYTES, mu, CRHBYTES);
    xof256_squeeze(seedbuf, CRHBYTES, &state);

    /* Rejection sampling loop. */
    uint16_t counter = 0;
    poly shared_buf;  /* shared across passA/B/C to avoid redundant poly locals */

    for (;;) {
        uint16_t accepted_start = 0;
        uint8_t b = 0;
        poly c;
        fp96_76 scaling; /* cached: computed once in passA, reused by passB/C */

        fs_passA_compute_challenge(&c, &shared_buf, &b, &accepted_start, &counter,
                                   &scaling,
                                   seedbuf, mu, pkbytes);

        /* Pass B/C use accepted_start (not counter) to regenerate y.
         * counter has advanced past the accepted window for the next
         * outer iteration.  scaling is cached from passA to avoid
         * redundant hyperball_pass1_scaling recomputations. */
        if (fs_passB_check_streaming(&c, &shared_buf, b, seedbuf, accepted_start,
                                     &scaling,
                                     sk_s1_packed, sk_s2_packed)) {
            continue; /* reject */
        }

        if (fs_passC_hint_and_pack(sig, siglen, &shared_buf, &c, b,
                                   seedbuf, accepted_start,
                                   &scaling,
                                   pkbytes, sk_s1_packed, sk_s2_packed)) {
            continue; /* encoding failed => reject */
        }

        return 0;
    }
}

#endif /* FULL_STREAM_SIGN */



/*
 * crypto_sign_signature: public API.
 *   Always uses the full-streaming path.
 */
int crypto_sign_signature(uint8_t *sig, size_t *siglen, const uint8_t *m,
                          size_t mlen, const uint8_t *sk) {
    return full_stream_crypto_sign_signature(sig, siglen, m, mlen, sk);
}



/********************************************************************
 * Name:        crypto_sign
 *
 * Description: Compute signed message.
 *
 * Arguments:   - uint8_t *sm: pointer to output signed message (allocated
 *                             array with CRYPTO_BYTES + mlen bytes),
 *                             can be equal to m
 *              - size_t *smlen: pointer to output length of signed
 *                               message
 *              - const uint8_t *m: pointer to message to be signed
 *              - size_t mlen: length of message
 *              - const uint8_t *sk: pointer to bit-packed secret key
 *
 * Returns 0 (success)
 **************************************************/
int crypto_sign(uint8_t *sm, size_t *smlen, const uint8_t *m, size_t mlen,
                const uint8_t *sk) {
    size_t i;

    for (i = 0; i < mlen; ++i)
        sm[CRYPTO_BYTES + mlen - 1 - i] = m[mlen - 1 - i];
    crypto_sign_signature(sm, smlen, sm + CRYPTO_BYTES, mlen, sk);
    *smlen += mlen;
    return 0;
}
