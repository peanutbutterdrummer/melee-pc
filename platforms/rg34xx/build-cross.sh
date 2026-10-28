#!/bin/sh
# Cross-build melee-pc for the RG34XX on an x86-64 Ubuntu 22.04 host (e.g. WSL2).
# Usage: build-cross.sh <source dir> <build dir>
set -e
SRC=$(readlink -f "${1:-.}")
BUILD=${2:-$SRC/build-rg34xx}
CMAKE=${CMAKE:-cmake}
SHIM=$SRC/platforms/rg34xx/shim
CC=aarch64-linux-gnu-gcc-12
CXX=aarch64-linux-gnu-g++-12
mkdir -p "$BUILD/shim/lib"

# Compatibility objects for the prebuilt Dawn archive (glibc 2.38 / GCC 13 libstdc++ symbols).
$CC -O2 -c -o "$BUILD/shim/glibc238compat.o" "$SHIM/glibc238compat.c"
$CXX -O2 -c -o "$BUILD/shim/libstdcxx13compat.o" "$SHIM/libstdcxx13compat.cpp"
# Runtime shims: libEGL.so (fbdev window surface) and a stand-in libX11.so.6.
$CC -O2 -Wall -fPIC -shared -o "$BUILD/shim/lib/libEGL.so" "$SHIM/fbegl.c" -ldl
$CC -O2 -Wall -fPIC -shared -Wl,-soname,libX11.so.6 -o "$BUILD/shim/lib/libX11.so.6" "$SHIM/fakex11.c"

"$CMAKE" -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$SRC/platforms/rg34xx/toolchain-aarch64-linux-gnu.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAURORA_DAWN_PROVIDER=package \
    -DAURORA_SDL3_PROVIDER=vendor \
    -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_VULKAN=OFF \
    -DSDL_OFFSCREEN=ON -DSDL_DUMMYVIDEO=ON -DSDL_UNIX_CONSOLE_BUILD=ON \
    "-DCMAKE_EXE_LINKER_FLAGS=$BUILD/shim/glibc238compat.o $BUILD/shim/libstdcxx13compat.o"

sh "$SRC/platforms/rg34xx/patch-sdl-offscreen.sh" "$BUILD"
ninja -C "$BUILD"
echo BUILD-OK
