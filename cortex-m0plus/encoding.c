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
#include "encoding.h"
#include "params.h"
#include "rans_byte.h"
#include "platform.h"
#include "const.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>


/* Struct access helpers for the rANS symbol tables.
 * READ_STRUCT reads a const struct from the table (a direct read on the
 * M0+; a PROGMEM copy on Harvard targets such as AVR).
 */
#define RANS_ENC_PUT(rans_p, ptr_p, arr, idx) \
    do { RansEncSymbol _esym; READ_STRUCT(_esym, &(arr)[idx], RansEncSymbol); \
         RansEncPutSymbol((rans_p), (ptr_p), &_esym); } while(0)
#define RANS_DEC_ADV(rans_p, ptr_p, end_p, arr, idx, bits) \
    do { RansDecSymbol _dsym; READ_STRUCT(_dsym, &(arr)[idx], RansDecSymbol); \
         RansDecAdvanceSymbol((rans_p), (ptr_p), (end_p), &_dsym, (bits)); } while(0)

#define H_CUT ((M_H -1) >>1)

/*************************************************
 * Name:        encode_h
 *
 * Description: rANS encode polynomial vector h
 *
 * Arguments:   - uint8_t *buf: pointer to output buffer
 *              - const int32_t *h: pointer to polynomial vector h
 **************************************************/
uint16_t encode_h(uint8_t *buf, size_t cap, const int32_t *h) {
    size_t size_encoded;
    RansState rans;
    uint8_t *ptr;
    size_t size_h = N * K;
    uint32_t tmp;
    uint8_t s;
    uint8_t encoding[size_h]; // size_h is a loose upper bound

    RansEncInit(&rans);
    ptr = encoding + size_h; // end of encoding buffer

    for (size_t i = size_h; i > 0; i--) {
        tmp = h[i - 1];
        // check for very unlikely values that we do not encode
        // to make the encoding cheaper :
        if(H_CUT < tmp && tmp <= H_CUT + OFFSET_H) { 
            return 0;
        } 
        // map the upper likely part of symbols next to the lower part
        // to have a dense and compact distribution: 
        tmp = (tmp > (H_CUT + OFFSET_H)) ? tmp - OFFSET_H : tmp;
        s = (uint8_t) tmp;

        RANS_ENC_PUT(&rans, &ptr, esyms_h, s);
        // check that at least 4 byte remain for memory safety:
        if (ptr < encoding + 4)
        {
            return 0;
        }
    }

    RansEncFlush(&rans, &ptr);

    size_encoded = encoding + size_h - ptr;
    if (size_encoded > cap) return 0;
    memcpy(buf, ptr, size_encoded);
    return size_encoded;
}

/*************************************************
 * Name:        encode_h_u16
 *
 * Description: rANS encode hint from compact uint16_t array
 *              (identical logic to encode_h but reads uint16_t instead of int32_t)
 **************************************************/
uint16_t encode_h_u16(uint8_t *buf, size_t cap, const uint16_t *h) {
    size_t size_encoded;
    RansState rans;
    uint8_t *ptr;
    size_t size_h = N * K;
    uint32_t tmp;
    uint8_t s;
    uint8_t encoding[size_h];

    RansEncInit(&rans);
    ptr = encoding + size_h;

    for (size_t i = size_h; i > 0; i--) {
        tmp = (uint32_t)h[i - 1];
        if(H_CUT < tmp && tmp <= H_CUT + OFFSET_H) {
            return 0;
        }
        tmp = (tmp > (H_CUT + OFFSET_H)) ? tmp - OFFSET_H : tmp;
        s = (uint8_t) tmp;

        RANS_ENC_PUT(&rans, &ptr, esyms_h, s);
        if (ptr < encoding + 4)
        {
            return 0;
        }
    }

    RansEncFlush(&rans, &ptr);

    size_encoded = encoding + size_h - ptr;
    if (size_encoded > cap) return 0;
    memcpy(buf, ptr, size_encoded);
    return size_encoded;
}

/*************************************************
 * Name:        decode_h
 *
 * Description: rANS decode polynomial vector h
 *
 * Arguments:   - int32_t *h: pointer to polynomial vector h
 *              - uint8_t *buf: pointer to output buffer
 **************************************************/
