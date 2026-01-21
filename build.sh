#!/bin/bash
# Build murm386 for RP2350
#
# Usage: ./build.sh [options]
#   -M1, -M2           Board variant (default: M1)
#   -252, -378, -504   CPU speed in MHz (default: 504)
#   -clean             Clean build directory first
#
# Examples:
#   ./build.sh                  # Build M1 at 504MHz
#   ./build.sh -M2 -378         # Build M2 at 378MHz
#   ./build.sh -clean -M1 -504  # Clean build M1 at 504MHz

BOARD_VARIANT="M1"
CPU_SPEED="504"
PSRAM_SPEED="166"
CLEAN=0

# Parse arguments
for arg in "$@"; do
    case $arg in
        -M1) BOARD_VARIANT="M1" ;;
        -M2) BOARD_VARIANT="M2" ;;
        -252) CPU_SPEED="252"; PSRAM_SPEED="133" ;;
        -378) CPU_SPEED="378"; PSRAM_SPEED="133" ;;
        -504) CPU_SPEED="504"; PSRAM_SPEED="166" ;;
        -clean) CLEAN=1 ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

echo "Building murm386 for RP2350"
echo "  Board: $BOARD_VARIANT"
echo "  CPU:   $CPU_SPEED MHz"
echo "  PSRAM: $PSRAM_SPEED MHz"

if [ $CLEAN -eq 1 ] || [ ! -d ./build ]; then
    rm -rf ./build
    mkdir build
fi

cd build
cmake -DPICO_PLATFORM=rp2350 \
      -DBOARD_VARIANT=$BOARD_VARIANT \
      -DCPU_SPEED=$CPU_SPEED \
      -DPSRAM_SPEED=$PSRAM_SPEED \
      ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
