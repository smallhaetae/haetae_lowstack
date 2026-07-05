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
 * ntt_asm.S - readable AVR-GCC assembly NTT for HAETAE Barrett C layout.
 *
 * ABI:
 *   void ntt(int32_t a[256]);
 *   void invntt_tomont(int32_t a[256]);
 *
 * Coefficients stay as int32_t words.  The Barrett zeta table is provided by
 * ntt.c as:
 *
 *   const int32_t zetas_barrett[256][2] PROGMEM;
 *
 * Each table entry is zeta || bp, both signed int32_t little-endian.
 */

#include "config.h"

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

__SP_H__ = 0x3e
__SP_L__ = 0x3d
__SREG__ = 0x3f
__tmp_reg__ = 0
__zero_reg__ = 1

.text

.extern zetas_barrett
/* Callee-saved register allocation used by both transforms:
 *   r2:r3    base pointer to a[0]
 *   r4:r5    j pointer
 *   r6:r7    j+len pointer
 *   r8:r9    end pointer
 *   r10-r13  zeta
 *   r14-r17  bp
 *
 * Stack frame:
 *   Y+1:Y+2     zeta table pointer
 *   Y+3:Y+4     len in bytes
 *   Y+5:Y+6     start in bytes
 *   Y+7..Y+10   temporary int32_t
 *   Y+11..Y+14  saved input coefficient for inline Barrett multiply
 */

.macro ALLOC_FRAME bytes
    in      r28, __SP_L__
    in      r29, __SP_H__
    sbiw    r28, \bytes
    in      __tmp_reg__, __SREG__
    cli
    out     __SP_H__, r29
    out     __SREG__, __tmp_reg__
    out     __SP_L__, r28
.endm

.macro FREE_FRAME bytes
    adiw    r28, \bytes
    in      __tmp_reg__, __SREG__
    cli
    out     __SP_H__, r29
    out     __SREG__, __tmp_reg__
    out     __SP_L__, r28
.endm

.macro ADDI16 lo, hi, imm
    movw    r30, \lo
    subi    r30, lo8(-(\imm))
    sbci    r31, hi8(-(\imm))
    movw    \lo, r30
.endm

.macro SUBI16 lo, hi, imm
    movw    r30, \lo
    subi    r30, lo8(\imm)
    sbci    r31, hi8(\imm)
    movw    \lo, r30
.endm

.macro LOAD32 ptrlo, d0, d1, d2, d3
    movw    r30, \ptrlo
    ld      \d0, Z+
    ld      \d1, Z+
    ld      \d2, Z+
    ld      \d3, Z+
.endm

.macro STORE32 ptrlo, s0, s1, s2, s3
    movw    r30, \ptrlo
    st      Z+, \s0
    st      Z+, \s1
    st      Z+, \s2
    st      Z+, \s3
.endm

.macro LOAD_CT_ZETA
    ldd     r30, Y+1
    ldd     r31, Y+2
    lpm     r10, Z+
    lpm     r11, Z+
    lpm     r12, Z+
    lpm     r13, Z+
    lpm     r14, Z+
    lpm     r15, Z+
    lpm     r16, Z+
    lpm     r17, Z+
    std     Y+1, r30
    std     Y+2, r31
.endm

.macro NEG32 a0, a1, a2, a3
    com     \a0
    com     \a1
    com     \a2
    com     \a3
    ldi     r18, 1
    add     \a0, r18
    adc     \a1, __zero_reg__
    adc     \a2, __zero_reg__
    adc     \a3, __zero_reg__
.endm

.macro LOAD_GS_ZETA
    ldd     r30, Y+1
    ldd     r31, Y+2
    SUBI16  r30, r31, 8
    std     Y+1, r30
    std     Y+2, r31
    lpm     r10, Z+
    lpm     r11, Z+
    lpm     r12, Z+
    lpm     r13, Z+
    lpm     r14, Z+
    lpm     r15, Z+
    lpm     r16, Z+
    lpm     r17, Z+
    NEG32   r10, r11, r12, r13
    NEG32   r14, r15, r16, r17
.endm

.macro MULS16X16_32 a0, a1, b0, b1, c0, c1, c2, c3
    muls    \a1, \b1
    movw    \c2, r0
    mul     \a0, \b0
    movw    \c0, r0
    mulsu   \a1, \b0
    sbc     \c3, r26
    add     \c1, r0
    adc     \c2, r1
    adc     \c3, r26
    mulsu   \b1, \a0
    sbc     \c3, r26
    add     \c1, r0
    adc     \c2, r1
    adc     \c3, r26
