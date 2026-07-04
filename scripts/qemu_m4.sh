#!/usr/bin/env bash
# Build and RUN a Low-Stack HAETAE Cortex-M4 port under QEMU (mps2-an386).
# No physical board required. Prints functional result (KeyPair / Signature OK)
# and peak stack for keygen (low-stack and streamed), sign, and verify.
#
# Usage:  ./scripts/qemu_m4.sh [VARIANT] [MODE]
#   VARIANT : C (pure C) | asm (assembly NTT/sampler)   [default C]
#   MODE    : 2 | 3 | 5                                 [default 5]
#
# Sources:  cortex-m4/<VARIANT>/   (crypto port)
#           qemu/              (MPS2 HAL + shared Makefile; QEMU-only glue)
#
# Run from this artifact/ directory (self-contained):
#   docker build -t haetae-artifact .
#   docker run --rm -v "$PWD":/work -w /work haetae-artifact ./scripts/qemu_m4.sh C 5
set -euo pipefail

VARIANT="${1:-C}"
MODE="${2:-5}"
TEST_MODE="${TEST_MODE:-}"   # set TEST_MODE=1 for the deterministic checksum self-test

case "$VARIANT" in C|asm) ;; *) echo "VARIANT must be C|asm" >&2; exit 2;; esac
case "$MODE" in 2|3|5) ;; *) echo "MODE must be 2|3|5" >&2; exit 2;; esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ART="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT_DIR="$ART/cortex-m4/$VARIANT"
QEMU_SRC="$ART/qemu"
[ -d "$PORT_DIR" ] || { echo "missing port dir: $PORT_DIR" >&2; exit 1; }
[ -d "$QEMU_SRC/common" ] || { echo "missing qemu/common" >&2; exit 1; }

command -v arm-none-eabi-gcc >/dev/null || { echo "arm-none-eabi-gcc not found (run inside the container)" >&2; exit 1; }
command -v qemu-system-arm  >/dev/null || { echo "qemu-system-arm not found (run inside the container)" >&2; exit 1; }

cd "$PORT_DIR"

# Restore any files/dirs we stage in, even on error (keep the port tree pristine).
cleanup() {
  [ -f config.h.artbak ] && mv -f config.h.artbak config.h
  rm -rf obj bin elf common
  rm -f Makefile
}
trap cleanup EXIT

# Select security level (config.h hardcodes the mode; no -D override).
cp config.h config.h.artbak
sed -i -E "s/^#define[[:space:]]+HAETAE_MODE[[:space:]]+[0-9]+/#define HAETAE_MODE $MODE/" config.h

# Stage the QEMU-only glue into the port dir: the bundled qemu.mk expects
# `common/` and the shared Makefile alongside the crypto sources.
cp -r "$QEMU_SRC/common" common
cp "$QEMU_SRC/Makefile.qemu" Makefile

echo ">> Building cortex-m4/$VARIANT  HAETAE-$MODE  (functional + stack)"
rm -rf obj bin elf          # force a clean build (stale obj/ldscript.ld -> HardFault)
# NOTE: do NOT pass CPPFLAGS on the command line. It overrides the makefile's
# target-specific `CPPFLAGS += -DDATA_IN_FLASH` for the linker script, which
# drops .data initialisation and faults at reset. The defaults are correct.
make PLATFORM=qemu ELFNAME=qemu-test \
     EXTRA_CFLAGS="${TEST_MODE:+-DTEST_MODE=$TEST_MODE}" \
     bin/qemu-test.bin >/tmp/haetae_qemu_build.log 2>&1 \
  || { echo "BUILD FAILED"; tail -25 /tmp/haetae_qemu_build.log; exit 1; }

echo ">> Running under QEMU (mps2-an386)"
echo "------------------------------------------------------------"
timeout 180 qemu-system-arm -M mps2-an386 -nographic -semihosting \
        -kernel bin/qemu-test.bin
echo "------------------------------------------------------------"
echo ">> done."
