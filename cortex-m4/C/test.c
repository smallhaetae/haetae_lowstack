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
/*
 * Test mode selection:
 *   TEST_MODE 0 = stack profiling + functional test (default)
 *   TEST_MODE 1 = deterministic self-test round-trip (NUM_TESTS iterations)
 *
 * To switch mode, build with: CFLAGS += -DTEST_MODE=1
 */
#ifndef TEST_MODE
#define TEST_MODE 0
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "hal.h"
#include "randombytes.h"
#include "api.h"
#include "config.h"
#include "sign.h"
#include "verify.h"
#include "keygen.h"
#include "stack_profile.h"

static void hal_sendf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    hal_send_str(buf);
}

#if TEST_MODE == 0
/* ===================== Stack profiling test ===================== */

extern uint8_t __stack_bottom__, __stack_top__;

int main(void) {

	sp_scope_t sc = sp_begin("print");

	hal_send_str("====== START ======");

	sp_end(&sc);

	hal_sendf("stack span = %lu", (unsigned long)(&__stack_top__ - &__stack_bottom__));

	uint8_t pk[CRYPTO_PUBLICKEYBYTES];
	uint8_t sk[CRYPTO_SECRETKEYBYTES];

	// Small test message
	uint8_t m[32] = { 0x1, 0x2, 0x3, 0x4 };
	size_t mlen = sizeof m;

	// Signature buffer
	uint8_t sig[CRYPTO_BYTES];
	size_t siglen = 0;

	hal_send_str("=== Testing crypto_sign_keypair_streamed ===\n\n");
	sc = sp_begin("keypair_streamed");
	if (crypto_sign_keypair(pk, sk) != 0) {
		hal_send_str("KeyPair (streamed) FAIL\n");
		return 1;
	}
	sp_end(&sc);
	hal_send_str("KeyPair (streamed) OK!\n\n");

	/* The sign/verify round-trip below uses the streamed keypair generated
	 * above, validating the headline low-stack keygen -> sign -> verify chain. */
	for (int i = 0; i < 1; i++) {

		sp_scope_t sp_sign = sp_begin("sign");
		crypto_sign_signature(sig, &siglen, m, mlen, sk);
		sp_end(&sp_sign);

		sp_scope_t sp_verify = sp_begin("verify");
		int res = crypto_sign_verify(sig, siglen, m, mlen, pk);
		sp_end(&sp_verify);

		if (res != 0) {
			hal_send_str("Signature Fail\n");
			return 1;
		}
		hal_send_str("Signature OK!\n");

		m[0]++;
	}

	hal_send_str("====== END ======");

	return 0;
}

#elif TEST_MODE == 1
/* ============ Deterministic self-test (checksums + round-trip) ============ */

#define NUM_TESTS 100

static uint32_t checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += data[i];
    return sum;
}

static void print_hex8(const char *label, const uint8_t *data)
{
    hal_sendf("%s: %02x%02x%02x%02x%02x%02x%02x%02x",
              label,
              data[0], data[1], data[2], data[3],
              data[4], data[5], data[6], data[7]);
}

int main(void)
{
    uint8_t pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[CRYPTO_SECRETKEYBYTES];
    uint8_t sig[CRYPTO_BYTES];
    size_t siglen;
    uint8_t m[32];

    hal_send_str("====== TESTVEC START ======");
    hal_sendf("MODE=%d", HAETAE_MODE);

    for (int t = 0; t < NUM_TESTS; t++) {
        for (int i = 0; i < 32; i++)
            m[i] = (uint8_t)(t * 37 + i);

        crypto_sign_keypair(pk, sk);

        siglen = 0;
        crypto_sign_signature(sig, &siglen, m, 32, sk);

        int res = crypto_sign_verify(sig, siglen, m, 32, pk);

        hal_sendf("--- iter %d ---", t);
        hal_sendf("pk_sum=%lu", (unsigned long)checksum(pk, CRYPTO_PUBLICKEYBYTES));
        print_hex8("pk", pk);
        hal_sendf("sk_sum=%lu", (unsigned long)checksum(sk, CRYPTO_SECRETKEYBYTES));
        print_hex8("sk", sk);
        hal_sendf("sig_sum=%lu siglen=%u", (unsigned long)checksum(sig, siglen), (unsigned)siglen);
        print_hex8("sig", sig);
        hal_sendf("verify=%d", res);

        if (res != 0) {
            hal_send_str("VERIFY FAIL!");
            return 1;
        }
    }

    hal_send_str("====== TESTVEC END ======");
    return 0;
}

#endif
