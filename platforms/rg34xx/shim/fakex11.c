/* fakex11.c - stand-in libX11.so.6 for melee-pc on Mali fbdev.
 *
 * Dawn validates an Xlib surface by calling XGetWindowAttributes through a
 * dlopened libX11 before creating the EGL surface. There is no X server on
 * the device; the window id is only a token that fbegl.c replaces with an
 * fbdev window, so report a valid 720x480 window. Ship this only in the
 * port's private library directory.
 */
#include <stdlib.h>
#include <string.h>

typedef int (*XErrorHandler)(void*, void*);
static XErrorHandler g_handler;

typedef struct {
    int x, y, width, height, border_width, depth;
    void* visual;
    unsigned long root;
    int class_, bit_gravity, win_gravity, backing_store;
    unsigned long backing_planes, backing_pixel;
    int save_under;
    unsigned long colormap;
    int map_installed, map_state;
    long all_event_masks, your_event_mask, do_not_propagate_mask;
    int override_redirect;
    void* screen;
} XWindowAttributes;

XErrorHandler XSetErrorHandler(XErrorHandler h) {
    XErrorHandler old = g_handler;
    g_handler = h;
    return old;
}

int XGetWindowAttributes(void* dpy, unsigned long win, XWindowAttributes* a) {
    (void)dpy; (void)win;
    memset(a, 0, sizeof *a);
    const char* w = getenv("FBEGL_W");
    const char* h = getenv("FBEGL_H");
    a->width = w ? atoi(w) : 720;
    a->height = h ? atoi(h) : 480;
    a->depth = 24;
    a->map_state = 2; /* IsViewable */
    return 1;
}

void* XSynchronize(void* dpy, int onoff) { (void)dpy; (void)onoff; return NULL; }
void* XOpenDisplay(const char* name) { (void)name; return NULL; }
