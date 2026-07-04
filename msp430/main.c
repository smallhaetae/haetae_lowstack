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

#include "api.h"
#include "randombytes.h"
#include <msp430.h>

#ifndef BENCH_STIR_VALUE
#define BENCH_STIR_VALUE 0u
#endif

volatile uint32_t g_cycles_keygen __attribute__((section(".noinit")));
volatile uint32_t g_cycles_sign __attribute__((section(".noinit")));
volatile uint32_t g_cycles_verify __attribute__((section(".noinit")));

volatile int g_status_keygen __attribute__((section(".noinit")));
volatile int g_status_sign __attribute__((section(".noinit")));
volatile int g_status_verify __attribute__((section(".noinit")));
volatile size_t g_siglen __attribute__((section(".noinit")));

volatile uint32_t g_timer_overflows __attribute__((section(".noinit")));
volatile uint32_t g_fail_stage __attribute__((section(".noinit")));
volatile uint16_t g_result_magic __attribute__((section(".noinit")));
volatile uint16_t g_led_mode __attribute__((section(".noinit")));
volatile uint32_t g_rng_boot_state __attribute__((section(".noinit")));

static uint8_t g_pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t g_sk[CRYPTO_SECRETKEYBYTES];
static uint8_t g_sig[CRYPTO_BYTES];
static const uint8_t g_msg[] = "haetae bench";

#define BENCH_INFO_ADDR ((uint16_t *)0x1800)
#define BENCH_MAGIC_START 0x1111
#define BENCH_MAGIC_KEYGEN_DONE 0x2222
#define BENCH_MAGIC_SIGN_DONE 0x3333
#define BENCH_MAGIC_OK 0x600D
#define BENCH_MAGIC_FAIL 0xBEEF

#define MCLK_HZ 8000000UL

#define LED_MODE_IDLE 0u
#define LED_MODE_KEYGEN 1u
#define LED_MODE_SIGN 2u
#define LED_MODE_VERIFY 3u
#define LED_MODE_SUCCESS 4u
#define LED_MODE_FAIL 5u

typedef struct {
    uint16_t magic;
    uint16_t fail_stage;
    int16_t status_verify;
    int16_t status_sign;
    int16_t status_keygen;
    uint16_t reserved;
    uint32_t siglen;
    uint32_t cycles_verify;
    uint32_t cycles_sign;
    uint32_t cycles_keygen;
} bench_result_t;

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
    P1OUT &= (unsigned char)~BIT0;
}

static void led_on(void)
{
    P1OUT |= BIT0;
}

static void led_off(void)
{
    P1OUT &= (unsigned char)~BIT0;
}

static void delay_cycles_approx(uint32_t cycles)
{
    while (cycles--) {
        __no_operation();
    }
}

static void blink_once(uint32_t on_cycles, uint32_t off_cycles)
{
    led_on();
    delay_cycles_approx(on_cycles);
    led_off();
    delay_cycles_approx(off_cycles);
}

static void blink_count(uint32_t count, uint32_t on_cycles, uint32_t off_cycles,
                        uint32_t pause_cycles)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        blink_once(on_cycles, off_cycles);
    }

    delay_cycles_approx(pause_cycles);
}

static void announce_startup(void)
{
    blink_count(4, 120000UL, 120000UL, 400000UL);
}

static void announce_stage(uint32_t stage)
{
    blink_count(stage, 200000UL, 160000UL, 800000UL);
}

static void rng_stir_boot(void)
{
    uint32_t stir = (uint32_t)BENCH_STIR_VALUE;

    if (stir == 0u) {
        stir = g_rng_boot_state;
        if ((stir == 0u) || (stir == 0xffffffffu)) {
            stir = 0x6d2b79f5u;
        }

        stir = stir * 1664525u + 1013904223u;
    }

    g_rng_boot_state = stir ^ 0xa5a5f00du;
    randombytes_stir(stir);
}

static void led_show_pulses(uint16_t pulses, uint32_t ticks)
{
    uint32_t slot = ticks % 80u;
    uint32_t pulse_end = (uint32_t)pulses * 16u;

    if ((pulses != 0u) && (slot < pulse_end) && ((slot & 0x0Fu) < 8u)) {
        led_on();
    } else {
        led_off();
    }
}

