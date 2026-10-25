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
 * Fair speed measurement - Sign.
 * Uses the same message every run so the deterministic signing rejection path
 * is identical across implementations and iterations.
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
    uint64_t cycles[NUM_RUNS];
    uint8_t msg[32] = {0};

    uart_init();
    cycle_init();

    memcpy(msg, tv_msg, TV_MLEN < sizeof(msg) ? TV_MLEN : sizeof(msg));

    uart_puts("=== HAETAE-"); uart_print_u32(HAETAE_MODE);
    uart_puts(" speed-fixed: Sign ("); uart_print_u32(NUM_RUNS); uart_println(" runs) ===");

    for (int i = 0; i < NUM_RUNS; i++) {
        uint8_t sig[CRYPTO_BYTES];
        size_t siglen = 0;

        cycle_reset();
        int ret = crypto_sign_signature(sig, &siglen, msg, sizeof(msg), tv_sk);
        cycles[i] = cycle_read();

        if (ret) { uart_println("FAIL"); goto halt; }
        if ((i + 1) % 10 == 0) {
            uart_print_u32(i + 1); uart_puts("/"); uart_print_u32(NUM_RUNS); uart_putchar('\n');
        }
    }

    uart_puts("RAW:");
    for (int i = 0; i < NUM_RUNS; i++) { uart_putchar(' '); uart_print_u64(cycles[i]); }
    uart_putchar('\n');
    sort_u64(cycles, NUM_RUNS);
    uart_puts("Min: "); uart_print_u64(cycles[0]); uart_println(" cycles");
    uart_puts("Median: "); uart_print_u64(cycles[NUM_RUNS / 2]); uart_println(" cycles");
    uart_puts("Max: "); uart_print_u64(cycles[NUM_RUNS - 1]); uart_println(" cycles");
    uart_println("=== Done ===");
halt:
#ifdef __arm__
    while (1);
#endif
    return 0;
}
