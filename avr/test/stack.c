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
/* Stack painting                                                      */
/* ------------------------------------------------------------------ */
#define STACK_PAINT 0xAA

#ifdef __AVR__

extern uint8_t __heap_start;

static void stack_paint(void) {
    volatile uint8_t *p = &__heap_start;
    volatile uint8_t *sp = (volatile uint8_t *)(SP);
    while (p < sp) {
        *p = STACK_PAINT;
        p++;
    }
}

static uint16_t stack_measure(void) {
    uint8_t *p = &__heap_start;
    uint8_t *ramend = (uint8_t *)(RAMEND);
    while (p < ramend && *p == STACK_PAINT)
        p++;
    return (uint16_t)(ramend - p + 1);
}

#else

static void stack_paint(void) { }
static uint16_t stack_measure(void) { return 0; }

#endif

/* ------------------------------------------------------------------ */
/* Main: stack measurement for verify                                  */
/* ------------------------------------------------------------------ */
int main(void)
{
    uart_init();

    PRINTLN("=== HAETAE stack measurement ===");
    PRINT("HAETAE-"); print_u32(HAETAE_MODE); PRINTLN("");

    uint8_t sig[TV_SIGLEN];
    uint8_t msg[TV_MLEN];

#ifdef __AVR__
    memcpy_P(sig, tv_sig, TV_SIGLEN);
#else
    memcpy(sig, tv_sig, TV_SIGLEN);
#endif
    memcpy(msg, tv_msg, TV_MLEN);
    const uint8_t *pk = tv_pk;

#ifdef __AVR__
    PRINT("SP before paint: "); print_u32(SP); PRINTLN("");
#endif

    stack_paint();

    PRINT("Running verify ... ");
    int ret = crypto_sign_verify(sig, TV_SIGLEN, msg, TV_MLEN, pk);
    if (ret) {
        PRINTLN("FAIL");
    } else {
        PRINTLN("OK");
    }

    uint16_t used = stack_measure();
    PRINT("Max stack used: "); print_u32(used); PRINTLN(" bytes");

#ifdef __AVR__
    PRINT("SP after verify: "); print_u32(SP); PRINTLN("");
    PRINT("RAMEND: "); print_u32(RAMEND); PRINTLN("");
    PRINT("Heap start: "); print_u32((uint16_t)&__heap_start); PRINTLN("");
#endif

    PRINTLN("=== Done ===");
    while (1);
    return 0;
}
