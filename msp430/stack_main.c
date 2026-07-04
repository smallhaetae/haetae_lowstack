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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "api.h"
#include "randombytes.h"
#include "testvec.h"
#include <msp430.h>

#ifndef STACK_SCENARIO
#define STACK_SCENARIO 1
#endif

#define STACK_PAINT 0xAAu
#define STACK_MAGIC_OK 0x600Du
#define STACK_MAGIC_FAIL 0xBEEFu

extern uint8_t __heap_end__;
extern uint8_t __stack;

volatile uint32_t g_stack_peak __attribute__((section(".noinit")));
volatile int g_stack_status __attribute__((section(".noinit")));
volatile size_t g_stack_siglen __attribute__((section(".noinit")));
volatile uint16_t g_stack_magic __attribute__((section(".noinit")));
volatile uint16_t g_stack_scenario __attribute__((section(".noinit")));

__attribute__((section(".crt_0001wdt"), naked, used, no_instrument_function))
static void crt0_stop_wdt(void)
{
    __asm__ volatile(
        "mov #0x5a80, &0x015c\n"
    );
}

static void clock_init(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    UCSCTL3 = SELREF__REFOCLK;
    UCSCTL4 = SELA__REFOCLK | SELS__DCOCLKDIV | SELM__DCOCLKDIV;
    UCSCTL6 |= XT1OFF | XT2OFF;

    __bis_SR_register(SCG0);
    UCSCTL0 = 0;
    UCSCTL1 = DCORSEL_5;
    UCSCTL2 = FLLD__1 | 249;
    __bic_SR_register(SCG0);

    __delay_cycles(250000UL);
    UCSCTL7 &= (uint16_t)~DCOFFG;
    SFRIFG1 &= (uint8_t)~OFIFG;
}

static void led_init(void)
{
    P1DIR |= BIT0;
    P1OUT &= (uint8_t)~BIT0;
}

static void led_on(void)
{
    P1OUT |= BIT0;
}

static void led_off(void)
{
    P1OUT &= (uint8_t)~BIT0;
}

static void delay_cycles_approx(uint32_t cycles)
{
    while (cycles--) {
        __no_operation();
    }
}

static void fail_halt(void)
{
    for (;;) {
        led_on();
        delay_cycles_approx(250000UL);
        led_off();
        delay_cycles_approx(250000UL);
    }
}

static void success_halt(void)
{
    led_on();
    for (;;) {
        __no_operation();
    }
}

static uint8_t *stack_sp(void)
{
    uint8_t *sp;

    __asm__ volatile("mov r1, %0" : "=r"(sp));
    return sp;
}

static void stack_paint(void)
{
    volatile uint8_t *p = &__heap_end__;
    volatile uint8_t *sp = stack_sp();

    while (p < sp) {
        *p++ = STACK_PAINT;
    }
}

static uint32_t stack_measure(void)
{
    uint8_t *p = &__heap_end__;
    uint8_t *top = &__stack;

    while ((p < top) && (*p == STACK_PAINT)) {
        p++;
    }

    return (uint32_t)(top - p);
}

static int scenario_keygen(void)
{
    int ret;

    stack_paint();
    {
        uint8_t sk[CRYPTO_SECRETKEYBYTES];
        uint8_t pk[CRYPTO_PUBLICKEYBYTES];

        randombytes_reset();
        ret = crypto_sign_keypair(pk, sk);
    }

    return ret;
}

static int scenario_sign(void)
{
    int ret;

    stack_paint();
    {
        uint8_t sk[CRYPTO_SECRETKEYBYTES];
        uint8_t sig[CRYPTO_BYTES];
        size_t siglen = 0;

        memcpy(sk, tv_sk, TV_SKLEN);
        randombytes_reset();
        ret = crypto_sign_signature(sig, &siglen, tv_msg, TV_MLEN, sk);
        g_stack_siglen = siglen;
    }

    return ret;
}

static int scenario_verify(void)
{
    int ret;

    stack_paint();
    {
        uint8_t pk[CRYPTO_PUBLICKEYBYTES];
        uint8_t sig[CRYPTO_BYTES];

        memcpy(pk, tv_pk, TV_PKLEN);
        memcpy(sig, tv_sig, TV_SIGLEN);
        ret = crypto_sign_verify(sig, TV_SIGLEN, tv_msg, TV_MLEN, pk);
    }

    return ret;
}

int main(void)
{
    clock_init();
    led_init();

    g_stack_peak = 0;
    g_stack_status = -1;
    g_stack_siglen = 0;
    g_stack_magic = 0;
    g_stack_scenario = STACK_SCENARIO;

#if STACK_SCENARIO == 1
    g_stack_status = scenario_keygen();
#elif STACK_SCENARIO == 2
    g_stack_status = scenario_sign();
#elif STACK_SCENARIO == 3
    g_stack_status = scenario_verify();
#else
#error "Unsupported STACK_SCENARIO"
#endif

    g_stack_peak = stack_measure();

    if (g_stack_status != 0) {
        g_stack_magic = STACK_MAGIC_FAIL;
        fail_halt();
    }

    g_stack_magic = STACK_MAGIC_OK;
    success_halt();
}
