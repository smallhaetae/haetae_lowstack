# Reproducibility container for the Low-Stack HAETAE pqm4 integration
# (TCHES 2026).  Lets you drive the low-stack Cortex-M4 ports through the
# standard pqm4 / mupq benchmarking framework (benchmarks.py, test.py).
#
# Board-free path (no hardware): the mps2-an386 platform runs under QEMU.
#   docker build -t haetae-pqm4 .
#   docker run --rm -v "$PWD":/work -w /work haetae-pqm4 \
#       python3 benchmarks.py --platform mps2-an386 haetae5
#
# QEMU is functionally correct and reproduces the paper's STACK and SIZE
# figures, but it is NOT cycle-accurate: the speed (cycle) numbers from the
# mps2-an386 platform are not the paper's board figures.  The authoritative
# cycle counts come from the physical STM32L4R5ZI (nucleo-l4r5zi platform),
# which additionally needs libopencm3 and openocd + the board (see README).
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    make \
    git \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    qemu-system-arm \
    python3 \
    python3-serial \
    python3-tqdm \
    openocd \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
CMD ["bash"]
