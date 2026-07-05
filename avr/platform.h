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
#ifndef PLATFORM_H
#define PLATFORM_H

/*
 * Platform abstraction for AVR / MSP430 / native builds.
 * - CONST_TABLE: place constant arrays in Flash (AVR PROGMEM)
 * - READ_BYTE/WORD/DWORD: read from const table
 */

#ifdef __AVR__
  #include <avr/pgmspace.h>
  #include <stdint.h>
  #include <string.h>
  #define CONST_TABLE        PROGMEM
  #define READ_BYTE(addr)    pgm_read_byte(&(addr))
  #define READ_WORD(addr)    pgm_read_word(&(addr))
  #define READ_DWORD(addr)   pgm_read_dword(&(addr))
  static inline uint64_t _read_qword(const void *addr) {
      uint32_t lo = pgm_read_dword((const uint32_t*)addr);
      uint32_t hi = pgm_read_dword((const uint32_t*)addr + 1);
      return ((uint64_t)hi << 32) | lo;
  }
  #define READ_QWORD(addr)   _read_qword(&(addr))
  /* Read a struct of given size from PROGMEM into a stack-local variable */
  #define READ_STRUCT(dst, src_ptr, type) memcpy_P(&(dst), (src_ptr), sizeof(type))
#else
  #define CONST_TABLE
  #define READ_BYTE(addr)    (addr)
  #define READ_WORD(addr)    (addr)
  #define READ_DWORD(addr)   (addr)
  #define READ_QWORD(addr)   (addr)
  #define READ_STRUCT(dst, src_ptr, type) ((dst) = *(src_ptr))
#endif

#endif /* PLATFORM_H */
