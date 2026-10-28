#!/bin/sh
# SDL's offscreen driver reports a fixed 1024x768 desktop; make it the panel's 720x480 so
# fullscreen matches the framebuffer (override with SDL_OFFSCREEN_W/H). Usage: <build dir>
OFF=$(find "$1" -path '*src/video/offscreen/SDL_offscreenvideo.c' | head -1)
[ -n "$OFF" ] || { echo "SDL offscreen source not found under $1" >&2; exit 1; }
grep -q 'SDL_OFFSCREEN_W' "$OFF" && exit 0
python3 - "$OFF" <<'PY'
import re, sys
p = sys.argv[1]
s = open(p).read()
s2 = re.sub(r'mode\.w = 1024;\s*\n(\s*)mode\.h = 768;',
            lambda m: 'mode.w = SDL_getenv("SDL_OFFSCREEN_W") ? SDL_atoi(SDL_getenv("SDL_OFFSCREEN_W")) : 720;\n'
                      + m.group(1) + 'mode.h = SDL_getenv("SDL_OFFSCREEN_H") ? SDL_atoi(SDL_getenv("SDL_OFFSCREEN_H")) : 480;', s)
if s2 == s:
    sys.exit("offscreen mode lines not found in " + p)
open(p, "w").write(s2)
print("patched", p)
PY
