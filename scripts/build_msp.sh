#!/usr/bin/env bash
# Cross-compile the 16-bit MSP430 port (MSP-TS430PEU128 / MSP430F67791).
# Reports code size. Running for cycles/stack needs the board + TI tools
# (see README for the on-board measurement procedure).
#
# Usage:  ./scripts/build_msp.sh [MODE] [SCENARIO]
#   MODE     : 2 | 3 | 5                                [default 5]
#   SCENARIO : functional (default, main.c) | stack (stack_main.c, stack painting)
#
# Toolchain: msp430-elf-gcc (override MSP_GCC=). Paper used msp430-gcc 9.3.1.
# Device support headers/linker scripts: MSP_INC= (defaults next to the compiler).
set -euo pipefail

MODE="${1:-5}"
SCENARIO="${2:-functional}"
MSP_GCC="${MSP_GCC:-msp430-elf-gcc}"
SZ="${MSP_GCC%gcc}size"
MCU="${MCU:-msp430f67791}"
OPT="${OPT:--Os}"     # code size/stack reported at -Os; cycles at -O3 (OPT=-O3)

case "$MODE" in 2|3|5) ;; *) echo "bad MODE" >&2; exit 2;; esac
command -v "$MSP_GCC" >/dev/null || { echo "$MSP_GCC not found; set MSP_GCC=" >&2; exit 1; }

# Device support files (msp430f67791.ld + headers) usually ship with the toolchain.
GCC_PATH="$(command -v "$MSP_GCC")"
MSP_INC="${MSP_INC:-$(cd "$(dirname "$GCC_PATH")/.." && pwd)/include}"
[ -f "$MSP_INC/$MCU.ld" ] || { echo "device support not found: $MSP_INC/$MCU.ld (set MSP_INC=)" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR="$(cd "$SCRIPT_DIR/.." && pwd)/msp430"
[ -d "$DIR" ] || { echo "missing $DIR" >&2; exit 1; }
cd "$DIR"

OUT=/tmp/haetae_msp_build; mkdir -p "$OUT"
# -mlarge: 20-bit addressing; the image is >64 KB so the small model overflows.
FLAGS=(-mmcu="$MCU" -mlarge -mcode-region=either -mdata-region=lower
       -DHAETAE_MODE="$MODE" "$OPT" -std=gnu99 -ffunction-sections -fdata-sections
       -I. -I"$MSP_INC")
LDF=(-L"$MSP_INC" -Wl,--gc-sections)

# main.c = functional entry; stack_main.c = stack-measurement entry (mutually
# exclusive: each defines main()).
# STACK_SCENARIO selects which op the stack-painting entry measures:
#   1 = keygen (default), 2 = sign, 3 = verify
STACK_SCENARIO="${STACK_SCENARIO:-1}"
case "$SCENARIO" in
  functional) EXCL=stack_main.c; NAME="msp_${MODE}";;
  stack)      EXCL=main.c;       NAME="msp_stack_s${STACK_SCENARIO}_${MODE}"
              FLAGS+=(-DSTACK_SCENARIO="$STACK_SCENARIO");;
  *) echo "bad SCENARIO (functional|stack)" >&2; exit 2;;
esac
SRCS=(); for f in *.c; do [ "$f" = "$EXCL" ] && continue; SRCS+=("$f"); done

echo ">> msp430  HAETAE-$MODE  ($SCENARIO, $OPT -mlarge, mcu=$MCU)"
"$MSP_GCC" "${FLAGS[@]}" "${SRCS[@]}" "${LDF[@]}" \
    -o "$OUT/$NAME.elf" 2>"$OUT/err.txt" \
  || { echo "BUILD FAILED:"; grep -iE 'error|overflow|truncated' "$OUT/err.txt" | head; exit 1; }
echo "   text  data  bss  dec  hex  file"
printf "  "; "$SZ" "$OUT/$NAME.elf" | tail -1
echo ">> done. $OUT/$NAME.elf (load via TI tools / mspdebug; see README)."
