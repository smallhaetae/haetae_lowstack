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
 * Scenario: KeyGen + Sign (sender device)
 *
 * 1. KeyGen: generate pk and sk on stack.
 *    pk is sent out (or discarded — sk contains pk).
 *    pk scope ends after keygen, freeing its stack.
 *
 * 2. Sign: using sk on stack, produce sig on stack.
 *    sig is streamed out to the verifier.
 *
 * Peak SRAM = max(sk + pk + keygen_stack, sk + sig + sign_stack)
 */
#include <stdint.h>
#include <string.h>
#include "api.h"
#include "params.h"
#include "test/uart.h"

int main(void)
{
    int ret;

    uart_init();
    uart_puts("=== HAETAE-");
    uart_print_u32(HAETAE_MODE);
    uart_println(" keygen+sign scenario ===");

    /* sk on stack — persists across both phases */
    uint8_t sk[CRYPTO_SECRETKEYBYTES];

    /* ---------- Phase 1: KeyGen ---------- */
    {
        uint8_t pk[CRYPTO_PUBLICKEYBYTES];

        uart_puts("[1] KeyGen ... ");
        ret = crypto_sign_keypair(pk, sk);
        if (ret) { uart_println("FAIL"); goto halt; }
        uart_println("OK");

        /* pk would be sent out here. */
    }
    /* pk stack freed here */

    /* ---------- Phase 2: Sign ---------- */
    {
        uint8_t sig[CRYPTO_BYTES];
        size_t siglen = 0;
        uint8_t msg[] = "test message for HAETAE";

        uart_puts("[2] Sign ... ");
        ret = crypto_sign_signature(sig, &siglen, msg, sizeof(msg), sk);
        if (ret) { uart_println("FAIL"); goto halt; }
        uart_puts("OK (siglen=");
        uart_print_u32(siglen);
        uart_println(")");

        /* sig is streamed out to the verifier here. */
    }

    uart_println("=== All tests passed ===");

halt:
#ifdef __arm__
    while (1);
#endif
    return ret;
}
