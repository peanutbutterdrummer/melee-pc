/* fbegl.c - libEGL.so shim for melee-pc on Mali fbdev (RG34XX stock firmware).
 *
 * Dawn's GLES backend dlopens "libEGL.so", loads every entry point through
 * eglGetProcAddress, and creates its window surface from an X11 window id.
 * The firmware's Mali blob is an fbdev build: no X11, and its native window
 * type is a pointer to struct fbdev_window. This shim forwards everything to
 * the real driver and replaces the native window with a full-screen
 * fbdev_window.
 *
 * The blob also reports no EGL_WINDOW_BIT on any config (only pbuffer and
 * pixmap), although window surfaces work; eglChooseConfig drops that bit
 * from the request so Dawn finds its RGBA8888 config.
 *
 * Env: FBEGL_REAL (default /usr/lib/libEGL.so.1), FBEGL_W/FBEGL_H (default
 * 720x480), FBEGL_LOG=1 for call logging on stderr, FBEGL_KEEPMODE=1 to leave
 * the framebuffer mode alone, FBEGL_PROF=1 for GL call timing (glprof.c).
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLSurface;
typedef intptr_t EGLAttrib;
typedef int32_t EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void (*proc_t)(void);

struct fbdev_window { unsigned short width, height; };
static struct fbdev_window g_win;

static void* g_real;
static proc_t (*real_gpa)(const char*);
static int g_log;

#define LOG(...) do { if (g_log) fprintf(stderr, "[fbegl] " __VA_ARGS__); } while (0)

static void fb_size(void) {
    /* fb0 reports an HDMI-sized 1280x1024 mode, but the panel (and every SDL
     * port on this firmware) uses a 720x480 window; the display layer scales
     * it for HDMI. */
    const char* ew = getenv("FBEGL_W");
    const char* eh = getenv("FBEGL_H");
    unsigned w = ew ? (unsigned)atoi(ew) : 720;
    unsigned h = eh ? (unsigned)atoi(eh) : 480;
    g_win.width = (unsigned short)(w ? w : 720);
    g_win.height = (unsigned short)(h ? h : 480);
}

/* After the menu exits uncleanly, fb0 is left at a bogus 1280x1024@16 mode in
 * which the blob refuses every window surface (EGL_BAD_MATCH). Put it in the
 * mode games normally inherit: WxH, 32 bpp ARGB, double-buffered. */
static void fb_mode(void) {
    if (getenv("FBEGL_KEEPMODE")) return;
    /* FBEGL_BUFFERS=3: triple buffering. With two buffers and vblank-locked
     * swaps, a 23 ms frame waits for the 33 ms vblank; a third buffer lets the
     * GPU start the next frame instead. */
    const char* eb = getenv("FBEGL_BUFFERS");
    unsigned nbuf = eb ? (unsigned)atoi(eb) : 2;
    if (nbuf < 2 || nbuf > 4) nbuf = 2;
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) return;
    struct fb_var_screeninfo v;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &v) == 0 &&
        (v.xres != g_win.width || v.yres != g_win.height || v.bits_per_pixel != 32 ||
         v.yres_virtual != g_win.height * nbuf)) {
        LOG("fb0 was %ux%u@%u, setting %ux%u@32 x%u\n", v.xres, v.yres, v.bits_per_pixel, g_win.width,
            g_win.height, nbuf);
        v.xres = v.xres_virtual = g_win.width;
        v.yres = g_win.height;
        v.yres_virtual = g_win.height * nbuf;
        v.xoffset = v.yoffset = 0;
        v.bits_per_pixel = 32;
        v.red = (struct fb_bitfield){16, 8, 0};
        v.green = (struct fb_bitfield){8, 8, 0};
        v.blue = (struct fb_bitfield){0, 8, 0};
        v.transp = (struct fb_bitfield){24, 8, 0};
        if (ioctl(fd, FBIOPUT_VSCREENINFO, &v) != 0) LOG("FBIOPUT_VSCREENINFO failed\n");
    }
    close(fd);
}

static void init(void) {
    if (g_real) return;
    g_log = getenv("FBEGL_LOG") && getenv("FBEGL_LOG")[0] == '1';
    const char* path = getenv("FBEGL_REAL");
    if (!path) path = "/usr/lib/libEGL.so.1";
    g_real = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!g_real) {
        fprintf(stderr, "[fbegl] cannot open %s: %s\n", path, dlerror());
        abort();
    }
    real_gpa = (proc_t (*)(const char*))dlsym(g_real, "eglGetProcAddress");
    fb_size();
    fb_mode();
    LOG("real=%s window=%ux%u\n", path, g_win.width, g_win.height);
}

static proc_t real_proc(const char* name) {
    init();
    proc_t p = (proc_t)dlsym(g_real, name);
    if (!p && real_gpa) p = real_gpa(name);
    return p;
}

typedef EGLSurface (*create_window_surface_fn)(EGLDisplay, EGLConfig, void*, const EGLint*);
typedef EGLDisplay (*get_display_fn)(void*);
typedef const char* (*query_string_fn)(EGLDisplay, EGLint);

