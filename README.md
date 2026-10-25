# Low-Stack HAETAE — Artifact

Reproducibility artifact for the paper

> **Low-Stack HAETAE for Memory-Constrained Microcontrollers**
> YoungBeom Kim, Christine van Vredendaal, Gustavo Banegas, Seog Chung Seo.
> *IACR Transactions on Cryptographic Hardware and Embedded Systems (TCHES) 2026.*

It contains the full source of our low-stack HAETAE implementation, the
assembly-optimized variants for low-end microcontrollers, and scripts that
build (and, where no board is required, **run**) each target.

> **HAETAE 1.2.0 version in pqm4.** The code here is the code measured in the
> paper (commit `080d282`), with only the fixes of the patch notes below applied.
> It follows HAETAE v3.0. The Cortex-M4 implementation, ported to HAETAE
> 1.2.0, is part of [pqm4](https://github.com/mupq/pqm4) as `m4fstack`
> ([haetae2](https://github.com/mupq/pqm4/tree/master/crypto_sign/haetae2/m4fstack),
> [haetae3](https://github.com/mupq/pqm4/tree/master/crypto_sign/haetae3/m4fstack),
> [haetae5](https://github.com/mupq/pqm4/tree/master/crypto_sign/haetae5/m4fstack);
> [mupq/pqm4#420](https://github.com/mupq/pqm4/pull/420)). Its outputs are
> byte-identical to the HAETAE 1.2.0 reference implementation, and its README
> lists the changes with respect to this artifact. Its stack and cycle counts
> therefore differ from the paper's tables; the pqm4 numbers are in pqm4's
> [`benchmarks.md`](https://github.com/mupq/pqm4/blob/master/benchmarks.md).

> **Patch notes (2026-10-26, security-related fix).** In `cortex-m0plus/` and `avr/`,
> `polyfix_addmul_ntt_inplace_acc2()` (HAETAE-5 signing, pass B) used the
> output of the Barrett inverse NTT, which lies in [0, q), as the signed
> product c·s. Both the C and the assembly Barrett paths (including
> `HAETAE_FORCE_C_NTT`) are affected. The rejection test therefore ran on wrong
> values: the ‖2z−y‖² test never rejected and the ‖z‖² test was decided by an
> overflowed sum. In our instrumented runs about 40 % of the signing attempts
> were accepted instead of about 17 % (the rates depend on the key and the
> messages). The packed signatures are correct and verify, but they were not
> rejection-sampled as the specification requires, so the security argument
> does not apply to them: the affected signing code must not be used, and keys
> that were used with it should be replaced as a precaution. We have not
> established a practical key-recovery attack. HAETAE-2 and -3, key generation,
> verification, the Montgomery-based Cortex-M4 and MSP430 signing paths and the
> pqm4 version do not have this defect; the AVR port ships the signing source,
> but its supplied build and test scenarios measure verification only.
> Fixed in `4949846` (reported by Ben Stephens, Deakin University). `529fb9a`
> also removes an inverted early exit in pass B, placed before the secret
> products are accumulated, that could only cause extra rejections and did not
> trigger in any of our instrumented runs. `9e9c43d` fixes the Cortex-M0+
> measurement harness: the vector table was dropped under `-flto` (the default
> of `scripts/build_m0p.sh`), so those images did not boot; the SysTick cycle
> counter of `test/cycle.h` added 2^24 cycles to about half of the samples
> longer than 0.35 s (it tested COUNTFLAG, which the handler does not clear)
> and wrapped after 2^32 cycles (89 s); and the signing benchmark left the last
> 10 bytes of its message uninitialised. The code as published with the paper
> is commit `080d282`.
>
> **Table 6 of the paper (NUCLEO-C031C6).** The HAETAE-5 (asm) signing figure
> was measured with the signing defect, and all key-generation and signing
> cycle counts of the table were taken with the faulty counter; the message
> set of the published signing runs cannot be reconstructed. We re-measured
> the twelve key-generation and signing cells with the fixed code and the
> corrected harness under the paper's conditions — arm-none-eabi-gcc 14.2.1,
> `-Os` without `-flto`, 48 MHz, upper median (sorted sample [50]) of 100 runs;
> (C) = the Montgomery-NTT C port from which the (asm) port was derived,
> (asm) = this repository — and these values replace the published ones:
>
> | | Impl. | KeyGen cycles (published) | Sign cycles (published) |
> |---|---|---|---|
> | HAETAE-2 | (C) | 51,412 K (66,191 K) | 528,030 K (318,888 K) |
> | | (asm) | 42,260 K (53,093 K) | 519,696 K (387,619 K) |
> | HAETAE-3 | (C) | 85,699 K (102,646 K) | 855,101 K (851,868 K) |
> | | (asm) | 68,342 K (85,287 K) | 834,384 K (835,638 K) |
> | HAETAE-5 | (C) | 63,777 K (74,318 K) | 1,056,488 K (728,614 K) |
> | | (asm) | 59,469 K (65,700 K) | 1,016,411 K (374,542 K) |
>
> The milliseconds follow from the cycles at 48 MHz. Key generation runs a
> deterministic sequence; measured with the published counter it exceeds the
> corrected counter by 2^24 cycles in 53 of 100 runs and agrees within 20
> cycles otherwise, so the KeyGen changes are the counter correction. Of the
> signing changes only HAETAE-5 (asm) is due to the signing defect; the other
> signing cells are a new sample (100 fully initialised messages, corrected
> counter) and differ from the published medians also by the sampling
> variability of a 100-run median, whose rejection-sampling attempt counts are
> small integers. The verification counts (below one counter wrap) reproduce
> the published ones within 0.4 % and are kept, as are the stack and flash
> entries; the peak signing stack of HAETAE-5 (asm) is 11,992 bytes for both
> the published and the fixed code in this re-measurement (published 12,048),
> and rebuilt images differ in size from the published ones by a few tens of
> bytes. The raw values of every run, the build recipe and the hash of the
> Montgomery-C sources are in `cortex-m0plus/results-2026-10-26/`. The MSP430
> port (Table 7) was not re-measured; its harness stores the cycle count in
> 32 bits, and we are re-checking those figures separately.

---

## 1. What this artifact demonstrates

| Claim in the paper | Where | How to reproduce here |
|--------------------|-------|------------------------|
| Pure-C low-stack keygen/sign/verify, functional + streaming | `tab:pqm4-eval` | `scripts/qemu_m4.sh` (QEMU, **no board**) |
| Assembly NTT/sampler variant (M4) | `tab:pqm4-eval` | `scripts/qemu_m4.sh asm` |
| M4 ports inside the pqm4 framework (stack/size + cycles) | `tab:pqm4-eval` | `cortex-m4/pqm4/` — `benchmarks.py` (QEMU + board) |
| Cortex-M0+ port within 12 KB SRAM (Barrett ASM) | `tab:m0p-results` | `scripts/build_m0p.sh` + board |
| 8-bit AVR verification within 8 KB SRAM, Barrett-ASM | `tab:avr-verify` | `scripts/build_avr.sh` + board |
| 16-bit MSP430 cross-architecture port | `tab:msp430-results` | `scripts/build_msp.sh` + board |

The **Cortex-M4 path runs end-to-end in QEMU** and needs no hardware: it prints
`KeyPair OK` / `Signature OK` and the peak stack of each operation. The M0+, AVR
and MSP430 cycle/stack numbers were taken on physical boards; the scripts here
reproduce the **builds and code sizes**, and the README documents the flashing
and on-board measurement procedure.

**What you can check without any board:** (1) the M4 QEMU functional run; (2) the
code sizes the build scripts print. As a concrete anchor, the default builds
(HAETAE-5, toolchains in §3.2) produce `.text` sizes of about:

| target | scenario | `.text` (bytes) |
|--------|----------|-----------------|
| Cortex-M0+ | keygen+sign / verify | 23128 / 20264 |
| 8-bit AVR  | verify               | 50340           |
| 16-bit MSP430 | functional        | 53230           |

(±~1 % across nearby compiler versions.) The SRAM/stack and cycle rows of the
paper tables require the boards.

---

## 2. Artifact layout

This directory is **self-contained** — each platform's sources sit in its own
top-level folder:

```
artifact/
├── README.md
├── LICENSE                 # Apache License 2.0 (our contribution)
├── NOTICE                  # third-party attributions (HAETAE ref, SHA-3, rANS, CMSIS, ...)
├── Dockerfile              # QEMU + arm-none-eabi build environment
├── scripts/
│   ├── qemu_m4.sh          # build + RUN the M4 ports under QEMU (no board)
│   ├── build_m0p.sh        # Cortex-M0+ cross-compile
│   ├── build_avr.sh        # 8-bit AVR cross-compile
│   └── build_msp.sh        # 16-bit MSP430 cross-compile
├── cortex-m4/              # 32-bit Cortex-M4 (QEMU; the headline low-stack target)
│   ├── C/                  #   ours (C)   — pure C, low-stack
│   ├── asm/                #   ours (asm) — + assembly NTT and sampler
│   └── pqm4/               #   the same C/asm ports as pqm4 implementations
│                           #   (benchmarks.py / test.py; QEMU + nucleo-l4r5zi)
├── cortex-m0plus/          # 32-bit Cortex-M0+ (NUCLEO-C031C6)   — Barrett ASM
├── avr/                    # 8-bit AVR  (Arduino Mega 2560)      — Barrett ASM
├── msp430/                 # 16-bit MSP430 (MSP-TS430PEU128)     — pure C
└── qemu/                   # QEMU-only glue, kept separate from the crypto ports
    ├── common/             #   MPS2 HAL, startup, randombytes (M4 / QEMU)
    └── Makefile.qemu       #   shared build Makefile for the M4 / QEMU ports
```

The M0+ and AVR boards ship the optimized **Barrett-ASM** implementation (one
variant per platform); MSP430 ships a pure-C port. The Cortex-M4 directory keeps
both the pure-C and assembly variants since both are reported as the headline
low-stack result. The Cortex-M4
reference port (`cortex-m4/C`) sets the paper's streaming knobs in `config.h`
(`SAMPLER=2`, `STREAM_MATRIX=1`, `FROZEN_A=2`, `FULL_STREAM_SIGN=1`); the board
ports derive from it and bake full-stream signing in (`SAMPLER=2`,
`STREAM_MATRIX=1`, `FROZEN_A=2`). Build outputs (`obj/`, `bin/`, `elf/`,
precompiled `bin_*`) are excluded; the scripts reconstruct the exact compiler
invocations, so the bundle builds clean. `qemu_m4.sh` stages `qemu/common`
and `Makefile.qemu` into the M4 port at build time and cleans them up after.

---

## 3. Prerequisites

### 3.1 Cortex-M4 via QEMU (recommended; no board)

A container is provided. Run everything from inside this `artifact/` directory
(the bundle is self-contained):

```bash
cd artifact
docker build -t haetae-artifact .
docker run --rm -it -v "$PWD":/work -w /work haetae-artifact bash
# then, inside the container:
./scripts/qemu_m4.sh C 5
```

It bundles `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi` and `qemu-system-arm`
(Ubuntu 24.04: arm-none-eabi-gcc 13.2, QEMU 8.2, `mps2-an386`). The image build
needs network access (apt). The exact compiler version is not pinned; code sizes
move by ≤~1 % across nearby GCC versions and the QEMU path is functional, so this
does not affect the claims. The AVR and MSP430 vendor toolchains are **not** in
this container (see §3.2).

### 3.2 Cross toolchains (for the low-end boards)

| Target | Toolchain (paper) | Board | Flasher |
|--------|-------------------|-------|---------|
| Cortex-M0+ | `arm-none-eabi-gcc` 14.2 | NUCLEO-C031C6 (STM32C031C6) | `st-flash` 1.8 |
| 8-bit AVR  | `avr-gcc` 15.2          | Arduino Mega 2560 R3 (ATmega2560) | `avrdude` 6.3 |
| 16-bit MSP430 | `msp430-elf-gcc` 9.3.1 | MSP-TS430PEU128 (MSP430F67791) | TI GDB agent / `mspdebug` |

Each script accepts a toolchain override, e.g. `ARM_GCC=…`, `AVR_GCC=…`,
`MSP_GCC=…`. Code sizes are stable across nearby compiler versions (±~1%).

---

## 4. Quick start — Cortex-M4 in QEMU

From inside the container (or any host with the ARM toolchain + QEMU):

```bash
./scripts/qemu_m4.sh C 5        # pure C, HAETAE-5
./scripts/qemu_m4.sh C 2        # HAETAE-2
./scripts/qemu_m4.sh asm 5      # assembly variant
```

The script first prints `>> Building …` / `>> Running …` banners; the program
output then **contains** the following key lines (HAETAE-5, `C`; an initial
`[SP] print …` and a `stack span = …` line also appear before this block):

```
====== START ======
=== Testing crypto_sign_keypair_streamed ===
[SP] keypair_streamed base=8400 peak=15224 (+6824) / 262144
KeyPair (streamed) OK!
[SP] sign   base=8400 peak=17496 (+9096) / 262144
[SP] verify base=8400 peak=16552 (+8152) / 262144
Signature OK!
====== END ======
```

The run exercises the headline streamed (low-stack) keygen → sign → verify
chain and reports `Signature OK!` only if the round-trip verifies. Each
operation also prints a `[SP] … peak=… (+N)` line with its measured peak stack
(`(+N)` = bytes of stack used by that operation).

This QEMU build is **unoptimized** (`-O0`): the runtime peaks are *larger* than
the paper's optimized figures and are not meant to match them — the run
demonstrates functional correctness and the streaming structure, not the
absolute numbers. The authoritative optimized stack/cycle figures are measured
on the boards and reported in the paper tables. The `asm` variant yields
identical functional output (the assembly only changes the NTT and sampler
internals).

> **Correctness.** `TEST_MODE=0` (the default) performs the functional
> sign→verify round-trip above. `TEST_MODE=1` runs a multi-iteration
> deterministic round-trip, printing per-iteration `pk/sk/sig` checksums and
> failing loudly on any verify mismatch:
>
> ```bash
> TEST_MODE=1 ./scripts/qemu_m4.sh C 5
> ```
>
> This is a self-test (deterministic checksums + sign→verify), **not** a
> comparison against external known-answer test vectors.

> **Randomness (important).** For deterministic, reproducible benchmarking and
> testing, every target uses a **non-random, deterministic `randombytes`**
> (a fixed-seed expander; the source carries a `#warning Using a non-random
> randombytes`). This is intentional for measurement and KAT reproducibility and
> is **not** a secure RNG — a deployment must replace `randombytes` with a real
> entropy source. The low-stack/streaming results are independent of the RNG.

### 4.1 Same M4 ports inside the pqm4 framework

The directory `cortex-m4/pqm4/` registers the **same** low-stack C and asm ports
as standard **pqm4** implementations (`crypto_sign/haetae{2,3,5}/{C,asm}`), so
they can be driven through `benchmarks.py` / `test.py`. It is self-contained
(its own `Dockerfile`):

```bash
cd cortex-m4/pqm4
docker build -t haetae-pqm4 .
docker run --rm -v "$PWD":/work -w /work haetae-pqm4 \
    python3 benchmarks.py --platform mps2-an386 haetae5
```

Under QEMU (`mps2-an386`) the **stack and size** rows of `tab:pqm4-eval` are
reproduced exactly (deterministic), while **cycles are not cycle-accurate**; the
authoritative cycle counts use the `nucleo-l4r5zi` platform on the physical
board. See `cortex-m4/pqm4/README.md`.

---

## 5. Low-end boards

### 5.1 Cortex-M0+ (NUCLEO-C031C6)

```bash
ARM_GCC=arm-none-eabi-gcc ./scripts/build_m0p.sh 5    # Barrett ASM, HAETAE-5
```

Builds the keygen+sign and verify scenarios with `-Os -flto`, prints code size,
and writes `.elf`/`.bin` to `/tmp/haetae_m0p_build/`. To run on the board:

```bash
st-flash --reset write /tmp/haetae_m0p_build/verify_5.bin 0x08000000
# then read USART2 at 115200 8N1, e.g.:  picocom -b 115200 /dev/ttyACM0
```

For the paper's figures, build the measurement scenarios explicitly — the script
takes the scenario as a second argument and the opt level via `OPT=` (no editing
required); flash the matching `.bin` the same way as above:

```bash
OPT=-O3 ./scripts/build_m0p.sh 5 speed_sign   # SysTick cycles (also speed_keygen, speed_verify)
./scripts/build_m0p.sh 5 stack_sign           # stack painting (-Os; also stack_keygen, stack_verify)
st-flash --reset write /tmp/haetae_m0p_build/speed_sign_5.bin 0x08000000
```

Cycles are taken at `-O3`, stack/size at `-Os`, as in the paper (the default is
`-Os`). Note a full `-O3` image can exceed the 32 KB STM32C031C6 flash; the
single-operation measurement binaries above still fit. All three levels fit in
12 KB SRAM (tightest: HAETAE-5 signing).

### 5.2 8-bit AVR (Arduino Mega 2560)

```bash
AVR_GCC=avr-gcc ./scripts/build_avr.sh 5
```

Verification only — the 8 KB-SRAM target. Constant tables and the public key
live in Flash (`PROGMEM`), so `.data = .bss = 0` and the whole SRAM is free for
the stack. The script produces `.elf` and a flashable `.hex`; flash with
`avrdude` and read the UART (9600 8N1, opening the port resets the board):

```bash
avrdude -c wiring -p atmega2560 -P /dev/ttyACM0 -b 115200 -D \
        -U flash:w:/tmp/haetae_avr_build/avr_verify_5.hex:i
stty -F /dev/ttyACM0 9600 raw -echo -hupcl; timeout 14 cat /dev/ttyACM0
```

The default build is the functional verify; the `speed` and `stack` scenarios
give the Timer1 cycle counts and stack-painting figures of the paper's AVR table
(cycles at `-O3`, stack at the default `-Os`). Flash the matching `.hex`
(`avr_speed_5.hex` / `avr_stack_5.hex`) the same way:

```bash
OPT=-O3 ./scripts/build_avr.sh 5 speed    # Timer1 cycle counts
./scripts/build_avr.sh 5 stack            # stack painting
```

### 5.3 16-bit MSP430 (MSP-TS430PEU128)

```bash
MSP_GCC=msp430-elf-gcc ./scripts/build_msp.sh 5
```

Built with `-mlarge` (20-bit addressing; the image exceeds 64 KB). The default
build uses `main.c` (functional + cycle-count globals); the `stack` scenario
builds the stack-painting entry, with `STACK_SCENARIO` selecting the operation:

```bash
OPT=-O3 ./scripts/build_msp.sh 5                      # cycles (functional entry, main.c)
STACK_SCENARIO=1 ./scripts/build_msp.sh 5 stack       # keygen stack
STACK_SCENARIO=2 ./scripts/build_msp.sh 5 stack       # sign stack
STACK_SCENARIO=3 ./scripts/build_msp.sh 5 stack       # verify stack
```

> **On-board measurement is not scripted here.** The MSP430 build produces the
> ELF; reading the cycle/stack results needs a TI debug probe (MSP-FET) plus the
> TI GDB agent or `mspdebug`, which are not bundled. With a probe attached you
> load the ELF and read the globals `g_cycles_keygen` / `g_cycles_sign` /
> `g_cycles_verify` (functional build) or the stack high-water mark
> (`stack` build). The MSP430 cycle/stack table rows are therefore **not
> reproducible from this bundle without that hardware/tooling**; the build and
> code size are.

---

## 6. Toolchain notes

* On-board cycle/stack numbers in the paper were taken with the toolchain
  versions in §3.2. Code-size figures reproduced here may differ by ~1% under a
  different compiler version; the C-vs-assembly comparison is unaffected because
  both rows are built with the same compiler.
* This artifact bundles only the HAETAE port sources; unrelated example
  directories from the development tree are **excluded**.

---

## 7. Licensing

The source files (`.c`/`.h`/`.S`) we authored or modified for this work are
released under the **Apache License 2.0** (see `LICENSE`) and carry an Apache 2.0
header at the top of the file. The build scripts, Dockerfile and linker scripts
are likewise ours and Apache-2.0-covered (see `NOTICE`), without an individual
per-file header.

This artifact also bundles third-party code that remains under its own license
and is **not** relicensed: the HAETAE reference implementation (the files we
kept unmodified), SHA-3/SHAKE and the rANS coder (public domain), the CMSIS/MPS2
platform support (Apache-2.0, ARM), and a simavr helper header (GPLv3). See
`NOTICE` for the per-component attribution and the exact list of files.