uint16_t decode_h(int32_t *h, const uint8_t *buf, uint16_t size_in) {
    size_t size_used;
    RansState rans;
    size_t size_h = N * K;
    uint32_t tmp;
    uint8_t s;
    const uint8_t *buf_start = buf;
    const uint8_t *buf_end = buf + size_in;

    if(RansDecInit(&rans, (uint8_t **) &buf)) {
        return 1; // corrupted initial state
    }

    for (size_t i = 0; i < size_h; i++) {
        s = READ_WORD(symbol_h[RansDecGet(&rans, SCALE_BITS)]);
        tmp = (uint32_t) s;
        if(tmp >= M_H) {
            return 1; // invalid symbol       
        }
        tmp =  (H_CUT < tmp) ? (tmp + OFFSET_H) : tmp;
        h[i] = tmp;
        RANS_DEC_ADV(&rans, (uint8_t **) &buf, buf_end, dsyms_h, s, SCALE_BITS);
    }

    if(RansDecVerify(&rans)) {
        return 1; // final state not correct        
    }

    size_used = buf - buf_start;
    if(size_used != size_in) {
        return 1; // size does not match
    }
    return 0;
}

/*
 * Low-SRAM helper: same as decode_h(), but writes into a uint16_t array.
 * This is safe because h is encoded as a small non-negative value.
 */
uint16_t decode_h_u16(uint16_t *h, const uint8_t *buf, uint16_t size_in) {
    size_t size_used;
    RansState rans;
    size_t size_h = N * K;
    uint32_t tmp;
    uint8_t s;
    const uint8_t *buf_start = buf;
    const uint8_t *buf_end = buf + size_in;

    if (RansDecInit(&rans, (uint8_t **)&buf)) {
        return 1; // corrupted initial state
    }

    for (size_t i = 0; i < size_h; i++) {
        s = READ_WORD(symbol_h[RansDecGet(&rans, SCALE_BITS)]);
        tmp = (uint32_t)s;
        if (tmp >= M_H) {
            return 1; // invalid symbol
        }
        tmp = (H_CUT < tmp) ? (tmp + OFFSET_H) : tmp;
        h[i] = (uint16_t)tmp;
        RANS_DEC_ADV(&rans, (uint8_t **)&buf, buf_end, dsyms_h, s, SCALE_BITS);
    }

    if (RansDecVerify(&rans)) {
        return 1; // final state not correct
    }

    size_used = (size_t)(buf - buf_start);
    if (size_used != size_in) {
        return 1; // size does not match
    }
    return 0;
}

/* =========================================================================
 * Streaming rANS decode API — row/column-at-a-time decoding
 * ========================================================================= */

/* --- h (hint) streaming --- */

uint16_t decode_h_stream_init(rans_stream_ctx *ctx, const uint8_t *buf, uint16_t size_in) {
    ctx->buf_start = buf;
    ctx->buf = buf;
    ctx->buf_end = buf + size_in;
    ctx->size_in = size_in;
    if (RansDecInit(&ctx->rans, (uint8_t **)&ctx->buf)) {
        return 1;
    }
    return 0;
}

uint16_t decode_h_stream_row_u16(uint16_t *h_row, rans_stream_ctx *ctx) {
    uint32_t tmp;
    uint8_t s;
    for (size_t i = 0; i < N; i++) {
        s = READ_WORD(symbol_h[RansDecGet(&ctx->rans, SCALE_BITS)]);
        tmp = (uint32_t)s;
        if (tmp >= M_H) {
            return 1;
        }
        tmp = (H_CUT < tmp) ? (tmp + OFFSET_H) : tmp;
        h_row[i] = (uint16_t)tmp;
        RANS_DEC_ADV(&ctx->rans, (uint8_t **)&ctx->buf, ctx->buf_end, dsyms_h, s, SCALE_BITS);
    }
    return 0;
}

uint16_t decode_h_stream_finish(rans_stream_ctx *ctx) {
    if (RansDecVerify(&ctx->rans)) {
        return 1;
    }
    size_t size_used = (size_t)(ctx->buf - ctx->buf_start);
    if (size_used != ctx->size_in) {
        return 1;
    }
    return 0;
}

/* --- hb_z1 (HighBits z1) streaming --- */

uint16_t decode_hb_z1_stream_init(rans_stream_ctx *ctx, const uint8_t *buf, uint16_t size_in) {
    ctx->buf_start = buf;
    ctx->buf = buf;
    ctx->buf_end = buf + size_in;
    ctx->size_in = size_in;
    if (RansDecInit(&ctx->rans, (uint8_t **)&ctx->buf)) {
        return 1;
    }
    return 0;
}

uint16_t decode_hb_z1_stream_col_i8(int8_t *hb_col, rans_stream_ctx *ctx) {
    uint32_t tmp;
    uint8_t s;
    for (size_t i = 0; i < N; i++) {
        s = READ_WORD(symbol_hb_z1[RansDecGet(&ctx->rans, SCALE_BITS)]);
        tmp = (uint32_t)s;
        if (tmp >= M_HB_Z1) {
            return 1;
        }
        hb_col[i] = (int8_t)((int32_t)tmp - (int32_t)OFFSET_HB_Z1);
        RANS_DEC_ADV(&ctx->rans, (uint8_t **)&ctx->buf, ctx->buf_end, dsyms_hb_z1, s, SCALE_BITS);
    }
    return 0;
}

