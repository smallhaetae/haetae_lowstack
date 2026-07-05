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
#include "fft.h"
#include "const.h"
#include "platform.h"


static inline int32_t _mulrnd16(const int32_t x, const int32_t y) {
    int64_t r = ((int64_t)x * (int64_t)y) + (1L << 15);
    return r >> 16;
}

static inline int32_t _complex_mul_real(const complex_fp32_16 x,
                                        const complex_fp32_16 y) {
    return _mulrnd16(x.real, y.real) - _mulrnd16(x.imag, y.imag);
}

static inline int32_t _complex_mul_imag(const complex_fp32_16 x,
                                        const complex_fp32_16 y) {
    return _mulrnd16(x.real, y.imag) + _mulrnd16(x.imag, y.real);
}

static void _complex_mul(complex_fp32_16 *r, const complex_fp32_16 x,
                         const complex_fp32_16 y) {
    r->real = _complex_mul_real(x, y);
    r->imag = _complex_mul_imag(x, y);
}


/****************************************************************************
 * Initialize the FFT array with coeffs[i] * root[i] and
 * store the values in bit-reversed order.
 ****************************************************************************/
void fft_init_and_bitrev(complex_fp32_16 r[FFT_N], const poly *x) {
    int i, inv_i;
    int c;
    for (i = 0; i < FFT_N; i++) {
        inv_i = (int)READ_WORD(brv8[i]);
        c = x->coeffs[i];
        r[inv_i].real = c * (int32_t)READ_DWORD(roots[i].real);
        r[inv_i].imag = c * (int32_t)READ_DWORD(roots[i].imag);
    }
}

void fft_init_and_bitrev_inplace(complex_fp32_16 r[FFT_N]) {
    int32_t *coeffs = (int32_t *)r;
    /* Phase 1: expand int32 → complex×root, backwards */
    for (int i = FFT_N - 1; i >= 0; i--) {
        int c = coeffs[i];
        r[i].real = c * (int32_t)READ_DWORD(roots[i].real);
        r[i].imag = c * (int32_t)READ_DWORD(roots[i].imag);
    }
    /* Phase 2: in-place bit-reversal permutation */
    for (int i = 0; i < FFT_N; i++) {
        int j = (int)READ_WORD(brv8[i]);
        if (i < j) {
            complex_fp32_16 tmp = r[i];
            r[i] = r[j];
            r[j] = tmp;
        }
    }
}

int32_t complex_fp_sqabs(complex_fp32_16 x) {
    return _mulrnd16(x.real, x.real) + _mulrnd16(x.imag, x.imag);
}

void fft(complex_fp32_16 data[FFT_N]) {
    unsigned int r, m, md2, n, k, even, odd, twid;
    complex_fp32_16 u, t;

    for (r = 1; r <= FFT_LOGN; r++) {
        m = 1 << r;
        md2 = m >> 1;
        for (n = 0; n < FFT_N; n += m) {
            for (k = 0; k < md2; k++) {
                even = n + k;
                odd = even + md2;              
                twid = k << (FFT_LOGN - r + 1);
                        
                u = data[even];
                {
                    complex_fp32_16 _root_tmp;
                    _root_tmp.real = (int32_t)READ_DWORD(roots[twid].real);
                    _root_tmp.imag = (int32_t)READ_DWORD(roots[twid].imag);
                    _complex_mul(&t, _root_tmp, data[odd]);
                }
                data[even].real = u.real + t.real;
                data[even].imag = u.imag + t.imag;
                data[odd].real = u.real - t.real;
                data[odd].imag = u.imag - t.imag;
            }
        }
    }
}
