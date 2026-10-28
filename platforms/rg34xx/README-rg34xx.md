# melee-pc on the Anbernic RG34XX / RG34XXSP (stock firmware)

A build of [melee-pc](https://github.com/999sian/melee-pc) (the native port of
Super Smash Bros. Melee built from the [doldecomp/melee](https://github.com/doldecomp/melee)
decompilation) for Anbernic's H700 handhelds running the **stock Anbernic
firmware**. Tested on an RG34XXSP; other H700 models on stock firmware
(RG35XX Plus/H/SP/2024, RG40XX, RG28XX, RG CubeXX) use the same GPU, driver and
userland and will probably work, but are untested.

## You need your own game disc

**No game data is included, and none will be provided.** Make a backup of your
own disc: **Super Smash Bros. Melee, USA, revision 2 (NTSC-U 1.02, GALE01)**,
SHA-1 `d4e70c064cc714ba8400a849cf299dbd1aa326fc`. `.iso`, `.gcm`, `.ciso` and
`.rvz` images are accepted. Do not open issues asking where to get one.

## Status

| | |
|---|---|
| 1v1 match | ~30 fps |
| 4-player match | ~22 fps |
| Menus, intro | ~30 fps |
| Audio | full speed |
| Controls, rumble | working |
| Shader stutter | none with the bundled cache (see below) |

The frame rate is limited by CPU time inside the stock Mali driver (per-draw
overhead), not by the GPU clock. Frames are vsync-locked, so a 23 ms frame
shows as 30 fps. Testing on a mainline-kernel firmware (ROCKNIX, Panfrost
driver) is the next step toward 60 fps.

## Installing

1. Download `melee-rg34xx-stock-<version>.zip` from the
   [releases page](../../releases) and extract it into the SD card's
   `Roms/PORTS/` folder:

   ```
   Roms/PORTS/Super Smash Bros. Melee.sh
   Roms/PORTS/melee/melee                     (the game)
   Roms/PORTS/melee/lib/                      (EGL and X11 shims)
   Roms/PORTS/melee/resources/
   Roms/PORTS/melee/shadercache/              (prebuilt shader cache)
   Roms/PORTS/melee/melee.gptk
   Roms/PORTS/melee/portable.txt              (keeps settings and saves in melee/user/)
   Roms/PORTS/melee/disc/                     (put your disc image here)
   ```

2. Copy your disc image into `Roms/PORTS/melee/disc/`.
3. Start **Super Smash Bros. Melee** from the Ports menu.

Logs: `melee/log.txt` (launcher and display shim) and `melee/melee.log` (game).
Settings and memory card saves are in `melee/user/`.

## Shader cache

The GameCube's graphics are rebuilt as thousands of GPU shader programs. On
this GPU each one takes ~0.1-0.2 s to compile, and an effect stays invisible
until its shader is ready, so a cold cache shows black screens and missing
effects for the first hours of play.

`melee/shadercache/` holds a cache compiled on an RG34XXSP with stock firmware
(6,275 pipelines: menus, stages, characters and Classic mode). The launcher
copies it into `melee/user/` on first start and never overwrites an existing
cache. On every launch the game loads all of it in the background in about a
minute, during the intro and menus. If your firmware's GPU driver differs, the
compiled half is simply ignored and rebuilt as you play.

To start over, delete `melee/user/pipeline_cache.db` and
`melee/user/dawn_cache.db`.

## Controls

Modern Smash layout:

| Handheld | Action | GameCube |
|---|---|---|
| Left stick / D-pad | Move / taunt | Control stick / D-pad |
| Right stick | Smash attacks | C-stick |
| A | Attack | A |
| B | Special | B |
| X / Y | Jump | X / Y |
| L1 / R1 | Grab | Z |
| L2 / R2 | Shield | L / R |
| Start | Pause | Start |
| Select | Settings overlay (remap controls here) | |
| Select + Start | Quit to the menu | |

Rumble uses the handheld's vibration motor (an on/off switch on the AXP2202
power chip) for player 1.

