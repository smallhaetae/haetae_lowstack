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
 * pointmul_asm.S - readable AVR-GCC assembly pointwise Montgomery operations
 * for HAETAE int32_t polynomials.
 *
 * ABI:
 *   void poly_pointwise_montgomery(poly *c, const poly *a, const poly *b);
 *   void poly_pointwise_montgomery_addto(poly *c, const poly *a, const poly *b);
 *   void poly_pointwise_montgomery_mixed(poly *c,
 *                                        const poly_frozen *a,
 *                                        const poly *b);
 *   void poly_pointwise_montgomery_mixed_addto(poly *c,
 *                                              const poly_frozen *a,
 *                                              const poly *b);
 *   void polyq_unpack_pointwise_montgomery(poly *c,
 *                                          const uint8_t *a,
 *                                          const poly *b);
 *
 * poly_frozen and D==0 packed-q coefficients are unsigned 16-bit values.
 * All products are reduced with the HAETAE Montgomery reducer:
 *
 *   r = a * b * 2^-32 mod q, q = 64513.
 */

#include "config.h"
#include "params.h"

#if HAETAE_MODE == 2
#define MONT_REDUCE_SYMBOL cryptolab_haetae2_montgomery_reduce
#define POLY_PWMUL_SYMBOL cryptolab_haetae2_poly_pointwise_montgomery
#define POLY_PWMUL_ADDTO_SYMBOL cryptolab_haetae2_poly_pointwise_montgomery_addto
#define POLY_PWMUL_MIXED_SYMBOL cryptolab_haetae2_poly_pointwise_montgomery_mixed
#define POLY_PWMUL_MIXED_ADDTO_SYMBOL cryptolab_haetae2_poly_pointwise_montgomery_mixed_addto
#define POLYQ_UNPACK_PWMUL_SYMBOL cryptolab_haetae2_polyq_unpack_pointwise_montgomery
#define POLYQ_UNPACK_PWMUL_P_SYMBOL cryptolab_haetae2_polyq_unpack_pointwise_montgomery_P_asm
#elif HAETAE_MODE == 3
#define MONT_REDUCE_SYMBOL cryptolab_haetae3_montgomery_reduce
#define POLY_PWMUL_SYMBOL cryptolab_haetae3_poly_pointwise_montgomery
#define POLY_PWMUL_ADDTO_SYMBOL cryptolab_haetae3_poly_pointwise_montgomery_addto
#define POLY_PWMUL_MIXED_SYMBOL cryptolab_haetae3_poly_pointwise_montgomery_mixed
#define POLY_PWMUL_MIXED_ADDTO_SYMBOL cryptolab_haetae3_poly_pointwise_montgomery_mixed_addto
#define POLYQ_UNPACK_PWMUL_SYMBOL cryptolab_haetae3_polyq_unpack_pointwise_montgomery
#define POLYQ_UNPACK_PWMUL_P_SYMBOL cryptolab_haetae3_polyq_unpack_pointwise_montgomery_P_asm
#elif HAETAE_MODE == 5
#define MONT_REDUCE_SYMBOL cryptolab_haetae5_montgomery_reduce
#define POLY_PWMUL_SYMBOL cryptolab_haetae5_poly_pointwise_montgomery
#define POLY_PWMUL_ADDTO_SYMBOL cryptolab_haetae5_poly_pointwise_montgomery_addto
#define POLY_PWMUL_MIXED_SYMBOL cryptolab_haetae5_poly_pointwise_montgomery_mixed
#define POLY_PWMUL_MIXED_ADDTO_SYMBOL cryptolab_haetae5_poly_pointwise_montgomery_mixed_addto
#define POLYQ_UNPACK_PWMUL_SYMBOL cryptolab_haetae5_polyq_unpack_pointwise_montgomery
#define POLYQ_UNPACK_PWMUL_P_SYMBOL cryptolab_haetae5_polyq_unpack_pointwise_montgomery_P_asm
#else
#error "unsupported HAETAE_MODE"
#endif