.endm

.macro MULSU16X16_32 a0, a1, b0, b1, c0, c1, c2, c3
    mulsu   \a1, \b1
    movw    \c2, r0
    mul     \a0, \b0
    movw    \c0, r0
    mulsu   \a1, \b0
    sbc     \c3, r26
    add     \c1, r0
    adc     \c2, r1
    adc     \c3, r26
    mul     \b1, \a0
    add     \c1, r0
    adc     \c2, r1
    adc     \c3, r26
.endm

.macro MUL32X32_LO32_U a0, a1, a2, a3, b0, b1, b2, b3, c0, c1, c2, c3
    mul     \a0, \b0
    movw    \c0, r0
    mul     \a2, \b0
    movw    \c2, r0
    mul     \a1, \b0
    add     \c1, r0
    adc     \c2, r1
    adc     \c3, r26
    mul     \a0, \b1
    add     \c1, r0
    adc     \c2, r1
    adc     \c3, r26
    mul     \a1, \b1
    add     \c2, r0
    adc     \c3, r1
    mul     \a0, \b2
    add     \c2, r0
    adc     \c3, r1
    mul     \a3, \b0
    add     \c3, r0
    mul     \a2, \b1
    add     \c3, r0
    mul     \a1, \b2
    add     \c3, r0
    mul     \a0, \b3
    add     \c3, r0
.endm

.macro ADD_CROSS_HIGH_TO_FRAME c2, c3
    mov     r27, \c3
    lsl     r27
    sbc     r27, r27

    ldd     r30, Y+7
    ldd     r31, Y+8
    add     r30, \c2
    adc     r31, \c3
    std     Y+7, r30
    std     Y+8, r31

    ldd     r30, Y+9
    ldd     r31, Y+10
    adc     r30, r27
    adc     r31, r27
    std     Y+9, r30
    std     Y+10, r31
.endm

.macro MUL32_Q64513_LO32_FROM_FRAME
    ldd     r18, Y+7
    ldd     r19, Y+8
    ldd     r20, Y+9
    ldd     r21, Y+10

    ldi     r27, 0xfc
    mul     r20, r27
    add     r21, r0
    mul     r19, r27
    add     r20, r0
    adc     r21, r1
    mul     r18, r27
    add     r19, r0
    adc     r20, r1
    adc     r21, r26

    std     Y+7, r18
    std     Y+8, r19
    std     Y+9, r20
    std     Y+10, r21
.endm

.macro BARRETT_MUL32_A22
    /* Input:
     *   r22-r25 = a
     *   r10-r13 = zeta
     *   r14-r17 = bp = round(zeta * 2^32 / Q)
     * Output:
     *   r22-r25 = low32(a * zeta) - low32(high32(a * bp) * Q)
     */
    clr     r26
    std     Y+11, r22
    std     Y+12, r23
    std     Y+13, r24
    std     Y+14, r25

    /* hi ~= high32(a * bp), using the 16-bit decomposition from the
     * previous Barrett AVR implementation.  A one-unit difference in hi only
     * changes the result by a multiple of Q.
     */
    mov     r18, r24
    mov     r19, r25
    MULS16X16_32   r18, r19, r16, r17, r20, r21, r22, r23
    std     Y+7, r20
    std     Y+8, r21
    std     Y+9, r22
    std     Y+10, r23

    ldd     r18, Y+11
    ldd     r19, Y+12
    MULSU16X16_32  r16, r17, r18, r19, r20, r21, r22, r23
    ADD_CROSS_HIGH_TO_FRAME r22, r23

    ldd     r18, Y+13
    ldd     r19, Y+14
    mov     r20, r14
    mov     r21, r15
    MULSU16X16_32  r18, r19, r20, r21, r22, r23, r24, r25
    ADD_CROSS_HIGH_TO_FRAME r24, r25

    MUL32_Q64513_LO32_FROM_FRAME

    ldd     r22, Y+11
    ldd     r23, Y+12
    ldd     r24, Y+13
    ldd     r25, Y+14
    MUL32X32_LO32_U r22, r23, r24, r25, r10, r11, r12, r13, r18, r19, r20, r21

    ldd     r30, Y+7
    sub     r18, r30
    ldd     r30, Y+8
    sbc     r19, r30
    ldd     r30, Y+9
    sbc     r20, r30
    ldd     r30, Y+10
    sbc     r21, r30
    movw    r22, r18
    movw    r24, r20
    clr     __zero_reg__
.endm

