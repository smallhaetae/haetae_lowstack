#!/usr/bin/env bash
# Cross-compile the Cortex-M0+ Barrett-ASM port (NUCLEO-C031C6 / STM32C031C6).
# Builds the two deployment scenarios and reports code size (.text/.data/.bss).
# Running for cycles/stack needs the physical board (see README, st-flash).
#
# Usage:  ./scripts/build_m0p.sh [MODE] [SCENARIO]
#   MODE     : 2 | 3 | 5                                      [default 5]
#   SCENARIO : test_sign | test_verify | speed_keygen | speed_sign |
#              speed_verify | stack_keygen | stack_sign | stack_verify
#              (any test/<name>.c; default: builds test_sign + test_verify)
#              speed_* = SysTick cycles, stack_* = stack painting (paper figures)
#
# Toolchain: arm-none-eabi-gcc (override with ARM_GCC=/path/to/arm-none-eabi-gcc).
# Paper used arm-gnu-toolchain 14.2-rel1:
#   ARM_GCC=/path/to/arm-gnu-toolchain-14.2/bin/arm-none-eabi-gcc
# (12.4 / 13.2 / 13.3 give the same code within ~1%.)
set -euo pipefail

MODE="${1:-5}"
SCENARIO="${2:-}"
ARM_GCC="${ARM_GCC:-arm-none-eabi-gcc}"
SZ="${ARM_GCC%gcc}size"
OPT="${OPT:--Os}"   # size/stack at -Os (default); cycles at -O3 (OPT=-O3,
                    # but note -O3 may overflow the 32 KB STM32C031C6 flash)

case "$MODE" in 2|3|5) ;; *) echo "bad MODE" >&2; exit 2;; esac
command -v "$ARM_GCC" >/dev/null || { echo "$ARM_GCC not found; set ARM_GCC=" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR="$(cd "$SCRIPT_DIR/.." && pwd)/cortex-m0plus"
[ -d "$DIR" ] || { echo "missing $DIR" >&2; exit 1; }
cd "$DIR"

OUT=/tmp/haetae_m0p_build; mkdir -p "$OUT"
FLAGS=(-Wall "$OPT" -std=c99 -mcpu=cortex-m0plus -mthumb -ffunction-sections -fdata-sections -flto -I. -DHAETAE_MODE="$MODE")
LDF=(-Tstm32c031.ld -Wl,--gc-sections -lm -lc -lgcc -nostartfiles --specs=nano.specs)
ASM=(ntt_asm.S pointmul_asm.S)

# *.c excludes nothing harmful; the test mains live under test/.
SRCS=(*.c)

build() {  # <entry-main> <out-name>
  "$ARM_GCC" "${FLAGS[@]}" "${SRCS[@]}" "${ASM[@]}" "$1" "${LDF[@]}" \
      -o "$OUT/$2.elf" 2>"$OUT/$2.err" \
    || { echo "  build $2 FAILED:"; tail -15 "$OUT/$2.err"; return 1; }
  "${ARM_GCC%gcc}objcopy" -O binary "$OUT/$2.elf" "$OUT/$2.bin"
  printf "  %-22s " "$2:"; "$SZ" "$OUT/$2.elf" | tail -1
}

echo ">> cortex-m0plus (Barrett ASM)  HAETAE-$MODE  ($OPT -flto)"
echo "   text  data  bss  dec  hex  file"
if [ -n "$SCENARIO" ]; then
  [ -f "test/${SCENARIO}.c" ] || { echo "no such scenario: test/${SCENARIO}.c" >&2; \
      echo "available:"; (cd test && ls *.c | sed 's/\.c$//' | sed 's/^/  /'); exit 2; }
  build "test/${SCENARIO}.c" "${SCENARIO}_${MODE}"
else
  build test/test_sign.c   "keygen_sign_${MODE}"
  build test/test_verify.c "verify_${MODE}"
fi
echo ">> done. ELFs in $OUT  (flash with st-flash; see README)"
