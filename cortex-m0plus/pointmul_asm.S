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
 * pointmul_asm.S - Cortex-M0+ pointwise Montgomery multiplication for HAETAE.
 *
 * The M0+ has only the low-word multiply instruction, so the 32x32->64
 * products are expanded with 16-bit limbs instead of using UMULL/SMULL.
 */

#include "config.h"
#include "params.h"

#if (defined(__arm__) || defined(__thumb__)) && \
    (!defined(HAETAE_FORCE_C_POINTMUL) || !defined(HAETAE_FORCE_C_MONTGOMERY_REDUCE))

#if HAETAE_MODE == 2
#define MONT_REDUCE_SYMBOL cryptolab_haetae2_montgomery_reduce
#define MONT_MUL_SYMBOL cryptolab_haetae2_montgomery_mul_i32
#define MONT_MUL_U16_SYMBOL cryptolab_haetae2_montgomery_mul_u16_i32
#define POLY_PWMUL_SYMBOL cryptolab_haetae2_poly_pointwise_montgomery
#define POLY_PWMUL_ADDTO_SYMBOL cryptolab_haetae2_poly_pointwise_montgomery_addto
#define POLY_PWMUL_MIXED_SYMBOL cryptolab_haetae2_poly_pointwise_montgomery_mixed
#define POLY_PWMUL_MIXED_ADDTO_SYMBOL cryptolab_haetae2_poly_pointwise_montgomery_mixed_addto
#define POLYQ_UNPACK_PWMUL_SYMBOL cryptolab_haetae2_polyq_unpack_pointwise_montgomery
#elif HAETAE_MODE == 3
#define MONT_REDUCE_SYMBOL cryptolab_haetae3_montgomery_reduce
#define MONT_MUL_SYMBOL cryptolab_haetae3_montgomery_mul_i32
#define MONT_MUL_U16_SYMBOL cryptolab_haetae3_montgomery_mul_u16_i32
#define POLY_PWMUL_SYMBOL cryptolab_haetae3_poly_pointwise_montgomery
#define POLY_PWMUL_ADDTO_SYMBOL cryptolab_haetae3_poly_pointwise_montgomery_addto
#define POLY_PWMUL_MIXED_SYMBOL cryptolab_haetae3_poly_pointwise_montgomery_mixed
#define POLY_PWMUL_MIXED_ADDTO_SYMBOL cryptolab_haetae3_poly_pointwise_montgomery_mixed_addto
#define POLYQ_UNPACK_PWMUL_SYMBOL cryptolab_haetae3_polyq_unpack_pointwise_montgomery
#elif HAETAE_MODE == 5
#define MONT_REDUCE_SYMBOL cryptolab_haetae5_montgomery_reduce
#define MONT_MUL_SYMBOL cryptolab_haetae5_montgomery_mul_i32
#define MONT_MUL_U16_SYMBOL cryptolab_haetae5_montgomery_mul_u16_i32
#define POLY_PWMUL_SYMBOL cryptolab_haetae5_poly_pointwise_montgomery
#define POLY_PWMUL_ADDTO_SYMBOL cryptolab_haetae5_poly_pointwise_montgomery_addto
#define POLY_PWMUL_MIXED_SYMBOL cryptolab_haetae5_poly_pointwise_montgomery_mixed
#define POLY_PWMUL_MIXED_ADDTO_SYMBOL cryptolab_haetae5_poly_pointwise_montgomery_mixed_addto
#define POLYQ_UNPACK_PWMUL_SYMBOL cryptolab_haetae5_polyq_unpack_pointwise_montgomery
#else
#error "unsupported HAETAE_MODE"
#endif

    .syntax unified
    .cpu cortex-m0plus
    .thumb
    .text
    .align 2

/* Signed 32x32 -> Montgomery-reduced result using only low-word multiplies.
 * Input:  r0 = a, r1 = b
 * Output: r0 = montgomery_reduce((int64_t)a * b)
 */
    .thumb_func
    .type m0_pm_mul_reduce, %function