.macro BARRETT_FREEZE_R22
    /* Input/output: r22-r25.
     * Specialized for final iNTT values after multiplication by
     * INV_N_MONT_CENTERED.  It matches the C Barrett freeze over the range
     * reached by the transform and avoids 256 helper calls.
     */
    clr     r26
    std     Y+11, r22
    std     Y+12, r23
    std     Y+13, r24
    std     Y+14, r25

    /* qrec = 0x0001040f.  high32(a*qrec) =
     * sign_extend(a_hi) + high16(a_hi * 0x040f) for the final-pass range.
     */
    std     Y+7, r24
    std     Y+8, r25
    mov     r18, r25
    lsl     r18
    sbc     r18, r18
    std     Y+9, r18
    std     Y+10, r18

    mov     r18, r24
    mov     r19, r25
    ldi     r20, 0x0f
    ldi     r21, 0x04
    MULSU16X16_32  r18, r19, r20, r21, r22, r23, r24, r25
    ADD_CROSS_HIGH_TO_FRAME r24, r25

    MUL32_Q64513_LO32_FROM_FRAME

    ldd     r22, Y+11
    ldd     r23, Y+12
    ldd     r24, Y+13
    ldd     r25, Y+14
    ldd     r18, Y+7
    sub     r22, r18
    ldd     r18, Y+8
    sbc     r23, r18
    ldd     r18, Y+9
    sbc     r24, r18
    ldd     r18, Y+10
    sbc     r25, r18

    sbrs    r25, 7
    rjmp    .Lfreeze_nonnegative\@
    ldi     r18, 0x02
    add     r22, r18
    ldi     r18, 0xf8
    adc     r23, r18
    ldi     r18, 0x01
    adc     r24, r18
    clr     r18
    adc     r25, r18

.Lfreeze_nonnegative\@:
    cpi     r22, 0x01
    ldi     r18, 0xfc
    cpc     r23, r18
    clr     r18
    cpc     r24, r18
    cpc     r25, r18
    brlo    .Lfreeze_done\@
    subi    r22, 0x01
    sbci    r23, 0xfc
    sbci    r24, 0x00
    sbci    r25, 0x00

.Lfreeze_done\@:
    clr     __zero_reg__
.endm

.macro SAVE_REGS
    push    r2
    push    r3
    push    r4
    push    r5
    push    r6
    push    r7
    push    r8
    push    r9
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
    pop     r9
    pop     r8
    pop     r7
    pop     r6
    pop     r5
    pop     r4
    pop     r3
    pop     r2
.endm

.global NTT_SYMBOL
.type NTT_SYMBOL, @function
NTT_SYMBOL:
    SAVE_REGS
    ALLOC_FRAME 14

    movw    r2, r24

    ldi     r30, lo8(zetas_barrett + 8)
    ldi     r31, hi8(zetas_barrett + 8)
    std     Y+1, r30
    std     Y+2, r31

    ldi     r18, lo8(512)
    ldi     r19, hi8(512)
    std     Y+3, r18
    std     Y+4, r19

.Lntt_layer:
    std     Y+5, __zero_reg__
    std     Y+6, __zero_reg__

.Lntt_block:
    LOAD_CT_ZETA

    ldd     r4, Y+5
    ldd     r5, Y+6
    add     r4, r2
    adc     r5, r3

    movw    r6, r4
    ldd     r18, Y+3
    ldd     r19, Y+4
    add     r6, r18
    adc     r7, r19
    movw    r8, r6

.Lntt_inner:
    LOAD32  r6, r22, r23, r24, r25
    BARRETT_MUL32_A22

    LOAD32  r4, r18, r19, r20, r21
    sub     r18, r22
    sbc     r19, r23
    sbc     r20, r24
    sbc     r21, r25
    STORE32 r6, r18, r19, r20, r21

    LOAD32  r4, r18, r19, r20, r21
    add     r18, r22
    adc     r19, r23
    adc     r20, r24
    adc     r21, r25
    STORE32 r4, r18, r19, r20, r21

    ADDI16  r4, r5, 4
    ADDI16  r6, r7, 4
    cp      r4, r8
    cpc     r5, r9
    breq    .Lntt_inner_done
    rjmp    .Lntt_inner
.Lntt_inner_done:

    ldd     r18, Y+5
    ldd     r19, Y+6
    ldd     r20, Y+3
    ldd     r21, Y+4
    add     r18, r20
    adc     r19, r21
    add     r18, r20
    adc     r19, r21
    std     Y+5, r18
    std     Y+6, r19
    cpi     r19, hi8(1024)
    brsh    .Lntt_blocks_done
    rjmp    .Lntt_block