static void result_state_init(void)
{
    g_result_magic = BENCH_MAGIC_START;
    g_cycles_keygen = 0;
    g_cycles_sign = 0;
    g_cycles_verify = 0;
    g_status_keygen = -1;
    g_status_sign = -1;
    g_status_verify = -1;
    g_siglen = 0;
    g_timer_overflows = 0;
    g_fail_stage = 0;
    g_led_mode = LED_MODE_IDLE;
}

static void fail_halt(uint32_t stage)
{
    g_fail_stage = stage;
    g_result_magic = BENCH_MAGIC_FAIL;
    g_led_mode = LED_MODE_FAIL;

    for (;;) {
        __no_operation();
    }
}

static void cycle_counter_init(void)
{
    TA0CTL = TACLR;
    TA0EX0 = TAIDEX_0;
    TA0CTL = TASSEL__SMCLK | ID__1 | MC__CONTINUOUS | TAIE;
    __enable_interrupt();
}

static uint32_t cycle_counter_read(void)
{
    uint16_t low;
    uint32_t high;

    __disable_interrupt();
    high = g_timer_overflows;
    low = TA0R;
    if ((TA0CTL & TAIFG) != 0) {
        high++;
        low = TA0R;
    }
    __enable_interrupt();

    return (high << 16) | low;
}

static uint32_t measure_keygen(void)
{
    uint32_t start = cycle_counter_read();
    g_status_keygen = crypto_sign_keypair(g_pk, g_sk);
    return cycle_counter_read() - start;
}

static uint32_t measure_sign(void)
{
    uint32_t start = cycle_counter_read();
    g_status_sign = crypto_sign_signature(g_sig, (size_t *)&g_siglen, g_msg,
                                          sizeof(g_msg) - 1, g_sk);
    return cycle_counter_read() - start;
}

static uint32_t measure_verify(void)
{
    uint32_t start = cycle_counter_read();
    g_status_verify =
        crypto_sign_verify(g_sig, g_siglen, g_msg, sizeof(g_msg) - 1, g_pk);
    return cycle_counter_read() - start;
}

int main(void)
{
    clock_init();
    led_init();
    result_state_init();
    announce_startup();
    rng_stir_boot();
    cycle_counter_init();

    announce_stage(1);
    g_led_mode = LED_MODE_KEYGEN;
    g_cycles_keygen = measure_keygen();
    if (g_status_keygen != 0) {
        g_fail_stage = 1;
        fail_halt(1);
    }
    g_result_magic = BENCH_MAGIC_KEYGEN_DONE;

    announce_stage(2);
    g_led_mode = LED_MODE_SIGN;
    g_cycles_sign = measure_sign();
    if (g_status_sign != 0) {
        g_fail_stage = 2;
        fail_halt(2);
    }
    g_result_magic = BENCH_MAGIC_SIGN_DONE;

    announce_stage(3);
    g_led_mode = LED_MODE_VERIFY;
    g_cycles_verify = measure_verify();
    if (g_status_verify != 0) {
        g_fail_stage = 3;
        fail_halt(3);
    }

    g_result_magic = BENCH_MAGIC_OK;
    g_led_mode = LED_MODE_SUCCESS;
    led_on();
    for (;;) {
        __no_operation();
    }
}

__attribute__((interrupt(TIMER0_A1_VECTOR))) static void timer0_a1_isr(void)
{
    switch (__even_in_range(TA0IV, 0x0E)) {
    case TA0IV_TA0IFG:
        g_timer_overflows++;
        if (g_led_mode == LED_MODE_SUCCESS) {
            led_on();
        } else if (g_led_mode == LED_MODE_FAIL) {
            led_show_pulses((uint16_t)g_fail_stage, g_timer_overflows);
        } else if (g_led_mode == LED_MODE_VERIFY) {
            led_show_pulses(3u, g_timer_overflows);
        } else if (g_led_mode == LED_MODE_SIGN) {
            led_show_pulses(2u, g_timer_overflows);
        } else if (g_led_mode == LED_MODE_KEYGEN) {
            led_show_pulses(1u, g_timer_overflows);
        } else {
            led_off();
        }
        break;
    default:
        break;
    }
}