uint16_t decode_hb_z1_stream_finish(rans_stream_ctx *ctx) {
    if (RansDecVerify(&ctx->rans)) {
        return 1;
    }
    size_t size_used = (size_t)(ctx->buf - ctx->buf_start);
    if (size_used != ctx->size_in) {
        return 1;
    }
    return 0;
}

/*************************************************
 * Name:        encode_hb_z1
 *
 * Description: rANS encode polynomial vector HighBits(z1)
 *
 * Arguments:   - uint8_t *buf: pointer to output buffer
 *              - const int32_t *hb_z1: pointer to polynomial vector
 *HighBits(z1)
 **************************************************/
uint16_t encode_hb_z1(uint8_t *buf, size_t cap, const int32_t *hb_z1) {
    size_t size_encoded;
    RansState rans;
    uint8_t *ptr;
    size_t size_hb_z1 = N * L;
    int8_t s;
    int32_t tmp;
    uint8_t encoding[size_hb_z1]; // size_hb_z1 is a loose upper bound

    RansEncInit(&rans);
    ptr = encoding + size_hb_z1; // end of output buffer

    for (size_t i = size_hb_z1; i > 0; i--) {
        // from centered to positive representation:
        tmp = hb_z1[i - 1] + OFFSET_HB_Z1;
        // check for very unlikely values that we do not encode
        // to make the encoding cheaper :
        if(tmp < 0 || M_HB_Z1 <= tmp) { 
            return 0;
        }
        s = (uint8_t) tmp;
                
        RANS_ENC_PUT(&rans, &ptr, esyms_hb_z1, s);
        // check that at least 4 byte remain for memory safety:
        if (ptr < encoding + 4)
        {
            return 0;
        }
    }
    RansEncFlush(&rans, &ptr);

    size_encoded = encoding + size_hb_z1 - ptr;
    if (size_encoded > cap) return 0;
    memcpy(buf, ptr, size_encoded);
    return size_encoded;
}

/*
 * Low-stack variant: encode HB(z1) from an int8_t array instead of int32_t.
 * This avoids buffering the full int32_t HB(z1) in the signing path.
 */
uint16_t encode_hb_z1_i8(uint8_t *buf, size_t cap, const int8_t *hb_z1) {
    size_t size_encoded;
    RansState rans;
    uint8_t *ptr;
    size_t size_hb_z1 = N * L;
    int32_t tmp;
    uint8_t encoding[size_hb_z1]; // size_hb_z1 is a loose upper bound

    RansEncInit(&rans);
    ptr = encoding + size_hb_z1; // end of output buffer

    for (size_t i = size_hb_z1; i > 0; i--) {
        // from centered to positive representation:
        tmp = (int32_t)hb_z1[i - 1] + OFFSET_HB_Z1;
        // check for very unlikely values that we do not encode
        if(tmp < 0 || M_HB_Z1 <= tmp) {
            return 0;
        }

        RANS_ENC_PUT(&rans, &ptr, esyms_hb_z1, (uint8_t)tmp);
        if (ptr < encoding + 4)
        {
            return 0;
        }
    }
    RansEncFlush(&rans, &ptr);

    size_encoded = encoding + size_hb_z1 - ptr;
    if (size_encoded > cap) return 0;
    memcpy(buf, ptr, size_encoded);
    return size_encoded;
}

/*************************************************
 * Name:        decode_hb_z1
 *
 * Description: rANS decode polynomial vector HighBits(z1)
 *
 * Arguments:   - int32_t *hb_z1: pointer to polynomial vector HighBits(z1)
 *              - uint8_t *buf: pointer to output buffer
 **************************************************/
uint16_t decode_hb_z1(int32_t *hb_z1, const uint8_t *buf, uint16_t size_in) {
    size_t size_used;
    RansState rans;
    size_t size_hb_z1 = N * L;
    uint32_t tmp;
    uint8_t s;
    const uint8_t *buf_start = buf;
    const uint8_t *buf_end = buf + size_in;

    if(RansDecInit(&rans, (uint8_t **) &buf)) {
        return 1; // corrupted initial state       
    }

    for (size_t i = 0; i < size_hb_z1; i++) {
        s = READ_WORD(symbol_hb_z1[RansDecGet(&rans, SCALE_BITS)]);
        tmp = (uint32_t) s;
        if(tmp >= M_HB_Z1) {
            return 1; // invalid symbol        
        }
        // from positive to centered representation:
        hb_z1[i] = tmp - OFFSET_HB_Z1;
        RANS_DEC_ADV(&rans, (uint8_t **)&buf, buf_end, dsyms_hb_z1, s, SCALE_BITS);
    }

    if(RansDecVerify(&rans)) {
        return 1; // final state not correct       
    }

    size_used = buf - buf_start;
    if(size_used != size_in) {
        return 1; // size does not match
    }
    return 0;
}

