#!/bin/bash
PORTS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"  # [rg34xxsp-stock] ports live next to this script
# melee-pc (native Super Smash Bros. Melee port) for the Anbernic RG34XX(SP) stock firmware.
# Renders through Dawn's OpenGL ES backend straight to the Mali framebuffer via
# melee/lib/libEGL.so (fbegl shim) and a stand-in libX11; see README-rg34xx.md.
# Disc: put your own NTSC-U 1.02 (GALE01 rev 2) image in melee/disc/.
pm_finish() { pkill -9 gptokeyb 2>/dev/null; }

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
[ -f "$controlfolder/control.txt" ] && source "$controlfolder/control.txt"
GPTOKEYB="${GPTOKEYB:-$controlfolder/gptokeyb -1}"

GAMEDIR="$PORTS_DIR/melee"
cd "$GAMEDIR"
exec > "$GAMEDIR/log.txt" 2>&1
echo "[rg34xx] start $(date)"

DISC=$(ls "$GAMEDIR"/disc/*.iso "$GAMEDIR"/disc/*.gcm "$GAMEDIR"/disc/*.rvz "$GAMEDIR"/disc/*.ciso 2>/dev/null | head -1)

# Our libEGL.so / libX11.so.6 and the bundled libSDL3 must come first. The
# firmware's Mali EGL lives in /usr/lib (fbegl forwards to /usr/lib/libEGL.so.1).
export LD_LIBRARY_PATH="$GAMEDIR/lib:/usr/lib/aarch64-linux-gnu:/usr/lib"
export HOME="${HOME:-/root}"
export SDL_VIDEO_DRIVER=offscreen
export MELEE_BACKEND=gles
export MELEE_LOG_FILE="$GAMEDIR/melee.log"
# Built-in pad, modern Smash layout: A attack, B special, X/Y jump, L1/R1 grab (Z),
# L2/R2 shield (L/R), right stick = C-stick, Select = settings overlay,
# Select+Start = quit.
export AURORA_PAD_EXTRA_BUTTONS="leftshoulder:z"
# Rumble: the vibration motor is an on/off switch on the AXP2202 PMIC.
MOTOR=/sys/class/power_supply/axp2202-battery/moto
[ -w "$MOTOR" ] && export AURORA_RUMBLE_SYSFS="$MOTOR"
export SDL_GAMECONTROLLERCONFIG="19000000010000000100000000010000,ANBERNIC-keys,a:b0,b:b1,x:b3,y:b2,back:b8,guide:b6,start:b7,leftstick:b9,rightstick:b12,leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:b10,righttrigger:b11,platform:Linux,"

# Settings live in melee/user/ (portable.txt). First run: the cheapest settings
# (1x internal resolution, no AA/anisotropy, no reverb). Always fullscreen: the
# window is the 720x480 framebuffer; windowed mode would render a 1280x960 window.
CFG="$GAMEDIR/user/launcher.cfg"
mkdir -p "$GAMEDIR/user"
[ -f "$CFG" ] || printf 'render_scale 1\nmsaa 1\nanisotropy 1\nreverb 0\n' > "$CFG"
if grep -q '^fullscreen ' "$CFG"; then
  sed -i 's/^fullscreen .*/fullscreen 1/' "$CFG"
else
  echo 'fullscreen 1' >> "$CFG"
fi

chmod 666 /dev/uinput 2>/dev/null
# Select+Start quits: gptokeyb killall()s by process name, and aurora renames the
# game's main thread to "Main thread".
$GPTOKEYB "Main thread" -c "$GAMEDIR/melee.gptk" &
./melee ${DISC:+"$DISC"}
echo "[rg34xx] exit status $?"
pm_finish
# A crash or kill mid-rumble would leave the motor running.
[ -w "$MOTOR" ] && echo 0 > "$MOTOR"
sync