__zero_reg__ = 1

.text

.extern __mulsidi3

/* Register allocation:
 *   Y       input a pointer for poly/poly and mixed variants
 *   r2:r3  end pointer for Y
 *   r4:r5  output/accumulator pointer
 *   r6:r7  input b/src_hat pointer
 *
 * The Montgomery reduction macro uses r10-r17 as scratch.  Outer entry points
 * save those callee-saved registers once in their prologue.
 */

.macro SAVE_REGS
    push    r2
    push    r3
    push    r4
    push    r5
    push    r6
    push    r7
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    push    r16
    push    r17
    push    r28
    push    r29
.endm

.macro RESTORE_REGS
    pop     r29
    pop     r28
    pop     r17
    pop     r16
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r7
    pop     r6
    pop     r5
    pop     r4
    pop     r3
    pop     r2
.endm

.macro LOAD32_POST ptrlo, d0, d1, d2, d3
    movw    r30, \ptrlo
    ld      \d0, Z+
    ld      \d1, Z+
    ld      \d2, Z+
    ld      \d3, Z+
    movw    \ptrlo, r30
.endm

.macro STORE32_POST ptrlo, s0, s1, s2, s3
    movw    r30, \ptrlo
    st      Z+, \s0
    st      Z+, \s1
    st      Z+, \s2
    st      Z+, \s3
    movw    \ptrlo, r30
.endm

.macro ADDTO32_POST ptrlo
    movw    r30, \ptrlo
    ld      r18, Z
    ldd     r19, Z+1
    ldd     r20, Z+2
    ldd     r21, Z+3
    add     r22, r18
    adc     r23, r19
    adc     r24, r20
    adc     r25, r21
    st      Z+, r22
    st      Z+, r23
    st      Z+, r24
    st      Z+, r25
    movw    \ptrlo, r30
.endm

.macro MUL16_Q64513_U x0, x1, c0, c1, c2, c3
    mov     \c0, \x0
    mov     \c1, \x1
    clr     \c2
    clr     \c3
    ldi     r26, 0xfc
    mul     \x0, r26
    add     \c1, r0
    adc     \c2, r1
    adc     \c3, r14
    mul     \x1, r26
    add     \c2, r0
    adc     \c3, r1
.endm

.macro MUL32_QINV_LO32
    /* r18-r21 *= 0x380f0401 mod 2^32, output r22-r25. */
    clr     r14
    movw    r22, r18
    movw    r24, r20

    ldi     r26, 0x04
    mul     r20, r26
    add     r25, r0
    mul     r19, r26
    add     r24, r0
    adc     r25, r1
    mul     r18, r26
    add     r23, r0
    adc     r24, r1
    adc     r25, r14

    ldi     r26, 0x0f
    mul     r19, r26
    add     r25, r0
    mul     r18, r26
    add     r24, r0
    adc     r25, r1

    ldi     r26, 0x38
    mul     r18, r26
    add     r25, r0
.endm

.macro MONT_REDUCE_R18
    /* Input:  signed 64-bit product in r18-r25.
     * Output: high32(product - low32(product * QINV) * Q) in r22-r25.
     */
    movw    r10, r22        /* signed high32(product) */
    movw    r12, r24

    MUL32_QINV_LO32         /* t = low32(product * QINV), r22-r25 */
    clr     r17
    sbrc    r25, 7
    com     r17             /* signed t correction mask */

    MUL16_Q64513_U r22, r23, r18, r19, r20, r21
    mov     r15, r20        /* high16(t_lo * Q) */
    mov     r16, r21

    MUL16_Q64513_U r24, r25, r18, r19, r20, r21
    add     r18, r15
    adc     r19, r16
    adc     r20, r14        /* high32(t*Q) = high16(t_hi*Q) + carry */
    adc     r21, r14

    movw    r22, r10
    movw    r24, r12
    sub     r22, r20
    sbc     r23, r21
    sbc     r24, r14
    sbc     r25, r14
    mov     r18, r17
    andi    r18, 0x01
    add     r22, r18
    mov     r18, r17
    andi    r18, 0xfc
    adc     r23, r18
    clr     r18
    adc     r24, r18
    adc     r25, r18
    clr     __zero_reg__