/*
 * Low-SRAM helper: rANS decode polynomial vector HighBits(z1) to int8_t.
 *
 * For all HAETAE parameter sets, HighBits(z1) is a small centered value
 * (in practice within a few dozen), so it fits in int8_t.
 */
uint16_t decode_hb_z1_i8(int8_t *hb_z1, const uint8_t *buf, uint16_t size_in) {
    size_t size_used;
    RansState rans;
    const size_t size_hb_z1 = N * L;
    uint32_t tmp;
    uint8_t s;
    const uint8_t *buf_start = buf;
    const uint8_t *buf_end = buf + size_in;

    if (RansDecInit(&rans, (uint8_t **)&buf)) {
        return 1; // corrupted initial state
    }

    for (size_t i = 0; i < size_hb_z1; i++) {
        s = READ_WORD(symbol_hb_z1[RansDecGet(&rans, SCALE_BITS)]);
        tmp = (uint32_t)s;
        if (tmp >= M_HB_Z1) {
            return 1; // invalid symbol
        }
        hb_z1[i] = (int8_t)((int32_t)tmp - (int32_t)OFFSET_HB_Z1);
        RANS_DEC_ADV(&rans, (uint8_t **)&buf, buf_end, dsyms_hb_z1, s, SCALE_BITS);
    }

    if (RansDecVerify(&rans)) {
        return 1; // final state not correct
    }

    size_used = (size_t)(buf - buf_start);
    if (size_used != size_in) {
        return 1; // size does not match
    }
    return 0;
}

/* =========================================================================
 * Streaming rANS encode API for h — feed coefficients one at a time in
 * reverse order (i=K-1..0, j=N-1..0).  Eliminates the need for a polyveck
 * hint buffer on the caller side.
 * ========================================================================= */

void encode_h_stream_init(encode_h_stream_ctx *ctx, uint8_t *buf, size_t cap) {
    RansEncInit(&ctx->rans);
    ctx->base = buf;
    ctx->ptr  = buf + cap;
    ctx->cap  = cap;
    ctx->error = 0;
}

void encode_h_stream_put(encode_h_stream_ctx *ctx, int32_t hint_value) {
    if (ctx->error) return;

    uint32_t tmp = (uint32_t)hint_value;
    if (H_CUT < tmp && tmp <= H_CUT + OFFSET_H) {
        ctx->error = 1;
        return;
    }
    tmp = (tmp > (H_CUT + OFFSET_H)) ? tmp - OFFSET_H : tmp;

    RANS_ENC_PUT(&ctx->rans, &ctx->ptr, esyms_h, (uint8_t)tmp);

    if (ctx->ptr < ctx->base + 4) {
        ctx->error = 1;
    }
}

uint16_t encode_h_stream_finish(encode_h_stream_ctx *ctx) {
    if (ctx->error) return 0;
    RansEncFlush(&ctx->rans, &ctx->ptr);

    size_t size_encoded = (size_t)(ctx->base + ctx->cap - ctx->ptr);
    if (size_encoded == 0) return 0;

    /* Shift compressed data to the front of the buffer. */
    memmove(ctx->base, ctx->ptr, size_encoded);
    return (uint16_t)size_encoded;
}

/*************************************************
 * Streaming rANS encode for HB(z1) — internal buffer variant.
 **************************************************/
void encode_hb_z1_stream_init(encode_hb_z1_stream_ctx *ctx) {
    RansEncInit(&ctx->rans);
    ctx->ptr = ctx->encoding + (BASE_ENC_HB_Z1 + 260);
    ctx->error = 0;
}

void encode_hb_z1_stream_put(encode_hb_z1_stream_ctx *ctx, int8_t hb_value) {
    if (ctx->error) return;
    int32_t tmp = (int32_t)hb_value + OFFSET_HB_Z1;
    if (tmp < 0 || M_HB_Z1 <= tmp) {
        ctx->error = 1;
        return;
    }
    RANS_ENC_PUT(&ctx->rans, &ctx->ptr, esyms_hb_z1, (uint8_t)tmp);
    if (ctx->ptr < ctx->encoding + 4) {
        ctx->error = 1;
    }
}

uint16_t encode_hb_z1_stream_finish(encode_hb_z1_stream_ctx *ctx, uint8_t *buf, size_t cap) {
    if (ctx->error) return 0;
    RansEncFlush(&ctx->rans, &ctx->ptr);

    size_t size_encoded = (size_t)(ctx->encoding + (BASE_ENC_HB_Z1 + 260) - ctx->ptr);
    if (size_encoded == 0 || size_encoded > cap) return 0;

    memcpy(buf, ctx->ptr, size_encoded);
    return (uint16_t)size_encoded;
}