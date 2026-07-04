# Low-Stack HAETAE — pqm4 / mupq integration

Our low-stack Cortex-M4 ports registered as **pqm4** implementations, so they can
be driven through the standard pqm4 / mupq benchmarking framework
(`benchmarks.py`, `test.py`). This is the framework path that produces the
paper's `tab:pqm4-eval` numbers; the standalone QEMU demo in `../C` / `../asm`
(`scripts/qemu_m4.sh`) is the lighter, framework-free alternative.

## What's here

```
crypto_sign/haetae{2,3,5}/C      # pure-C low-stack port  (ours)
crypto_sign/haetae{2,3,5}/asm    # + assembly NTT / sampler (ours)
mupq/  common/  mk/  ldscripts/  # pqm4 / mupq framework (trimmed to the above)
benchmarks.py  test.py           # framework entry points
Dockerfile  requirements.txt     # self-contained build/run environment
```

The implementation directory names (`C` / `asm`) mirror the platform-grouped
layout of the rest of the artifact (`cortex-m4/C`, `cortex-m4/asm`). Only the
HAETAE low-stack implementations are kept; the upstream pqm4 reference ports and
the unrelated schemes were removed. `libopencm3` (needed only for the physical
board, see below) is **not** bundled.

## Quick start — QEMU, no board

```bash
cd artifact/cortex-m4/pqm4
docker build -t haetae-pqm4 .
# benchmark HAETAE-5 (both C and asm), no hardware:
docker run --rm -v "$PWD":/work -w /work haetae-pqm4 \
    python3 benchmarks.py --platform mps2-an386 haetae5
```

Replace `haetae5` with `haetae2` / `haetae3` for the other levels. Results are
written under `benchmarks/{stack,speed,size,hashing}/...`.

Functional self-test / known-answer comparison:

```bash
docker run --rm -v "$PWD":/work -w /work haetae-pqm4 \
    python3 test.py --platform mps2-an386 haetae5
```

## What the QEMU (mps2-an386) path reproduces

* **Stack and code size — yes.** Stack painting is deterministic and code size is
  exact, so these match the paper's reported figures (every variant has
  `.data = .bss = 0`).
* **Speed (cycles) — no.** `mps2-an386` under QEMU is **not cycle-accurate**: the
  cycle counts it prints are functional, *not* the paper's board figures. The
  authoritative cycles come from the physical board (next section).

## Board path — authoritative cycles (NUCLEO-L4R5ZI)

The paper's M4 cycle counts are measured on the physical STM32L4R5ZI via the
`nucleo-l4r5zi` platform. In addition to the board itself this needs `libopencm3`
(not bundled) and `openocd` (in the container). The OpenOCD config
`st_nucleo_l4r5.cfg` is included.

```bash
# fetch + build libopencm3 once (or drop crypto_sign/haetae*/{C,asm} into a full
# pqm4 checkout that already has it):
git clone https://github.com/libopencm3/libopencm3
make -C libopencm3

# flash + measure on the attached board (set the right serial port):
python3 benchmarks.py --platform nucleo-l4r5zi -u /dev/ttyACMx haetae5
```

## Implementation notes

* `C` and `asm` use the **pqm4 common `fips202`** (assembly `KeccakF1600`), the
  same Keccak backend as the reference m4f ports, so cycle comparisons are on an
  equal footing. The thin `symmetric.h` / `symmetric-shake.c` layer maps our XOF
  calls onto pqm4's incremental SHAKE API.
* The low-stack contribution lives in `sign.c` / `sign_passes.c` (full-streaming
  signing), `keygen.c` (streamed key generation), `verify.c` (row-streamed
  verification) and `encoding.c` (reverse-order streaming rANS).
* On-chip stack profiling is disabled here (`STACK_PROFILE 0`); pqm4 measures
  stack itself.

## Licensing

* Our implementation files (`crypto_sign/haetae*/{C,asm}`, the files we authored
  or modified) are **Apache-2.0** and carry the header in-file. See `NOTICE`.
* The pqm4 / mupq framework and the bundled SHA-3/Keccak, AES, CMSIS/MPS2 support
  remain under their own licenses (Apache-2.0 / CC0-1.0 / public domain). See
  `NOTICE` and `README.pqm4-upstream.md` (upstream <https://github.com/mupq/pqm4>).
