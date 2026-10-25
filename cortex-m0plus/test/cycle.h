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
#ifndef CYCLE_H
#define CYCLE_H

/*
 * Cycle counter for Cortex-M0+ using SysTick (24-bit, processor clock).
 * Overflow ISR extends to 32+ bits.
 */

#include <stdint.h>

#ifdef __arm__

#define SYST_CSR   (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018)
#define SCB_ICSR   (*(volatile uint32_t *)0xE000ED04)
#define SYST_MAX   0xFFFFFFUL

static volatile uint32_t _systick_ovf;

/* Called from vector table — override weak alias in startup.c */
void SysTick_Handler(void) {
    _systick_ovf++;
}

static void cycle_init(void) {
    SYST_RVR = SYST_MAX;
    SYST_CVR = 0;
    /* Enable: CLKSOURCE=processor, TICKINT=1, ENABLE=1 */
    SYST_CSR = (1 << 2) | (1 << 1) | (1 << 0);
    _systick_ovf = 0;
}

static uint64_t cycle_read(void) {
    uint32_t ovf, val;
    /* Disable interrupts for atomic read */
    __asm volatile ("cpsid i");
    ovf = _systick_ovf;
    val = SYST_CVR;
    /* A wrap between the ISR's last run and here is pending in ICSR; COUNTFLAG
     * cannot be used for this: it stays set after wraps the ISR already counted. */
    if (SCB_ICSR & (1u << 26)) {   /* PENDSTSET */
        val = SYST_CVR;            /* the counter has reloaded: re-read */
        ovf++;                     /* the ISR adds it to _systick_ovf once unmasked */
    }
    __asm volatile ("cpsie i");
    /* SysTick counts DOWN, so invert */
    return (uint64_t)ovf * (SYST_MAX + 1) + (SYST_MAX - val);
}

static void cycle_reset(void) {
    __asm volatile ("cpsid i");
    SYST_CVR = 0;                  /* also clears COUNTFLAG */
    _systick_ovf = 0;
    SCB_ICSR = (1u << 25);         /* drop a wrap pending from the previous interval */
    __asm volatile ("cpsie i");
}

#else /* x86 */

#include <time.h>
static void cycle_init(void) {}
static struct timespec _cyc_start;
static void cycle_reset(void) { clock_gettime(CLOCK_MONOTONIC, &_cyc_start); }
static uint64_t cycle_read(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t ns = (uint64_t)(now.tv_sec - _cyc_start.tv_sec) * 1000000000ULL
                  + (now.tv_nsec - _cyc_start.tv_nsec);
    return ns / 1000;
}

#endif

/* Sort uint64_t array (insertion sort, small N) */
static void sort_u64(uint64_t *a, int n) {
    for (int i = 1; i < n; i++) {
        uint64_t key = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > key) { a[j+1] = a[j]; j--; }
        a[j+1] = key;
    }
}

#endif /* CYCLE_H */
