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
 * Fair speed measurement - Verify.
 * Verify is deterministic for a fixed pk/signature/message tuple.
 */
#include <stdint.h>
#include <string.h>
#include "api.h"
#include "params.h"
#include "testvec.h"
#include "test/uart.h"
#include "test/cycle.h"

#define NUM_RUNS 100

int main(void)
{
    uint32_t cycles[NUM_RUNS];
    uint8_t pk[TV_PKLEN];
    uint8_t sig[TV_SIGLEN];

    uart_init();
    cycle_init();

    memcpy(pk, tv_pk, TV_PKLEN);
    memcpy(sig, tv_sig, TV_SIGLEN);

    uart_puts("=== HAETAE-"); uart_print_u32(HAETAE_MODE);
    uart_puts(" speed-fixed: Verify ("); uart_print_u32(NUM_RUNS); uart_println(" runs) ===");

    for (int i = 0; i < NUM_RUNS; i++) {
        cycle_reset();
        int ret = crypto_sign_verify(sig, TV_SIGLEN, tv_msg, TV_MLEN, pk);
        cycles[i] = cycle_read();

        if (ret) { uart_println("FAIL"); goto halt; }
        if ((i + 1) % 10 == 0) {
            uart_print_u32(i + 1); uart_puts("/"); uart_print_u32(NUM_RUNS); uart_putchar('\n');
        }
    }

    sort_u32(cycles, NUM_RUNS);
    uart_puts("Min: "); uart_print_u32(cycles[0]); uart_println(" cycles");
    uart_puts("Median: "); uart_print_u32(cycles[NUM_RUNS / 2]); uart_println(" cycles");
    uart_puts("Max: "); uart_print_u32(cycles[NUM_RUNS - 1]); uart_println(" cycles");
    uart_println("=== Done ===");
halt:
#ifdef __arm__
    while (1);
#endif
    return 0;
}
