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
 * Stack measurement — Scenario 1: KeyGen
 * Budget: sk + pk + keygen_stack
 */
#include <stdint.h>
#include <string.h>
#include "api.h"
#include "params.h"
#include "test/uart.h"

#define STACK_PAINT 0xAA

#ifdef __arm__
extern uint32_t _ebss, _estack;

static void stack_paint(void) {
    volatile uint8_t *p = (volatile uint8_t *)&_ebss;
    volatile uint8_t *sp;
    __asm volatile ("mov %0, sp" : "=r"(sp));
    while (p < sp) *p++ = STACK_PAINT;
}

static uint32_t stack_measure(void) {
    uint8_t *p = (uint8_t *)&_ebss;
    uint8_t *top = (uint8_t *)&_estack;
    while (p < top && *p == STACK_PAINT) p++;
    return (uint32_t)(top - p);
}
#else
static void stack_paint(void) {}
static uint32_t stack_measure(void) { return 0; }
#endif

int main(void)
{
    int ret;
    uart_init();
    uart_puts("=== HAETAE-"); uart_print_u32(HAETAE_MODE);
    uart_println(" stack: KeyGen ===");

    stack_paint();
    {
        uint8_t sk[CRYPTO_SECRETKEYBYTES];
        uint8_t pk[CRYPTO_PUBLICKEYBYTES];

        ret = crypto_sign_keypair(pk, sk);
        if (ret) { uart_println("KeyGen FAIL"); goto halt; }
        uart_println("KeyGen OK");
    }
    uart_puts("Peak: "); uart_print_u32(stack_measure()); uart_println(" bytes");
    uart_println("=== Done ===");

halt:
#ifdef __arm__
    while (1);
#endif
    return 0;
}
