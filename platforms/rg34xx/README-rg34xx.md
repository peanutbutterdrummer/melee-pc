# melee-pc on the Anbernic RG34XX / RG34XXSP (stock firmware)

A build of [melee-pc](https://github.com/999sian/melee-pc) (the native port of
Super Smash Bros. Melee built from the [doldecomp/melee](https://github.com/doldecomp/melee)
decompilation) for Anbernic's H700 handhelds running the **stock Anbernic
firmware**.

You need your own disc image: **Super Smash Bros. Melee, USA, revision 2
(NTSC-U 1.02, GALE01)**, SHA-1 `d4e70c064cc714ba8400a849cf299dbd1aa326fc`.
No game data is included.

## Why this needed new work

melee-pc renders through Dawn (WebGPU), which on Linux only uses Vulkan and
expects an X11 or Wayland window. The stock firmware has neither a Vulkan
driver for the Mali-G31 nor a window system: only the Mali fbdev OpenGL ES 3.2
blob. This port gets there without Vulkan, X11 or Wayland:

| Piece | What it does |
|---|---|
| Dawn OpenGL ES backend | `MELEE_BACKEND=gles` selects Dawn's GLES 3.1 compatibility backend, which the Mali blob supports (all required EGL extensions are present). |
| SDL3 `offscreen` video driver | SDL3 is built with `SDL_UNIX_CONSOLE_BUILD` (no X11/Wayland/KMSDRM) and only provides input, audio and a window object. Its fake desktop is patched to 720x480. |
| aurora patch | When SDL runs the `offscreen` driver, aurora hands Dawn a placeholder Xlib surface (`extern/aurora/lib/dawn/BackendBinding.cpp`). |
| `shim/fbegl.c` → `lib/libEGL.so` | Dawn dlopens `libEGL.so`; this shim forwards to the firmware's Mali EGL and turns the placeholder X11 window into a full-screen Mali `fbdev_window`. It also works around two blob quirks: no config advertises `EGL_WINDOW_BIT` (window surfaces still work), and window surfaces fail with `EGL_BAD_MATCH` unless fb0 is in a 32-bpp mode (it sets 720x480 ARGB, double-buffered). |
| `shim/fakex11.c` → `lib/libX11.so.6` | Dawn validates Xlib windows through `XGetWindowAttributes`; this stand-in reports a valid 720x480 window. Ship it only in the port's private `lib/`. |
| `shim/glibc238compat.c`, `shim/libstdcxx13compat.cpp` | The prebuilt Dawn archive was compiled against glibc 2.38 / GCC 13 libstdc++; the firmware has glibc 2.35 / GCC 12. These supply the five missing symbols at link time. |
| `shim/dawntest.cpp` | Standalone smoke test: clears the screen red/green/blue through Dawn → GLES → fbegl → Mali. |

## Installing

Copy to the SD card's `Roms/PORTS/`:

```
Roms/PORTS/Super Smash Bros. Melee.sh
Roms/PORTS/Imgs/Super Smash Bros. Melee.png      (optional menu art)
Roms/PORTS/melee/melee                           (the binary)
Roms/PORTS/melee/melee.gptk
Roms/PORTS/melee/resources/
Roms/PORTS/melee/lib/libEGL.so
Roms/PORTS/melee/lib/libX11.so.6
Roms/PORTS/melee/lib/libSDL3.so.0
Roms/PORTS/melee/portable.txt                    (keeps settings/saves in melee/user/)
Roms/PORTS/melee/disc/<your GALE01 image>.iso
```

Then start **Super Smash Bros. Melee** from the Ports menu. The log is
`melee/log.txt` (launcher + shim) and `melee/melee.log` (game).

## Controls

| Handheld | GameCube |
|---|---|
| Left stick / D-pad | Control stick / D-pad |
| Right stick | C-stick |
| A / B / X / Y (printed labels) | A / B / X / Y |
| R1 | Z |
| L2 / R2 | L / R (digital, full press) |
| Start | Start |
| Select | Settings overlay (remap controls here) |
| Select + Start | Quit to the menu |

## Building

On the device (stock firmware userland is Ubuntu 22.04 aarch64), with
`gcc-12`, `g++-12`, `ninja-build` and CMake >= 3.25:

```sh
CMAKE=/path/to/cmake JOBS=3 sh platforms/rg34xx/build-on-device.sh <source dir> <build dir>
```

The script builds the shims, configures with the options above and patches
SDL's offscreen desktop size. A full build takes a long time on the H700, and
the build tree needs roughly 1.5 GB of free space.

> Warning: do not `apt-get install` packages blindly on the stock firmware.
> Its multiarch armhf packages can make apt swap systemd to armhf and remove
> NetworkManager. Simulate with `apt-get -s` first.