.endm

.global MONT_REDUCE_SYMBOL
.type MONT_REDUCE_SYMBOL, @function
MONT_REDUCE_SYMBOL:
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15
    push    r16
    push    r17
    MONT_REDUCE_R18
    pop     r17
    pop     r16
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    ret
.size MONT_REDUCE_SYMBOL, .-MONT_REDUCE_SYMBOL

.macro INIT_POLY_POLY
    SAVE_REGS
    movw    r28, r22        /* Y = a */
    movw    r6, r20         /* b */
    movw    r4, r24         /* c */
    movw    r2, r22         /* end = a + 1024 bytes */
    ldi     r18, 4
    add     r3, r18
.endm

.macro LOOP_NEXT_32 label, done
    cp      r28, r2
    cpc     r29, r3
    breq    \done
    rjmp    \label
.endm

.macro FINISH
    clr     __zero_reg__
    RESTORE_REGS
    ret
.endm

.global POLY_PWMUL_SYMBOL
.type POLY_PWMUL_SYMBOL, @function
POLY_PWMUL_SYMBOL:
    INIT_POLY_POLY
.Lpwmul_loop:
    ld      r22, Y+
    ld      r23, Y+
    ld      r24, Y+
    ld      r25, Y+
    LOAD32_POST r6, r18, r19, r20, r21
    call    __mulsidi3
    MONT_REDUCE_R18
    STORE32_POST r4, r22, r23, r24, r25
    LOOP_NEXT_32 .Lpwmul_loop, .Lpwmul_done
.Lpwmul_done:
    FINISH
.size POLY_PWMUL_SYMBOL, .-POLY_PWMUL_SYMBOL

.global POLY_PWMUL_ADDTO_SYMBOL
.type POLY_PWMUL_ADDTO_SYMBOL, @function
POLY_PWMUL_ADDTO_SYMBOL:
    INIT_POLY_POLY
.Lpwmul_addto_loop:
    ld      r22, Y+
    ld      r23, Y+
    ld      r24, Y+
    ld      r25, Y+
    LOAD32_POST r6, r18, r19, r20, r21
    call    __mulsidi3
    MONT_REDUCE_R18
    ADDTO32_POST r4
    LOOP_NEXT_32 .Lpwmul_addto_loop, .Lpwmul_addto_done
.Lpwmul_addto_done:
    FINISH
.size POLY_PWMUL_ADDTO_SYMBOL, .-POLY_PWMUL_ADDTO_SYMBOL

#if FROZEN_A >= 1
.macro INIT_MIXED
    SAVE_REGS
    movw    r28, r22        /* Y = frozen a */
    movw    r6, r20         /* b */
    movw    r4, r24         /* c */
    movw    r2, r22         /* end = a + 512 bytes */
    ldi     r18, 2
    add     r3, r18
.endm

.global POLY_PWMUL_MIXED_SYMBOL
.type POLY_PWMUL_MIXED_SYMBOL, @function
POLY_PWMUL_MIXED_SYMBOL:
    INIT_MIXED
.Lpwmul_mixed_loop:
    ld      r18, Y+
    ld      r19, Y+
    clr     r20
    clr     r21
    LOAD32_POST r6, r22, r23, r24, r25
    call    __mulsidi3
    MONT_REDUCE_R18
    STORE32_POST r4, r22, r23, r24, r25
    LOOP_NEXT_32 .Lpwmul_mixed_loop, .Lpwmul_mixed_done
.Lpwmul_mixed_done:
    FINISH
.size POLY_PWMUL_MIXED_SYMBOL, .-POLY_PWMUL_MIXED_SYMBOL

