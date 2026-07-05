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
 * ntt_asm.S - Cortex-M0+ Barrett NTT/iNTT for HAETAE.
 *
 * This is the correctness-first M0+ version.  It keeps the same loop shape as
 * the C Barrett implementation and uses only Thumb-1 integer operations plus
 * the M0+ low-word multiply instruction.
 */

#include "config.h"

#if (defined(__arm__) || defined(__thumb__)) && !defined(HAETAE_FORCE_C_NTT)

#if HAETAE_MODE == 2
#define NTT_SYMBOL cryptolab_haetae2_ntt
#define INVNTT_SYMBOL cryptolab_haetae2_invntt_tomont
#elif HAETAE_MODE == 3
#define NTT_SYMBOL cryptolab_haetae3_ntt
#define INVNTT_SYMBOL cryptolab_haetae3_invntt_tomont
#elif HAETAE_MODE == 5
#define NTT_SYMBOL cryptolab_haetae5_ntt
#define INVNTT_SYMBOL cryptolab_haetae5_invntt_tomont
#else
#error "unsupported HAETAE_MODE"
#endif

    .syntax unified
    .cpu cortex-m0plus
    .thumb
    .text
    .align 2

    .extern zetas_barrett

/* Barrett multiply using the AVR/TCHES high-word approximation.
 * Input/output: r0 = a/result.
 * Stack contract:
 *   [sp, #0]  = zeta
 *   [sp, #4]  = bp
 *   [sp, #28] = scratch a
 *   [sp, #32] = scratch hi
 *   [sp, #40] = Q
 */
    .macro M0_NTT_BARRETT_MUL_INLINE
    str     r0, [sp, #28]   @ a
    ldr     r2, [sp, #4]    @ bp
    asrs    r3, r0, #16     @ ahi
    asrs    r1, r2, #16     @ bphi
    muls    r1, r3          @ ahi * bphi
    str     r1, [sp, #32]   @ hi accumulator

    uxth    r1, r0          @ alo
    asrs    r2, r2, #16     @ bphi
    muls    r1, r2          @ alo * bphi
    asrs    r1, r1, #16
    ldr     r3, [sp, #32]
    adds    r3, r3, r1

    ldr     r0, [sp, #28]
    asrs    r1, r0, #16     @ ahi
    ldr     r2, [sp, #4]
    uxth    r2, r2          @ bplo
    muls    r1, r2          @ ahi * bplo
    asrs    r1, r1, #16
    adds    r3, r3, r1      @ approximate high32(a * bp)

    ldr     r2, [sp, #40]
    muls    r3, r2          @ hi * q, low32

    ldr     r0, [sp, #28]
    ldr     r1, [sp, #0]
    muls    r0, r1          @ a * zeta, low32
    subs    r0, r0, r3
    .endm

/* Final iNTT canonicalization matching reduce.c:freeze().
 * Input/output: r0 = a/result.
 * Stack contract:
 *   [sp, #28] = scratch a
 *   [sp, #36] = 1039
 *   [sp, #40] = Q
 */
    .macro M0_NTT_BARRETT_FREEZE_INLINE
    str     r0, [sp, #28]
    asrs    r1, r0, #16     @ ahi; QREC = 1 * 2^16 + 1039
    mov     r2, r1
    ldr     r3, [sp, #36]
    muls    r2, r3
    asrs    r2, r2, #16
    adds    r1, r1, r2      @ approximate high32(a * QREC)

    ldr     r2, [sp, #40]
    muls    r1, r2
    ldr     r0, [sp, #28]
    subs    r0, r0, r1      @ a - t*q

    cmp     r0, #0
    bge     .Lfreeze_nonnegative\@
    ldr     r1, [sp, #40]
    adds    r0, r0, r1
    adds    r0, r0, r1
.Lfreeze_nonnegative\@:
    ldr     r1, [sp, #40]
    cmp     r0, r1
    blo     .Lfreeze_done\@
    subs    r0, r0, r1
.Lfreeze_done\@:
    .endm

    .ltorg

/* void ntt(int32_t a[256]); */
    .global NTT_SYMBOL
    .thumb_func
    .type NTT_SYMBOL, %function
NTT_SYMBOL:
    push    {r4-r7, lr}
    sub     sp, #44

    mov     r4, r0
    ldr     r0, =64513
    str     r0, [sp, #40]
    ldr     r7, =zetas_barrett
    adds    r7, r7, #8      @ skip table entry 0
    movs    r5, #128        @ len

.Lntt_len:
    movs    r6, #0          @ start / j

.Lntt_start:
    ldr     r0, [r7, #0]
    str     r0, [sp, #0]    @ zeta
    ldr     r0, [r7, #4]
    str     r0, [sp, #4]    @ bp
    adds    r7, r7, #8

    mov     r0, r6
    adds    r0, r0, r5
    str     r0, [sp, #16]   @ end = start + len
    adds    r0, r0, r5
    str     r0, [sp, #12]   @ next_start = start + 2*len

.Lntt_j:
    ldr     r0, [sp, #16]
    cmp     r6, r0
    bhs     .Lntt_j_done

    mov     r2, r6
    lsls    r2, r2, #2
    adds    r2, r4, r2      @ &a[j]
    str     r2, [sp, #20]
    mov     r3, r5
    lsls    r3, r3, #2
    adds    r3, r2, r3      @ &a[j + len]
    str     r3, [sp, #24]

    ldr     r2, [r2]
    str     r2, [sp, #8]    @ old a[j]
    ldr     r0, [r3]
    M0_NTT_BARRETT_MUL_INLINE

    mov     r3, r0
    ldr     r2, [sp, #8]
    adds    r0, r2, r3
    ldr     r1, [sp, #20]
    str     r0, [r1]
    subs    r0, r2, r3
    ldr     r1, [sp, #24]
    str     r0, [r1]

    adds    r6, r6, #1
    b       .Lntt_j

.Lntt_j_done:
    ldr     r6, [sp, #12]
    movs    r0, #1
    lsls    r0, r0, #8      @ 256
    cmp     r6, r0
    blo     .Lntt_start

    lsrs    r5, r5, #1
    bne     .Lntt_len

    add     sp, #44
    pop     {r4-r7, pc}
    .size NTT_SYMBOL, .-NTT_SYMBOL

    .ltorg

/* void invntt_tomont(int32_t a[256]); */
    .global INVNTT_SYMBOL
    .thumb_func
    .type INVNTT_SYMBOL, %function
INVNTT_SYMBOL:
    push    {r4-r7, lr}
    sub     sp, #44

    mov     r4, r0
    ldr     r0, =1039
    str     r0, [sp, #36]
    ldr     r0, =64513
    str     r0, [sp, #40]
    ldr     r7, =zetas_barrett
    movs    r0, #1
    lsls    r0, r0, #11     @ 256 entries * 8 bytes
    adds    r7, r7, r0
    movs    r5, #1          @ len

.Lintt_len:
    movs    r6, #0          @ start / j

.Lintt_start:
    subs    r7, r7, #8
    ldr     r0, [r7, #0]
    negs    r0, r0
    str     r0, [sp, #0]    @ -zeta
    ldr     r0, [r7, #4]
    negs    r0, r0
    str     r0, [sp, #4]    @ -bp

    mov     r0, r6
    adds    r0, r0, r5
    str     r0, [sp, #16]   @ end = start + len
    adds    r0, r0, r5
    str     r0, [sp, #12]   @ next_start = start + 2*len

.Lintt_j:
    ldr     r0, [sp, #16]
    cmp     r6, r0
    bhs     .Lintt_j_done

    mov     r2, r6
    lsls    r2, r2, #2
    adds    r2, r4, r2      @ &a[j]
    str     r2, [sp, #20]
    mov     r3, r5
    lsls    r3, r3, #2
    adds    r3, r2, r3      @ &a[j + len]
    str     r3, [sp, #24]

    ldr     r0, [r2]        @ t
    ldr     r1, [r3]
    adds    r2, r0, r1
    ldr     r3, [sp, #20]
    str     r2, [r3]
    subs    r0, r0, r1
    M0_NTT_BARRETT_MUL_INLINE
    ldr     r1, [sp, #24]
    str     r0, [r1]

    adds    r6, r6, #1
    b       .Lintt_j

.Lintt_j_done:
    ldr     r6, [sp, #12]
    movs    r0, #1
    lsls    r0, r0, #8      @ 256
    cmp     r6, r0
    blo     .Lintt_start

    lsls    r5, r5, #1
    movs    r0, #1
    lsls    r0, r0, #8
    cmp     r5, r0
    blo     .Lintt_len

    movs    r6, #0
.Lintt_final:
    movs    r1, #1
    lsls    r1, r1, #8
    cmp     r6, r1
    bhs     .Lintt_done

    mov     r1, r6
    lsls    r1, r1, #2
    adds    r1, r4, r1
    str     r1, [sp, #20]
    ldr     r0, [r1]
    ldr     r1, =3836
    str     r1, [sp, #0]
    ldr     r1, =255382552
    str     r1, [sp, #4]
    M0_NTT_BARRETT_MUL_INLINE
    M0_NTT_BARRETT_FREEZE_INLINE
    ldr     r1, [sp, #20]
    str     r0, [r1]

    adds    r6, r6, #1
    b       .Lintt_final

.Lintt_done:
    add     sp, #44
    pop     {r4-r7, pc}
    .size INVNTT_SYMBOL, .-INVNTT_SYMBOL

    .ltorg

#endif
