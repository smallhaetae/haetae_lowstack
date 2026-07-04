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
#ifndef CONFIG_H
#define CONFIG_H

#define HAETAE_MODE 3

/*
 * Build-time configuration knobs (fixed below for this artifact):
 *
 *   -DSAMPLER=1/2          1 = one-pass hyperball (fast, large stack)
 *                          2 = two-pass low-stack (Bos-Renes-Sprenkels style)
 *
 *   -DSTREAM_MATRIX=0/1    1 = streamed A1, no A1[K] on stack
 *
 *   -DFROZEN_A=0/1/2       0 = full poly tmp
 *                          1 = poly_frozen tmp
 *                          2 = fused sample+multiply, no tmp buffer
 *
 *   -DFULL_STREAM_SIGN=1
 *       Full-streaming signing (full_stream_crypto_sign_signature):
 *       no static workspace, Pass A/B/C structure with row-streaming Ay
 *       and reverse-order streaming rANS for the hint and HB(z1).
 *       Requires SAMPLER=2, STREAM_MATRIX=1, FROZEN_A=2.
 */

#define SAMPLER 2
#define STREAM_MATRIX 1
#define FROZEN_A 2
#define FULL_STREAM_SIGN 1

#define STACK_PROFILE 0
#define STACK_PROFILE_USE_HAL 1

#if HAETAE_MODE == 2
#define CRYPTO_ALGNAME "HAETAE2"
#define HAETAE_NAMESPACETOP haetae2
#define HAETAE_NAMESPACE(s) cryptolab_haetae2_##s
#elif HAETAE_MODE == 3
#define CRYPTO_ALGNAME "HAETAE3"
#define HAETAE_NAMESPACETOP haetae3
#define HAETAE_NAMESPACE(s) cryptolab_haetae3_##s
#elif HAETAE_MODE == 5
#define CRYPTO_ALGNAME "HAETAE5"
#define HAETAE_NAMESPACETOP haetae5
#define HAETAE_NAMESPACE(s) cryptolab_haetae5_##s
#endif
#endif