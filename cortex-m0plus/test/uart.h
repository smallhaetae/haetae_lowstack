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
#ifndef UART_H
#define UART_H

#include <stdint.h>

#ifdef __arm__
  /* STM32C031C6 USART2 (PA2=TX, PA3=RX) — NUCLEO VCP via ST-Link */
  /* Minimal register definitions */
  #define RCC_BASE       0x40021000UL
  #define RCC_IOPENR     (*(volatile uint32_t *)(RCC_BASE + 0x34))
  #define RCC_APBENR1    (*(volatile uint32_t *)(RCC_BASE + 0x3C))
  #define GPIOA_BASE     0x50000000UL
  #define GPIOA_MODER    (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
  #define GPIOA_AFRL     (*(volatile uint32_t *)(GPIOA_BASE + 0x20))
  #define USART2_BASE    0x40004400UL
  #define USART2_CR1     (*(volatile uint32_t *)(USART2_BASE + 0x00))
  #define USART2_BRR     (*(volatile uint32_t *)(USART2_BASE + 0x0C))
  #define USART2_ISR     (*(volatile uint32_t *)(USART2_BASE + 0x1C))
  #define USART2_TDR     (*(volatile uint32_t *)(USART2_BASE + 0x28))

  #define FLASH_BASE     0x40022000UL
  #define FLASH_ACR      (*(volatile uint32_t *)(FLASH_BASE + 0x00))
  #define RCC_CR         (*(volatile uint32_t *)(RCC_BASE + 0x00))
  #define RCC_CFGR       (*(volatile uint32_t *)(RCC_BASE + 0x08))

  #ifndef F_CPU
  #define F_CPU 48000000UL  /* HSI48 direct */
  #endif
  #define BAUD 115200UL

  static void clock_init_48mhz(void) {
      /* Flash: 1 wait state for 48MHz */
      FLASH_ACR = (FLASH_ACR & ~0x7UL) | 1UL;
      /* Enable HSI48 */
      RCC_CR |= (1 << 8);                /* HSION = HSI48 enable */
      while (!(RCC_CR & (1 << 10)));      /* wait HSIRDY */
      /* HSIDIV = 0 (no division, full 48MHz) */
      RCC_CR &= ~(7UL << 11);
      /* Select HSI48 as SYSCLK (SW=00 already default) */
      RCC_CFGR = (RCC_CFGR & ~0x7UL) | 0x0UL;
  }

  static void uart_init(void) {
      clock_init_48mhz();
      RCC_IOPENR  |= (1 << 0);   /* GPIOA clock */
      RCC_APBENR1 |= (1 << 17);  /* USART2 clock */
      /* PA2 = AF1 (USART2_TX) */
      GPIOA_MODER = (GPIOA_MODER & ~(3UL << 4)) | (2UL << 4);
      GPIOA_AFRL  = (GPIOA_AFRL  & ~(0xFUL << 8)) | (1UL << 8);
      /* Baud rate */
      USART2_BRR = F_CPU / BAUD;
      /* Enable USART, TX */
      USART2_CR1 = (1 << 0) | (1 << 3);
  }

  static void uart_putchar(char c) {
      while (!(USART2_ISR & (1 << 7)));  /* TXE */
      USART2_TDR = (uint8_t)c;
  }

  static void uart_puts(const char *s) {
      while (*s) uart_putchar(*s++);
  }

#else
  /* x86: stdout */
  #include <stdio.h>
  static inline void uart_init(void) {}
  static inline void uart_puts(const char *s) { fputs(s, stdout); }
  static inline void uart_putchar(char c) { putchar(c); }
#endif

static void uart_print_u32(uint32_t v) {
    char buf[11];
    int i = 10;
    buf[i] = 0;
    if (v == 0) { uart_putchar('0'); return; }
    while (v) { buf[--i] = '0' + (v % 10); v /= 10; }
    uart_puts(&buf[i]);
}

static void uart_print_u64(uint64_t v) {
    char buf[21];
    int i = 20;
    buf[i] = 0;
    if (v == 0) { uart_putchar('0'); return; }
    while (v) { buf[--i] = '0' + (v % 10); v /= 10; }
    uart_puts(&buf[i]);
}

static void uart_println(const char *s) {
    uart_puts(s);
    uart_putchar('\n');
}

#endif /* UART_H */
