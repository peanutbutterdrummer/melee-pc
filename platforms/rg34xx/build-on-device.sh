#!/bin/sh
# Build melee-pc natively on an Anbernic RG34XX(SP) (stock firmware, Ubuntu 22.04
# userland, gcc-12/g++-12 + ninja installed, CMake >= 3.25 at $CMAKE).
# Usage: build-on-device.sh <source dir> <build dir>
set -e
SRC=$(readlink -f "${1:-.}")
BUILD=${2:-$SRC/build-rg34xx}
CMAKE=${CMAKE:-cmake}
JOBS=${JOBS:-3}
SHIM=$SRC/platforms/rg34xx/shim
mkdir -p "$BUILD/shim/lib"

# Compatibility objects for the prebuilt Dawn archive (built against glibc 2.38
# and GCC 13 libstdc++; the firmware has glibc 2.35 and GCC 12).
gcc-12 -O2 -c -o "$BUILD/shim/glibc238compat.o" "$SHIM/glibc238compat.c"
g++-12 -O2 -c -o "$BUILD/shim/libstdcxx13compat.o" "$SHIM/libstdcxx13compat.cpp"
# Runtime shims: libEGL.so (fbdev window surface) and a stand-in libX11.so.6.
gcc-12 -O2 -Wall -fPIC -shared -o "$BUILD/shim/lib/libEGL.so" "$SHIM/fbegl.c" "$SHIM/glprof.c" -ldl
gcc-12 -O2 -Wall -fPIC -shared -Wl,-soname,libX11.so.6 -o "$BUILD/shim/lib/libX11.so.6" "$SHIM/fakex11.c"

configure() {
    "$CMAKE" -S "$SRC" -B "$BUILD" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12 \
        -DAURORA_DAWN_PROVIDER=package \
        -DAURORA_SDL3_PROVIDER=vendor \
        -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_VULKAN=OFF \
        -DSDL_OFFSCREEN=ON -DSDL_DUMMYVIDEO=ON -DSDL_UNIX_CONSOLE_BUILD=ON \
        -DCMAKE_CXX_FLAGS=-DAURORA_COMPACT_GPU_POOLS \
        "-DCMAKE_EXE_LINKER_FLAGS=$BUILD/shim/glibc238compat.o $BUILD/shim/libstdcxx13compat.o"
}
configure

sh "$SRC/platforms/rg34xx/patch-sdl-offscreen.sh" "$BUILD"

ninja -C "$BUILD" -j "$JOBS"
echo BUILD-OK
