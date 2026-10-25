# Re-measurement of Table 6 (NUCLEO-C031C6), 2026-10-26

UART output of the `test/speed_*.c` scenarios after the fixes of the erratum
(see the top-level README): HAETAE-2/3/5, 100 runs each. `RAW:` holds the
cycle count of every run in execution order, followed by the sorted
min / upper median (sorted sample [50]) / max.

## Conditions

- Arm GNU Toolchain 14.2.Rel1 (arm-none-eabi-gcc 14.2.1), `-Os`, **without
  `-flto`** (`scripts/build_m0p.sh` always adds `-flto`; the published cycle
  counts were taken without it, which is how the published deterministic
  verification counts are reproduced). Exact command, run in `cortex-m0plus/`:

  ```sh
  arm-none-eabi-gcc -Wall -Os -std=c99 -mcpu=cortex-m0plus -mthumb \
      -ffunction-sections -fdata-sections -I. -DHAETAE_MODE=5 \
      *.c ntt_asm.S pointmul_asm.S test/speed_sign.c \
      -Tstm32c031.ld -Wl,--gc-sections -lm -lc -lgcc -nostartfiles --specs=nano.specs \
      -o speed_sign_5.elf
  ```
- 48 MHz (HSI48), two NUCLEO-C031C6 boards (STM32C031C6), flashed with
  st-flash 1.8.0; output read from the ST-LINK VCP at 115200 baud.

## Files

- `asm-{verify,keygen,sign}-{2,3,5}.txt`: this directory's code at the commit
  of the erratum (Barrett NTT in assembly; the (asm) rows of Table 6).
- `montgomeryC-{verify,keygen,sign}-{2,3,5}.txt`: the development tree's
  pure-C port with the Montgomery NTT, from which this port was derived (the
  (C) rows of Table 6). It is not part of this artifact: besides `ntt.c` it
  differs from this directory in `reduce.c`, `poly.c`, `polyvec.c`,
  `keygen.c`, `verify.c`, `sign_passes.c` (it still contains the EE-3 early
  exit) and `startup.c`. It was built with the command above without the two
  assembly files and with this directory's `test/` harness. The SHA-256 of its
  sources (`*.c`, `*.h`, `*.ld`, excluding `test/`) are in
  `montgomeryC-sources.sha256`.
- `asm-verify-5-published-counter.txt`, `montgomeryC-verify-5-published-counter.txt`:
  the same verification measured with the published `test/cycle.h`
  (9,336,103 and 12,489,394 cycles, the published 9,336 K and 12,489 K).
- `asm-keygen-5-published-counter.txt`: the same key generation measured with
  the published counter: 53 of the 100 runs are larger by 2^24 cycles (within
  20 cycles), the others agree within 20 cycles.