#define REAL(type, name) static type real_##name; if (!real_##name) real_##name = (type)real_proc(#name)

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig cfg, uintptr_t win, const EGLint* attribs) {
    REAL(create_window_surface_fn, eglCreateWindowSurface);
    if (getenv("FBEGL_PASSTHRU")) {
        /* Diagnostic: forward the caller's native window unchanged and dump it. */
        const unsigned char* b = (const unsigned char*)win;
        EGLSurface ps = real_eglCreateWindowSurface(dpy, cfg, (void*)win, attribs);
        fprintf(stderr, "[fbegl] passthru win=%#lx bytes=", (unsigned long)win);
        for (int i = 0; b && i < 16; i++) fprintf(stderr, "%02x", b[i]);
        fprintf(stderr, " -> %p\n", ps);
        return ps;
    }
    EGLSurface s = real_eglCreateWindowSurface(dpy, cfg, &g_win, attribs);
    LOG("eglCreateWindowSurface(win=%#lx) -> fbdev %ux%u = %p\n", (unsigned long)win, g_win.width,
        g_win.height, s);
    return s;
}

EGLSurface eglCreatePlatformWindowSurface(EGLDisplay dpy, EGLConfig cfg, void* win, const EGLAttrib* attribs) {
    /* No platform extensions on fbdev; translate the attrib list to EGLint. */
    EGLint a[64];
    int n = 0;
    while (attribs && attribs[n] != 0x3038 /* EGL_NONE */ && n < 62) { a[n] = (EGLint)attribs[n]; n++; }
    a[n] = 0x3038;
    LOG("eglCreatePlatformWindowSurface(win=%p)\n", win);
    return eglCreateWindowSurface(dpy, cfg, (uintptr_t)win, a);
}

EGLSurface eglCreatePlatformWindowSurfaceEXT(EGLDisplay dpy, EGLConfig cfg, void* win, const EGLint* attribs) {
    LOG("eglCreatePlatformWindowSurfaceEXT(win=%p)\n", win);
    return eglCreateWindowSurface(dpy, cfg, (uintptr_t)win, attribs);
}

EGLDisplay eglGetPlatformDisplay(EGLenum platform, void* native, const EGLAttrib* attribs) {
    REAL(get_display_fn, eglGetDisplay);
    LOG("eglGetPlatformDisplay(%#x, %p) -> default display\n", platform, native);
    (void)attribs;
    return real_eglGetDisplay((void*)0);
}

EGLDisplay eglGetPlatformDisplayEXT(EGLenum platform, void* native, const EGLint* attribs) {
    (void)attribs;
    return eglGetPlatformDisplay(platform, native, NULL);
}

const char* eglQueryString(EGLDisplay dpy, EGLint name) {
    REAL(query_string_fn, eglQueryString);
    const char* s = real_eglQueryString(dpy, name);
    LOG("eglQueryString(%p, %#x) = %s\n", dpy, name, s ? s : "(null)");
    return s;
}

typedef EGLBoolean (*choose_config_fn)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint* attribs, EGLConfig* configs, EGLint size, EGLint* num) {
    REAL(choose_config_fn, eglChooseConfig);
    EGLint a[128];
    int n = 0;
    while (attribs && attribs[n] != 0x3038 /* EGL_NONE */ && n < 126) {
        a[n] = attribs[n];
        a[n + 1] = attribs[n + 1];
        if (a[n] == 0x3033 /* EGL_SURFACE_TYPE */) a[n + 1] &= ~0x0004 /* EGL_WINDOW_BIT */;
        n += 2;
    }
    a[n] = 0x3038;
    EGLBoolean ok = real_eglChooseConfig(dpy, a, configs, size, num);
    LOG("eglChooseConfig -> ok=%u num=%d\n", ok, num ? *num : -1);
    return ok;
}

static const struct { const char* name; proc_t fn; } k_overrides[] = {
    {"eglCreateWindowSurface", (proc_t)eglCreateWindowSurface},
    {"eglCreatePlatformWindowSurface", (proc_t)eglCreatePlatformWindowSurface},
    {"eglCreatePlatformWindowSurfaceEXT", (proc_t)eglCreatePlatformWindowSurfaceEXT},
    {"eglGetPlatformDisplay", (proc_t)eglGetPlatformDisplay},
    {"eglGetPlatformDisplayEXT", (proc_t)eglGetPlatformDisplayEXT},
    {"eglQueryString", (proc_t)eglQueryString},
    {"eglChooseConfig", (proc_t)eglChooseConfig},
};

proc_t glprof_wrap(const char* name, proc_t real); /* glprof.c, FBEGL_PROF=1 */

proc_t eglGetProcAddress(const char* name) {
    init();
    for (size_t i = 0; i < sizeof k_overrides / sizeof k_overrides[0]; i++)
        if (strcmp(name, k_overrides[i].name) == 0) return k_overrides[i].fn;
    proc_t p = real_proc(name);
    if (!p) LOG("eglGetProcAddress(%s) = NULL\n", name);
    return glprof_wrap(name, p);
}
