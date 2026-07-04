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
#include <stdint.h>
#include <string.h>
#include "api.h"
#include "params.h"
#include "platform.h"
#include "testvec.h"
#include "avr_uart.h"

/* ------------------------------------------------------------------ */
/* Main: verify-only test using embedded test vector                   */
/* ------------------------------------------------------------------ */
int main(void)
{
    uart_init();

    PRINTLN("=== HAETAE verify test ===");

    /* PROGMEM sanity: check first bytes of tv_pk and tv_sig */
    PRINT("pk[0..3]=");
    for (int i = 0; i < 4; i++) {
        uint8_t b = READ_BYTE(tv_pk[i]);
        print_u32(b); PRINT(" ");
    }
    PRINTLN("");
    PRINT("sig[0..3]=");
    for (int i = 0; i < 4; i++) {
#ifdef __AVR__
        uint8_t b = pgm_read_byte(&tv_sig[i]);
#else
        uint8_t b = tv_sig[i];
#endif
        print_u32(b); PRINT(" ");
    }
    PRINTLN("");

#ifdef __AVR__
    PRINT("SP="); print_u32(SP); PRINTLN("");
#endif

    /* sig and msg on stack */
    uint8_t sig[TV_SIGLEN];
    uint8_t msg[TV_MLEN];

#ifdef __AVR__
    memcpy_P(sig, tv_sig, TV_SIGLEN);
#else
    memcpy(sig, tv_sig, TV_SIGLEN);
#endif
    memcpy(msg, tv_msg, TV_MLEN);

    /* pk stays in Flash (PROGMEM on AVR) — verify reads it via pgm_read_byte */
    const uint8_t *pk = tv_pk;

    PRINT("[1] Verify (valid) ... ");
    int ret = crypto_sign_verify(sig, TV_SIGLEN, msg, TV_MLEN, pk);
    if (ret) {
        PRINTLN("FAIL");
        return 1;
    }
    PRINTLN("OK");

    /* tamper sig byte 0 and expect rejection */
    sig[0] ^= 0xFF;
    PRINT("[2] Verify (tampered) ... ");
    ret = crypto_sign_verify(sig, TV_SIGLEN, msg, TV_MLEN, pk);
    if (ret == 0) {
        PRINTLN("FAIL (accepted bad sig)");
        return 1;
    }
    PRINTLN("OK (rejected)");

    PRINTLN("=== All tests passed ===");
    return 0;
}