## How it works

melee-pc renders through Dawn (WebGPU), which on Linux normally uses Vulkan and
expects an X11 or Wayland window. The stock firmware has neither a Vulkan
driver for the Mali-G31 nor a window system, only the Mali r20p0 fbdev
OpenGL ES 3.2 driver.

| Piece | What it does |
|---|---|
| Dawn OpenGL ES backend | `MELEE_BACKEND=gles` selects Dawn's GLES 3.1 compatibility backend. |
| SDL3 `offscreen` video driver | SDL3 is built with `SDL_UNIX_CONSOLE_BUILD` (no X11/Wayland/KMSDRM) and only provides input, audio and a window object. Its fake desktop is patched to 720x480. |
| aurora: placeholder surface | When SDL runs the `offscreen` driver, aurora hands Dawn a placeholder Xlib surface (`extern/aurora/lib/dawn/BackendBinding.cpp`). |
| `shim/fbegl.c` → `lib/libEGL.so` | Forwards to the firmware's Mali EGL and turns the placeholder window into a full-screen Mali `fbdev_window`. Works around two driver quirks: no config advertises `EGL_WINDOW_BIT`, and window surfaces fail with `EGL_BAD_MATCH` unless fb0 is 720x480 at 32 bpp. `FBEGL_PROF=1` logs GL call timings. |
| `shim/fakex11.c` → `lib/libX11.so.6` | Answers Dawn's Xlib window checks. Ship it only in the port's private `lib/`. |
| `shim/glibc238compat.c`, `shim/libstdcxx13compat.cpp` | The prebuilt Dawn archive needs glibc 2.38 / GCC 13 symbols; the firmware has glibc 2.35 / GCC 12. |
| aurora: texture vertex fetch | The driver allows no storage buffers in vertex shaders, so vertex, index and per-draw data are read from `rgba8uint` textures (`gx/shader.cpp`). |
| aurora: per-frame resource slots | The driver CPU-copies any buffer written while the GPU still reads it. Buffers rotate through three slots and are uploaded once per frame. |
| aurora: shader-side index pulling | Indexed draws make the driver scan the index buffer on the CPU; indices are read in the vertex shader instead and draws are non-indexed. |
| aurora: mip count uniform | `textureNumLevels` costs a uniform upload per draw in Dawn's GL backend; the count is passed in an existing uniform. |
| aurora: `AURORA_PAD_EXTRA_BUTTONS` | Extra button bindings on top of the one-button-per-GC-button mapping (L1 grab). |
| aurora: `AURORA_RUMBLE_SYSFS` | On/off vibration motor in sysfs. |

## Building

Cross-compiling from Ubuntu 22.04 x86-64 (WSL2 works) with
`gcc-12-aarch64-linux-gnu`, `g++-12-aarch64-linux-gnu`, arm64 multiarch
development packages, Ninja and CMake >= 3.25:

```sh
sh platforms/rg34xx/build-cross.sh <source dir> <build dir>
```

A full build takes a few minutes. Building on the device itself
(`build-on-device.sh`) works but is slow and needs ~1.5 GB of free space;
the device has no swap.

> Warning: do not `apt-get install` packages blindly on the stock firmware.
> Its multiarch armhf packages can make apt swap systemd to armhf and remove
> NetworkManager (Wi-Fi). Simulate with `apt-get -s` first.

## License

The port code (`platforms/rg34xx/`, and this port's changes elsewhere) is
GPL-3.0-or-later, and the aurora changes are MIT like aurora itself. The
decompiled game code is not licensed by anyone; see
[`licenses/LICENSE.md`](../../licenses/LICENSE.md). Super Smash Bros. Melee is
© Nintendo / HAL Laboratory. This project is not affiliated with or endorsed
by Nintendo.
