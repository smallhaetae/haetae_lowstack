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
#ifndef AVR_UART_H
#define AVR_UART_H

/*
 * Shared UART / console I/O for test entry points.
 * - x86: printf via stdout
 * - AVR + SIMAVR_SIM: GPIOR0 console (simavr host stdout)
 * - AVR + real HW: UART0
 */

#include <stdint.h>

#ifdef SIMAVR_SIM
#include <avr/io.h>
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega2560");
AVR_MCU_SIMAVR_CONSOLE(&GPIOR0);
#endif

#ifdef __AVR__
  #ifdef SIMAVR_SIM
    static void uart_putchar(char c) {
        GPIOR0 = (uint8_t)c;
    }
    static inline void uart_init(void) {}
  #else
    #include <avr/io.h>
    #ifndef BAUD
    #define BAUD 9600UL
    #endif
    static void uart_init(void) {
        uint16_t ubrr = F_CPU / 16 / BAUD - 1;
        UBRR0H = (uint8_t)(ubrr >> 8);
        UBRR0L = (uint8_t)(ubrr);
        UCSR0B = (1 << TXEN0);
        UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    }
    static void uart_putchar(char c) {
        while (!(UCSR0A & (1 << UDRE0)));
        UDR0 = (uint8_t)c;
    }
  #endif /* SIMAVR_SIM */

  static void uart_puts(const char *s) {
      while (*s) uart_putchar(*s++);
  }
  #define PRINT(s)   uart_puts(s)
  #define PRINTLN(s) do { uart_puts(s); uart_putchar('\n'); } while(0)

#else /* x86 */
  #include <stdio.h>
  static inline void uart_init(void) {}
  #define PRINT(s)   fputs(s, stdout)
  #define PRINTLN(s) puts(s)
#endif

/* Minimal number printing */
static void print_u32(uint32_t v) {
    char buf[11];
    int i = 10;
    buf[i] = 0;
    if (v == 0) { PRINT("0"); return; }
    while (v) { buf[--i] = '0' + (v % 10); v /= 10; }
    PRINT(&buf[i]);
}

static void print_i32(int32_t v) {
    if (v < 0) { PRINT("-"); v = -v; }
    print_u32((uint32_t)v);
}

#endif /* AVR_UART_H */
