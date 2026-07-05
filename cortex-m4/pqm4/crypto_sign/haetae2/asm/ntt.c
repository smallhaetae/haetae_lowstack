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
#include "ntt.h"
#include "ntt332.h"
#include <stdint.h>

/*
 * NTT / Inverse NTT: delegate to ARM Cortex-M4 assembly (ntt332.S).
 * The ASM implementation uses the same q=64513, N=256 parameters and
 * produces bit-identical output to the C reference.
 */

void ntt(int32_t a[N]) {
    haetae_ntt332(a);
}

void invntt_tomont(int32_t a[N]) {
    haetae_invntt332_tomont(a);
}