m0_pm_mul_reduce:
    sub     sp, #28
    str     r0, [sp, #0]    @ a
    str     r1, [sp, #4]    @ b

    uxth    r2, r0
    uxth    r3, r1
    muls    r2, r3
    str     r2, [sp, #8]    @ p0 = alo * blo

    ldr     r0, [sp, #0]
    uxth    r2, r0
    ldr     r1, [sp, #4]
    lsrs    r3, r1, #16
    muls    r2, r3
    str     r2, [sp, #12]   @ p1 = alo * bhi

    ldr     r0, [sp, #0]
    lsrs    r2, r0, #16
    ldr     r1, [sp, #4]
    uxth    r3, r1
    muls    r2, r3
    str     r2, [sp, #16]   @ p2 = ahi * blo

    ldr     r0, [sp, #0]
    lsrs    r2, r0, #16
    ldr     r1, [sp, #4]
    lsrs    r3, r1, #16
    muls    r2, r3
    str     r2, [sp, #20]   @ p3 = ahi * bhi

    ldr     r0, [sp, #8]
    lsrs    r1, r0, #16
    ldr     r2, [sp, #12]
    uxth    r3, r2
    adds    r1, r1, r3
    ldr     r2, [sp, #16]
    uxth    r3, r2
    adds    r1, r1, r3

    uxth    r0, r0
    mov     r3, r1
    lsls    r3, r3, #16
    adds    r0, r0, r3

    ldr     r3, [sp, #20]
    ldr     r2, [sp, #12]
    lsrs    r2, r2, #16
    adds    r3, r3, r2
    ldr     r2, [sp, #16]
    lsrs    r2, r2, #16
    adds    r3, r3, r2
    lsrs    r1, r1, #16
    adds    r1, r3, r1

    ldr     r2, [sp, #0]
    cmp     r2, #0
    bge     1f
    ldr     r3, [sp, #4]
    subs    r1, r1, r3
1:
    ldr     r3, [sp, #4]
    cmp     r3, #0
    bge     2f
    ldr     r2, [sp, #0]
    subs    r1, r1, r2
2:
    str     r0, [sp, #0]    @ product low32
    str     r1, [sp, #4]    @ product high32

    ldr     r2, =940508161  @ QINV
    muls    r0, r2          @ t = low32((int32_t)a * QINV)
    str     r0, [sp, #8]

    uxth    r1, r0
    ldr     r2, =64513
    muls    r1, r2
    str     r1, [sp, #12]   @ p0 = tlo * Q

    ldr     r0, [sp, #8]
    asrs    r0, r0, #16
    ldr     r2, =64513
    muls    r0, r2
    str     r0, [sp, #16]   @ p1 = thi * Q

    ldr     r0, [sp, #12]
    lsrs    r1, r0, #16
    ldr     r2, [sp, #16]
    uxth    r3, r2
    adds    r1, r1, r3

    uxth    r0, r0
    mov     r3, r1
    lsls    r3, r3, #16
    adds    r0, r0, r3      @ low32(t * Q)

    asrs    r2, r2, #16
    lsrs    r1, r1, #16
    adds    r1, r1, r2      @ high32(t * Q)

    ldr     r2, [sp, #0]
    ldr     r3, [sp, #4]
    subs    r2, r2, r0
    sbcs    r3, r1
    mov     r0, r3

    add     sp, #28
    bx      lr
    .size m0_pm_mul_reduce, .-m0_pm_mul_reduce

/* uint16_t x int32_t -> Montgomery-reduced result. */
    .thumb_func
    .type m0_pm_u16_mul_reduce, %function
m0_pm_u16_mul_reduce:
    sub     sp, #20
    str     r0, [sp, #0]    @ a, already zero-extended
    str     r1, [sp, #4]    @ b

    uxth    r2, r1
    muls    r2, r0
    str     r2, [sp, #8]    @ p0 = a * blo

    ldr     r1, [sp, #4]
    asrs    r1, r1, #16
    muls    r1, r0
    str     r1, [sp, #12]   @ p1 = a * bhi

    ldr     r0, [sp, #8]
    lsrs    r1, r0, #16
    ldr     r2, [sp, #12]
    uxth    r3, r2
    adds    r1, r1, r3

    uxth    r0, r0
    mov     r3, r1
    lsls    r3, r3, #16
    adds    r0, r0, r3

    asrs    r2, r2, #16
    lsrs    r1, r1, #16
    adds    r1, r1, r2

    str     r0, [sp, #0]    @ product low32
    str     r1, [sp, #4]    @ product high32

    ldr     r2, =940508161  @ QINV
    muls    r0, r2
    str     r0, [sp, #8]

    uxth    r1, r0
    ldr     r2, =64513
    muls    r1, r2
    str     r1, [sp, #12]

    ldr     r0, [sp, #8]
    asrs    r0, r0, #16
    ldr     r2, =64513
    muls    r0, r2
    str     r0, [sp, #16]

    ldr     r0, [sp, #12]
    lsrs    r1, r0, #16
    ldr     r2, [sp, #16]
    uxth    r3, r2
    adds    r1, r1, r3

    uxth    r0, r0
    mov     r3, r1
    lsls    r3, r3, #16
    adds    r0, r0, r3      @ low32(t * Q)

    asrs    r2, r2, #16
    lsrs    r1, r1, #16
    adds    r1, r1, r2      @ high32(t * Q)

    ldr     r2, [sp, #0]
    ldr     r3, [sp, #4]
    subs    r2, r2, r0
    sbcs    r3, r1
    mov     r0, r3

    add     sp, #20
    bx      lr
    .size m0_pm_u16_mul_reduce, .-m0_pm_u16_mul_reduce

/* int32_t montgomery_reduce(int64_t a); */
#if !defined(HAETAE_FORCE_C_MONTGOMERY_REDUCE)
    .global MONT_REDUCE_SYMBOL
    .thumb_func
    .type MONT_REDUCE_SYMBOL, %function
MONT_REDUCE_SYMBOL:
    sub     sp, #24
    str     r0, [sp, #0]    @ original low32
    str     r1, [sp, #4]    @ original high32

    ldr     r2, =940508161  @ QINV
    muls    r0, r2          @ t = low32((int32_t)a * QINV)
    str     r0, [sp, #8]

    uxth    r1, r0
    ldr     r2, =64513
    muls    r1, r2
    str     r1, [sp, #12]   @ p0 = tlo * Q

    ldr     r0, [sp, #8]
    asrs    r0, r0, #16
    ldr     r2, =64513
    muls    r0, r2
    str     r0, [sp, #16]   @ p1 = thi * Q

    ldr     r0, [sp, #12]
    lsrs    r1, r0, #16
    ldr     r2, [sp, #16]
    uxth    r3, r2
    adds    r1, r1, r3

    uxth    r0, r0
    mov     r3, r1
    lsls    r3, r3, #16
    adds    r0, r0, r3      @ low32(t * Q)

    asrs    r2, r2, #16
    lsrs    r1, r1, #16
    adds    r1, r1, r2      @ high32(t * Q)

    ldr     r2, [sp, #0]
    ldr     r3, [sp, #4]
    subs    r2, r2, r0
    sbcs    r3, r1
    mov     r0, r3

    add     sp, #24
    bx      lr
    .size MONT_REDUCE_SYMBOL, .-MONT_REDUCE_SYMBOL
#endif

#if !defined(HAETAE_FORCE_C_POINTMUL)
/* int32_t montgomery_mul_i32(int32_t a, int32_t b); */
    .global MONT_MUL_SYMBOL
    .thumb_func
    .type MONT_MUL_SYMBOL, %function
MONT_MUL_SYMBOL:
    b       m0_pm_mul_reduce
    .size MONT_MUL_SYMBOL, .-MONT_MUL_SYMBOL

/* int32_t montgomery_mul_u16_i32(uint16_t a, int32_t b); */
    .global MONT_MUL_U16_SYMBOL
    .thumb_func
    .type MONT_MUL_U16_SYMBOL, %function
MONT_MUL_U16_SYMBOL:
    uxth    r0, r0
    b       m0_pm_u16_mul_reduce
    .size MONT_MUL_U16_SYMBOL, .-MONT_MUL_U16_SYMBOL

    .ltorg

/* void poly_pointwise_montgomery(poly *c, const poly *a, const poly *b); */
    .global POLY_PWMUL_SYMBOL
    .thumb_func
    .type POLY_PWMUL_SYMBOL, %function
POLY_PWMUL_SYMBOL:
    push    {r4-r7, lr}
    sub     sp, #12
    mov     r4, r0
    mov     r5, r1
    mov     r6, r2
    movs    r7, #1
    lsls    r7, r7, #8

.Lpwmul_loop:
    ldr     r0, [r5]
    adds    r5, r5, #4
    ldr     r1, [r6]
    adds    r6, r6, #4
    bl      m0_pm_mul_reduce
    str     r0, [r4]
    adds    r4, r4, #4
    subs    r7, r7, #1
    bne     .Lpwmul_loop

    add     sp, #12
    pop     {r4-r7, pc}
    .size POLY_PWMUL_SYMBOL, .-POLY_PWMUL_SYMBOL

/* void poly_pointwise_montgomery_addto(poly *c, const poly *a, const poly *b); */
    .global POLY_PWMUL_ADDTO_SYMBOL
    .thumb_func
    .type POLY_PWMUL_ADDTO_SYMBOL, %function
POLY_PWMUL_ADDTO_SYMBOL:
    push    {r4-r7, lr}
    sub     sp, #12
    mov     r4, r0
    mov     r5, r1
    mov     r6, r2
    movs    r7, #1
    lsls    r7, r7, #8

.Lpwmul_addto_loop:
    ldr     r0, [r5]
    adds    r5, r5, #4
    ldr     r1, [r6]
    adds    r6, r6, #4
    bl      m0_pm_mul_reduce
    ldr     r1, [r4]
    adds    r0, r0, r1
    str     r0, [r4]
    adds    r4, r4, #4
    subs    r7, r7, #1
    bne     .Lpwmul_addto_loop

    add     sp, #12
    pop     {r4-r7, pc}
    .size POLY_PWMUL_ADDTO_SYMBOL, .-POLY_PWMUL_ADDTO_SYMBOL

#if FROZEN_A >= 1
/* void poly_pointwise_montgomery_mixed(poly *c, const poly_frozen *a,
 *                                      const poly *b);
 */
    .global POLY_PWMUL_MIXED_SYMBOL
    .thumb_func
    .type POLY_PWMUL_MIXED_SYMBOL, %function
POLY_PWMUL_MIXED_SYMBOL:
    push    {r4-r7, lr}
    sub     sp, #12
    mov     r4, r0
    mov     r5, r1
    mov     r6, r2
    movs    r7, #1
    lsls    r7, r7, #8

.Lpwmul_mixed_loop:
    ldrh    r0, [r5]
    adds    r5, r5, #2
    ldr     r1, [r6]
    adds    r6, r6, #4
    bl      m0_pm_u16_mul_reduce
    str     r0, [r4]
    adds    r4, r4, #4
    subs    r7, r7, #1
    bne     .Lpwmul_mixed_loop

    add     sp, #12
    pop     {r4-r7, pc}
    .size POLY_PWMUL_MIXED_SYMBOL, .-POLY_PWMUL_MIXED_SYMBOL

/* void poly_pointwise_montgomery_mixed_addto(poly *c,
 *                                            const poly_frozen *a,
 *                                            const poly *b);
 */
    .global POLY_PWMUL_MIXED_ADDTO_SYMBOL
    .thumb_func
    .type POLY_PWMUL_MIXED_ADDTO_SYMBOL, %function
POLY_PWMUL_MIXED_ADDTO_SYMBOL:
    push    {r4-r7, lr}
    sub     sp, #12
    mov     r4, r0
    mov     r5, r1
    mov     r6, r2
    movs    r7, #1
    lsls    r7, r7, #8

.Lpwmul_mixed_addto_loop:
    ldrh    r0, [r5]
    adds    r5, r5, #2
    ldr     r1, [r6]
    adds    r6, r6, #4
    bl      m0_pm_u16_mul_reduce
    ldr     r1, [r4]
    adds    r0, r0, r1
    str     r0, [r4]
    adds    r4, r4, #4
    subs    r7, r7, #1
    bne     .Lpwmul_mixed_addto_loop

    add     sp, #12
    pop     {r4-r7, pc}
    .size POLY_PWMUL_MIXED_ADDTO_SYMBOL, .-POLY_PWMUL_MIXED_ADDTO_SYMBOL
#endif

#if D == 0
/* void polyq_unpack_pointwise_montgomery(poly *dest, const uint8_t *a,
 *                                        const poly *src_hat);
 */
    .global POLYQ_UNPACK_PWMUL_SYMBOL
    .thumb_func
    .type POLYQ_UNPACK_PWMUL_SYMBOL, %function
POLYQ_UNPACK_PWMUL_SYMBOL:
    push    {r4-r7, lr}
    sub     sp, #12
    mov     r4, r0
    mov     r5, r1
    mov     r6, r2
    movs    r7, #1
    lsls    r7, r7, #8

.Lpolyq_unpack_pwmul_loop:
    ldrb    r0, [r5]
    ldrb    r1, [r5, #1]
    lsls    r1, r1, #8
    adds    r0, r0, r1
    adds    r5, r5, #2
    ldr     r1, [r6]
    adds    r6, r6, #4
    bl      m0_pm_u16_mul_reduce
    str     r0, [r4]
    adds    r4, r4, #4
    subs    r7, r7, #1
    bne     .Lpolyq_unpack_pwmul_loop

    add     sp, #12
    pop     {r4-r7, pc}
    .size POLYQ_UNPACK_PWMUL_SYMBOL, .-POLYQ_UNPACK_PWMUL_SYMBOL
#endif
#endif

    .ltorg

#endif