.global POLY_PWMUL_MIXED_ADDTO_SYMBOL
.type POLY_PWMUL_MIXED_ADDTO_SYMBOL, @function
POLY_PWMUL_MIXED_ADDTO_SYMBOL:
    INIT_MIXED
.Lpwmul_mixed_addto_loop:
    ld      r18, Y+
    ld      r19, Y+
    clr     r20
    clr     r21
    LOAD32_POST r6, r22, r23, r24, r25
    call    __mulsidi3
    MONT_REDUCE_R18
    ADDTO32_POST r4
    LOOP_NEXT_32 .Lpwmul_mixed_addto_loop, .Lpwmul_mixed_addto_done
.Lpwmul_mixed_addto_done:
    FINISH
.size POLY_PWMUL_MIXED_ADDTO_SYMBOL, .-POLY_PWMUL_MIXED_ADDTO_SYMBOL
#endif

#if D == 0
.global POLYQ_UNPACK_PWMUL_SYMBOL
.type POLYQ_UNPACK_PWMUL_SYMBOL, @function
POLYQ_UNPACK_PWMUL_SYMBOL:
    SAVE_REGS
    movw    r28, r22        /* Y = packed q coefficients */
    movw    r6, r20         /* src_hat */
    movw    r4, r24         /* dest */
    movw    r2, r20         /* end = src_hat + 1024 bytes */
    ldi     r18, 4
    add     r3, r18

.Lpolyq_pwmul_loop:
    ld      r18, Y+
    ld      r19, Y+
    clr     r20
    clr     r21
    LOAD32_POST r6, r22, r23, r24, r25
    call    __mulsidi3
    MONT_REDUCE_R18
    STORE32_POST r4, r22, r23, r24, r25

    cp      r6, r2
    cpc     r7, r3
    breq    .Lpolyq_pwmul_done
    rjmp    .Lpolyq_pwmul_loop
.Lpolyq_pwmul_done:
    FINISH
.size POLYQ_UNPACK_PWMUL_SYMBOL, .-POLYQ_UNPACK_PWMUL_SYMBOL

.global POLYQ_UNPACK_PWMUL_P_SYMBOL
.type POLYQ_UNPACK_PWMUL_P_SYMBOL, @function
POLYQ_UNPACK_PWMUL_P_SYMBOL:
    SAVE_REGS
    movw    r28, r20        /* Y = src_hat */
    movw    r6, r22         /* packed q coefficients in flash */
    movw    r4, r24         /* dest */
    movw    r2, r20         /* end = src_hat + 1024 bytes */
    ldi     r18, 4
    add     r3, r18

.Lpolyq_pwmul_p_loop:
    movw    r30, r6
    lpm     r18, Z+
    lpm     r19, Z+
    movw    r6, r30
    clr     r20
    clr     r21
    ld      r22, Y+
    ld      r23, Y+
    ld      r24, Y+
    ld      r25, Y+
    call    __mulsidi3
    MONT_REDUCE_R18
    STORE32_POST r4, r22, r23, r24, r25

    cp      r28, r2
    cpc     r29, r3
    breq    .Lpolyq_pwmul_p_done
    rjmp    .Lpolyq_pwmul_p_loop
.Lpolyq_pwmul_p_done:
    FINISH
.size POLYQ_UNPACK_PWMUL_P_SYMBOL, .-POLYQ_UNPACK_PWMUL_P_SYMBOL
#endif

#undef MONT_REDUCE_SYMBOL
#undef POLY_PWMUL_SYMBOL
#undef POLY_PWMUL_ADDTO_SYMBOL
#undef POLY_PWMUL_MIXED_SYMBOL
#undef POLY_PWMUL_MIXED_ADDTO_SYMBOL
#undef POLYQ_UNPACK_PWMUL_SYMBOL
#undef POLYQ_UNPACK_PWMUL_P_SYMBOL