.Lntt_blocks_done:
    ldd     r18, Y+3
    ldd     r19, Y+4
    lsr     r19
    ror     r18
    std     Y+3, r18
    std     Y+4, r19
    tst     r19
    brne    .Lntt_next_layer
    cpi     r18, 4
    brlo    .Lntt_done
.Lntt_next_layer:
    rjmp    .Lntt_layer

.Lntt_done:
    FREE_FRAME 14
    clr     __zero_reg__
    RESTORE_REGS
    ret
.size NTT_SYMBOL, .-NTT_SYMBOL

.global INVNTT_SYMBOL
.type INVNTT_SYMBOL, @function
INVNTT_SYMBOL:
    SAVE_REGS
    ALLOC_FRAME 14

    movw    r2, r24

    ldi     r30, lo8(zetas_barrett + 2048)
    ldi     r31, hi8(zetas_barrett + 2048)
    std     Y+1, r30
    std     Y+2, r31

    ldi     r18, lo8(4)
    ldi     r19, hi8(4)
    std     Y+3, r18
    std     Y+4, r19

.Lintt_layer:
    std     Y+5, __zero_reg__
    std     Y+6, __zero_reg__

.Lintt_block:
    LOAD_GS_ZETA

    ldd     r4, Y+5
    ldd     r5, Y+6
    add     r4, r2
    adc     r5, r3

    movw    r6, r4
    ldd     r18, Y+3
    ldd     r19, Y+4
    add     r6, r18
    adc     r7, r19
    movw    r8, r6

.Lintt_inner:
    LOAD32  r4, r18, r19, r20, r21
    LOAD32  r6, r22, r23, r24, r25

    sub     r18, r22
    sbc     r19, r23
    sbc     r20, r24
    sbc     r21, r25
    std     Y+7, r18
    std     Y+8, r19
    std     Y+9, r20
    std     Y+10, r21

    LOAD32  r4, r18, r19, r20, r21
    LOAD32  r6, r22, r23, r24, r25
    add     r18, r22
    adc     r19, r23
    adc     r20, r24
    adc     r21, r25
    STORE32 r4, r18, r19, r20, r21

    ldd     r22, Y+7
    ldd     r23, Y+8
    ldd     r24, Y+9
    ldd     r25, Y+10
    BARRETT_MUL32_A22
    STORE32 r6, r22, r23, r24, r25

    ADDI16  r4, r5, 4
    ADDI16  r6, r7, 4
    cp      r4, r8
    cpc     r5, r9
    breq    .Lintt_inner_done
    rjmp    .Lintt_inner
.Lintt_inner_done:

    ldd     r18, Y+5
    ldd     r19, Y+6
    ldd     r20, Y+3
    ldd     r21, Y+4
    add     r18, r20
    adc     r19, r21
    add     r18, r20
    adc     r19, r21
    std     Y+5, r18
    std     Y+6, r19
    cpi     r19, hi8(1024)
    brsh    .Lintt_blocks_done
    rjmp    .Lintt_block

.Lintt_blocks_done:
    ldd     r18, Y+3
    ldd     r19, Y+4
    lsl     r18
    rol     r19
    std     Y+3, r18
    std     Y+4, r19
    cpi     r19, hi8(1024)
    brsh    .Lintt_done_layers
    rjmp    .Lintt_layer

.Lintt_done_layers:
    movw    r4, r2
    movw    r6, r2
    ADDI16  r6, r7, 1024

.Lintt_final:
    LOAD32  r4, r22, r23, r24, r25

    ldi     r30, 0xfc
    mov     r10, r30
    ldi     r30, 0x0e
    mov     r11, r30
    clr     r12
    clr     r13

    ldi     r30, 0x18
    mov     r14, r30
    ldi     r30, 0xd4
    mov     r15, r30
    ldi     r16, 0x38
    ldi     r17, 0x0f
    BARRETT_MUL32_A22
    BARRETT_FREEZE_R22

    STORE32 r4, r22, r23, r24, r25
    ADDI16  r4, r5, 4
    cp      r4, r6
    cpc     r5, r7
    breq    .Lintt_final_done
    rjmp    .Lintt_final

.Lintt_final_done:
    FREE_FRAME 14
    clr     __zero_reg__
    RESTORE_REGS
    ret
.size INVNTT_SYMBOL, .-INVNTT_SYMBOL

#undef NTT_SYMBOL
#undef INVNTT_SYMBOL
