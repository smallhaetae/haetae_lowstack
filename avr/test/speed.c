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
/* Timer (AVR: Timer1 cycle counter, x86: clock_gettime)              */
/* ------------------------------------------------------------------ */
#ifdef __AVR__
  #include <avr/interrupt.h>

  static volatile uint16_t timer1_ovf_count;

  ISR(TIMER1_OVF_vect) {
      timer1_ovf_count++;
  }

  static void timer_init(void) {
      TCCR1A = 0;
      TCCR1B = (1 << CS10);  /* prescaler=1, 1 tick = 1/F_CPU */
      TIMSK1 = (1 << TOIE1);
      sei();
  }

  static uint32_t timer_read(void) {
      uint16_t ovf, cnt;
      uint8_t sreg = SREG;
      cli();
      ovf = timer1_ovf_count;
      cnt = TCNT1;
      if ((TIFR1 & (1 << TOV1)) && cnt < 0x8000) ovf++;
      SREG = sreg;
      return ((uint32_t)ovf << 16) | cnt;
  }

  static void timer_reset(void) {
      uint8_t sreg = SREG;
      cli();
      TCNT1 = 0;
      timer1_ovf_count = 0;
      TIFR1 = (1 << TOV1);
      SREG = sreg;
  }

#else
  #include <time.h>

  static void timer_init(void) {}
  static struct timespec _ts_start;
  static void timer_reset(void) { clock_gettime(CLOCK_MONOTONIC, &_ts_start); }
  static uint32_t timer_read(void) {
      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      uint64_t ns = (uint64_t)(now.tv_sec - _ts_start.tv_sec) * 1000000000ULL
                    + (now.tv_nsec - _ts_start.tv_nsec);
      return (uint32_t)(ns / 1000);
  }
#endif

/* ------------------------------------------------------------------ */
/* Main: verify speed measurement                                      */
/* ------------------------------------------------------------------ */
#define NUM_RUNS 3

int main(void)
{
    uart_init();
    timer_init();

    PRINTLN("=== HAETAE speed measurement ===");
    PRINT("HAETAE-"); print_u32(HAETAE_MODE); PRINTLN("");
#ifdef __AVR__
    PRINT("F_CPU: "); print_u32(F_CPU); PRINTLN(" Hz");
#endif

    uint8_t sig[TV_SIGLEN];
    uint8_t msg[TV_MLEN];

#ifdef __AVR__
    memcpy_P(sig, tv_sig, TV_SIGLEN);
#else
    memcpy(sig, tv_sig, TV_SIGLEN);
#endif
    memcpy(msg, tv_msg, TV_MLEN);
    const uint8_t *pk = tv_pk;

    for (int run = 0; run < NUM_RUNS; run++) {
        PRINT("Run "); print_u32(run + 1); PRINT("/"); print_u32(NUM_RUNS); PRINT(": ");

        timer_reset();
        int ret = crypto_sign_verify(sig, TV_SIGLEN, msg, TV_MLEN, pk);
        uint32_t elapsed = timer_read();

        if (ret) {
            PRINTLN("FAIL");
        } else {
#ifdef __AVR__
            print_u32(elapsed); PRINT(" cycles (");
            print_u32(elapsed / (F_CPU / 1000UL)); PRINTLN(" ms)");
#else
            print_u32(elapsed); PRINTLN(" us");
#endif
        }
    }

    PRINTLN("=== Done ===");
    while (1);
    return 0;
}
