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
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include "sign_passes.h"

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
