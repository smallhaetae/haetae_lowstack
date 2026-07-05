# Reproducibility container for Low-Stack HAETAE (TCHES 2026).
#
# Covers the board-free reproduction paths:
#   * 32-bit Cortex-M4  : build + RUN under QEMU (mps2-an386), functional + stack
#   * 32-bit Cortex-M0+ : build only (flash/code-size; cycle/stack need the board)
#
# The 8-bit AVR and 16-bit MSP430 paths use vendor toolchains and physical
# boards; see README.md (they are not installable cleanly via apt at the exact
# versions used in the paper).
#
#   cd artifact && docker build -t haetae-artifact .
#   docker run --rm -it -v "$PWD":/work -w /work haetae-artifact bash
#   ./scripts/qemu_m4.sh C 5           # build + run HAETAE-5 in QEMU (or: asm 5)
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    make \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    qemu-system-arm \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
CMD ["bash"]
