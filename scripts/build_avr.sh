#!/usr/bin/env bash
# Cross-compile the 8-bit AVR verification port (Arduino Mega 2560 / ATmega2560).
# HAETAE verification only (the 8 KB SRAM target). Reports code size.
# Running for cycles/stack needs the board (see README, avrdude + Timer1).
#
# Usage:  ./scripts/build_avr.sh [MODE] [SCENARIO]
#   MODE     : 2 | 3 | 5                                      [default 5]
#   SCENARIO : verify (default) | speed | stack
#              speed = Timer1 cycles, stack = stack painting (paper AVR table)
#
# Toolchain: avr-gcc (override AVR_GCC=/path/to/avr-gcc). Paper used avr-gcc 15.2.
set -euo pipefail

MODE="${1:-5}"
SCENARIO="${2:-verify}"
AVR_GCC="${AVR_GCC:-avr-gcc}"
SZ="${AVR_GCC%gcc}size"
MCU="${MCU:-atmega2560}"
F_CPU="${F_CPU:-16000000}"
OPT="${OPT:--Os}"     # code size/stack reported at -Os; cycles at -O3 (OPT=-O3)

case "$MODE" in 2|3|5) ;; *) echo "bad MODE" >&2; exit 2;; esac
case "$SCENARIO" in
  verify) ENTRY=test/test.c;  NAME="avr_verify_${MODE}";;
  speed)  ENTRY=test/speed.c; NAME="avr_speed_${MODE}";;
  stack)  ENTRY=test/stack.c; NAME="avr_stack_${MODE}";;
  *) echo "bad SCENARIO (verify|speed|stack)" >&2; exit 2;;
esac
command -v "$AVR_GCC" >/dev/null || { echo "$AVR_GCC not found; set AVR_GCC=" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR="$(cd "$SCRIPT_DIR/.." && pwd)/avr"
[ -d "$DIR" ] || { echo "missing $DIR" >&2; exit 1; }
cd "$DIR"

OUT=/tmp/haetae_avr_build; mkdir -p "$OUT"
# Constant tables + public key in Flash via PROGMEM -> .data = .bss = 0.
FLAGS=(-mmcu="$MCU" -DF_CPU="${F_CPU}UL" -DHAETAE_MODE="$MODE" "$OPT" -std=gnu99
       -ffunction-sections -fdata-sections -I.)
LDF=(-Wl,--gc-sections)
ASM=(ntt_asm.S pointmul_asm.S)

# Crypto sources in this dir; entry main = the selected scenario under test/.
SRCS=(*.c)

echo ">> avr (Barrett ASM)  HAETAE-$MODE  ($SCENARIO, $OPT, mcu=$MCU)"
"$AVR_GCC" "${FLAGS[@]}" "${SRCS[@]}" "${ASM[@]}" "$ENTRY" "${LDF[@]}" \
    -o "$OUT/$NAME.elf" 2>"$OUT/err.txt" \
  || { echo "BUILD FAILED:"; tail -20 "$OUT/err.txt"; exit 1; }
"${AVR_GCC%gcc}objcopy" -O ihex -R .eeprom "$OUT/$NAME.elf" "$OUT/$NAME.hex"
echo "   text  data  bss  dec  hex  file"
printf "  "; "$SZ" "$OUT/$NAME.elf" | tail -1
echo ">> done. Flash $OUT/$NAME.hex with avrdude (see README)."
