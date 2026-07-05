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
 * Scenario: Verify (receiver device)
 *
 * The verifier receives pk and sig over a communication channel.
 * Both are placed on the stack, then verification is performed.
 *
 * Peak SRAM budget: pk + sig + verify_stack
 *
 * In this test, pk and sig are loaded from testvec (simulating
 * reception from the sender).
 */
#include <stdint.h>
#include <string.h>
#include "api.h"
#include "params.h"
#include "testvec.h"
#include "test/uart.h"

int main(void)
{
    int ret;

    uart_init();
    uart_puts("=== HAETAE-");
    uart_print_u32(HAETAE_MODE);
    uart_println(" verify scenario ===");

    /* ---------- Receive pk and sig onto stack ---------- */
    /* In deployment: received over UART/SPI/etc. */
    uint8_t pk[TV_PKLEN];
    uint8_t sig[TV_SIGLEN];

    memcpy(pk, tv_pk, TV_PKLEN);
    memcpy(sig, tv_sig, TV_SIGLEN);

    /* ---------- Verify ---------- */
    uart_puts("[1] Verify (valid) ... ");
    ret = crypto_sign_verify(sig, TV_SIGLEN, tv_msg, TV_MLEN, pk);
    if (ret) { uart_println("FAIL"); goto halt; }
    uart_println("OK");

    /* Tamper test */
    sig[0] ^= 0xFF;
    uart_puts("[2] Verify (tampered) ... ");
    ret = crypto_sign_verify(sig, TV_SIGLEN, tv_msg, TV_MLEN, pk);
    if (ret == 0) { uart_println("FAIL (accepted bad sig)"); ret = 1; goto halt; }
    uart_println("OK (rejected)");

    ret = 0;
    uart_println("=== All tests passed ===");

halt:
#ifdef __arm__
    while (1);
#endif
    return ret;
}
