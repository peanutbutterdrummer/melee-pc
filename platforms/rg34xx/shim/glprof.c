/* glprof.c - optional GL/EGL call profiler for the fbegl shim (FBEGL_PROF=1).
 *
 * Dawn loads every GL and EGL entry point through eglGetProcAddress, so the shim
 * can hand back timed wrappers for the calls that can stall a Mali driver
 * (uploads, buffer writes, maps, syncs, context switches, draws). Every
 * FBEGL_PROF_SECS (default 2) seconds it prints, per call: count, total ms,
 * max ms and bytes, then resets.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void (*proc_t)(void);
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLuint;
typedef unsigned int GLbitfield;
typedef unsigned char GLboolean;
typedef intptr_t GLintptr;
typedef intptr_t GLsizeiptr;
typedef void* GLsync;
typedef uint64_t GLuint64;

enum {
    P_glTexSubImage2D, P_glTexSubImage3D, P_glCompressedTexSubImage2D, P_glCompressedTexSubImage3D,
    P_glTexImage2D, P_glTexStorage2D, P_glTexStorage3D, P_glBufferSubData, P_glBufferData,
    P_glCopyBufferSubData, P_glMapBufferRange, P_glUnmapBuffer, P_glFlushMappedBufferRange,
    P_glDrawArrays, P_glDrawArraysInstanced, P_glDrawElements, P_glDrawElementsInstanced,
    P_glDrawElementsInstancedBaseVertex, P_glDrawElementsBaseVertex, P_glDrawRangeElements, P_glFlush,
    P_glFinish, P_glFenceSync, P_glClientWaitSync, P_glWaitSync, P_glBlitFramebuffer, P_glReadPixels,
    P_glClear, P_glClearBufferfv, P_glCopyTexSubImage2D, P_glGenerateMipmap, P_glLinkProgram,
    P_glCompileShader, P_glUseProgram, P_glBindFramebuffer, P_glBindTexture, P_glUniform1uiv,
    P_glBindBufferRange, P_glBindBufferBase, P_eglMakeCurrent, P_eglSwapBuffers, P_COUNT
};
static const char* k_names[P_COUNT] = {
    "glTexSubImage2D", "glTexSubImage3D", "glCompressedTexSubImage2D", "glCompressedTexSubImage3D",
    "glTexImage2D", "glTexStorage2D", "glTexStorage3D", "glBufferSubData", "glBufferData",
    "glCopyBufferSubData", "glMapBufferRange", "glUnmapBuffer", "glFlushMappedBufferRange",
    "glDrawArrays", "glDrawArraysInstanced", "glDrawElements", "glDrawElementsInstanced",
    "glDrawElementsInstancedBaseVertex", "glDrawElementsBaseVertex", "glDrawRangeElements", "glFlush",
    "glFinish", "glFenceSync", "glClientWaitSync", "glWaitSync", "glBlitFramebuffer", "glReadPixels",
    "glClear", "glClearBufferfv", "glCopyTexSubImage2D", "glGenerateMipmap", "glLinkProgram",
    "glCompileShader", "glUseProgram", "glBindFramebuffer", "glBindTexture", "glUniform1uiv",
    "glBindBufferRange", "glBindBufferBase", "eglMakeCurrent", "eglSwapBuffers",
};
static proc_t g_real[P_COUNT];
static uint64_t g_count[P_COUNT], g_ns[P_COUNT], g_max[P_COUNT], g_bytes[P_COUNT];
static uint64_t g_last_report;
static int g_enabled = -1;
static pthread_mutex_t g_report_lock = PTHREAD_MUTEX_INITIALIZER;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void report(uint64_t t) {
    static uint64_t secs;
    if (!secs) {
        const char* s = getenv("FBEGL_PROF_SECS");
        secs = (uint64_t)(s ? atoi(s) : 2);
        if (!secs) secs = 2;
    }
    if (!g_last_report) { g_last_report = t; return; }
    if (t - g_last_report < secs * 1000000000u) return;
    if (pthread_mutex_trylock(&g_report_lock) != 0) return;
    const double window = (double)(t - g_last_report) / 1e9;
    g_last_report = t;
    fprintf(stderr, "[glprof] --- %.2f s window ---\n", window);
    for (int i = 0; i < P_COUNT; i++) {
        uint64_t c = __atomic_exchange_n(&g_count[i], 0, __ATOMIC_RELAXED);
        uint64_t ns = __atomic_exchange_n(&g_ns[i], 0, __ATOMIC_RELAXED);
        uint64_t mx = __atomic_exchange_n(&g_max[i], 0, __ATOMIC_RELAXED);
        uint64_t b = __atomic_exchange_n(&g_bytes[i], 0, __ATOMIC_RELAXED);
        if (!c) continue;
        fprintf(stderr, "[glprof] %-34s n=%7llu total=%9.1f ms max=%8.2f ms bytes=%llu\n", k_names[i],
                (unsigned long long)c, ns / 1e6, mx / 1e6, (unsigned long long)b);
    }
    pthread_mutex_unlock(&g_report_lock);
}

static void add(int i, uint64_t t0, uint64_t bytes) {
    uint64_t t1 = now_ns(), d = t1 - t0;
    __atomic_add_fetch(&g_count[i], 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&g_ns[i], d, __ATOMIC_RELAXED);
    __atomic_add_fetch(&g_bytes[i], bytes, __ATOMIC_RELAXED);
    uint64_t m = __atomic_load_n(&g_max[i], __ATOMIC_RELAXED);
    while (d > m && !__atomic_compare_exchange_n(&g_max[i], &m, d, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
    }
    report(t1);
}

#define R(i, type) ((type)g_real[i])
#define VOIDW(name, params, args, bytes)                                                                   \
    static void w_##name params {                                                                          \
        uint64_t t0 = now_ns();                                                                            \
        R(P_##name, void(*) params) args;                                                                  \
        add(P_##name, t0, (uint64_t)(bytes));                                                              \
    }
#define RETW(ret, name, params, args, bytes)                                                               \
    static ret w_##name params {                                                                           \
        uint64_t t0 = now_ns();                                                                            \
        ret r = R(P_##name, ret(*) params) args;                                                           \
        add(P_##name, t0, (uint64_t)(bytes));                                                              \
        return r;                                                                                          \
    }

VOIDW(glTexSubImage2D, (GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void* p),
      (t, l, x, y, w, h, f, ty, p), (uint64_t)w * h * 4)
VOIDW(glTexSubImage3D,
      (GLenum t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d, GLenum f, GLenum ty, const void* p),
      (t, l, x, y, z, w, h, d, f, ty, p), (uint64_t)w * h * d * 4)
VOIDW(glCompressedTexSubImage2D,
      (GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLsizei n, const void* p),
      (t, l, x, y, w, h, f, n, p), n)
VOIDW(glCompressedTexSubImage3D,
      (GLenum t, GLint l, GLint x, GLint y, GLint z, GLsizei w, GLsizei h, GLsizei d, GLenum f, GLsizei n, const void* p),
      (t, l, x, y, z, w, h, d, f, n, p), n)
VOIDW(glTexImage2D, (GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void* p),
      (t, l, i, w, h, b, f, ty, p), (uint64_t)w * h * 4)
VOIDW(glTexStorage2D, (GLenum t, GLsizei l, GLenum f, GLsizei w, GLsizei h), (t, l, f, w, h), 0)
VOIDW(glTexStorage3D, (GLenum t, GLsizei l, GLenum f, GLsizei w, GLsizei h, GLsizei d), (t, l, f, w, h, d), 0)
VOIDW(glBufferSubData, (GLenum t, GLintptr o, GLsizeiptr s, const void* p), (t, o, s, p), s)
VOIDW(glBufferData, (GLenum t, GLsizeiptr s, const void* p, GLenum u), (t, s, p, u), s)
VOIDW(glCopyBufferSubData, (GLenum r, GLenum w, GLintptr ro, GLintptr wo, GLsizeiptr s), (r, w, ro, wo, s), s)
RETW(void*, glMapBufferRange, (GLenum t, GLintptr o, GLsizeiptr l, GLbitfield a), (t, o, l, a), l)
RETW(GLboolean, glUnmapBuffer, (GLenum t), (t), 0)
VOIDW(glFlushMappedBufferRange, (GLenum t, GLintptr o, GLsizeiptr l), (t, o, l), l)
VOIDW(glDrawArrays, (GLenum m, GLint f, GLsizei c), (m, f, c), 0)
VOIDW(glDrawArraysInstanced, (GLenum m, GLint f, GLsizei c, GLsizei n), (m, f, c, n), 0)
VOIDW(glDrawElements, (GLenum m, GLsizei c, GLenum t, const void* p), (m, c, t, p), 0)
VOIDW(glDrawElementsInstanced, (GLenum m, GLsizei c, GLenum t, const void* p, GLsizei n), (m, c, t, p, n), 0)
VOIDW(glDrawElementsInstancedBaseVertex, (GLenum m, GLsizei c, GLenum t, const void* p, GLsizei n, GLint b),
      (m, c, t, p, n, b), 0)
VOIDW(glDrawElementsBaseVertex, (GLenum m, GLsizei c, GLenum t, const void* p, GLint b), (m, c, t, p, b), 0)
VOIDW(glDrawRangeElements, (GLenum m, GLuint s, GLuint e, GLsizei c, GLenum t, const void* p), (m, s, e, c, t, p), 0)
VOIDW(glFlush, (void), (), 0)
VOIDW(glFinish, (void), (), 0)
RETW(GLsync, glFenceSync, (GLenum c, GLbitfield f), (c, f), 0)
RETW(GLenum, glClientWaitSync, (GLsync s, GLbitfield f, GLuint64 t), (s, f, t), 0)
VOIDW(glWaitSync, (GLsync s, GLbitfield f, GLuint64 t), (s, f, t), 0)
VOIDW(glBlitFramebuffer,
      (GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h, GLbitfield m, GLenum fl),
      (a, b, c, d, e, f, g, h, m, fl), 0)
VOIDW(glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void* p), (x, y, w, h, f, t, p),
      (uint64_t)w * h * 4)
VOIDW(glClear, (GLbitfield m), (m), 0)
VOIDW(glClearBufferfv, (GLenum b, GLint d, const float* v), (b, d, v), 0)
VOIDW(glCopyTexSubImage2D, (GLenum t, GLint l, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h),
      (t, l, xo, yo, x, y, w, h), (uint64_t)w * h * 4)
VOIDW(glGenerateMipmap, (GLenum t), (t), 0)
VOIDW(glLinkProgram, (GLuint p), (p), 0)
VOIDW(glCompileShader, (GLuint s), (s), 0)
VOIDW(glUseProgram, (GLuint p), (p), 0)
VOIDW(glBindFramebuffer, (GLenum t, GLuint f), (t, f), 0)
VOIDW(glBindTexture, (GLenum t, GLuint x), (t, x), 0)
VOIDW(glUniform1uiv, (GLint l, GLsizei c, const GLuint* v), (l, c, v), (uint64_t)c * 4)
VOIDW(glBindBufferRange, (GLenum t, GLuint i, GLuint b, GLintptr o, GLsizeiptr s), (t, i, b, o, s), 0)
VOIDW(glBindBufferBase, (GLenum t, GLuint i, GLuint b), (t, i, b), 0)
RETW(unsigned int, eglMakeCurrent, (void* d, void* dr, void* rd, void* c), (d, dr, rd, c), 0)
RETW(unsigned int, eglSwapBuffers, (void* d, void* s), (d, s), 0)

static const proc_t k_wrappers[P_COUNT] = {
    (proc_t)w_glTexSubImage2D, (proc_t)w_glTexSubImage3D, (proc_t)w_glCompressedTexSubImage2D,
    (proc_t)w_glCompressedTexSubImage3D, (proc_t)w_glTexImage2D, (proc_t)w_glTexStorage2D,
    (proc_t)w_glTexStorage3D, (proc_t)w_glBufferSubData, (proc_t)w_glBufferData, (proc_t)w_glCopyBufferSubData,
    (proc_t)w_glMapBufferRange, (proc_t)w_glUnmapBuffer, (proc_t)w_glFlushMappedBufferRange,
    (proc_t)w_glDrawArrays, (proc_t)w_glDrawArraysInstanced, (proc_t)w_glDrawElements,
    (proc_t)w_glDrawElementsInstanced, (proc_t)w_glDrawElementsInstancedBaseVertex,
    (proc_t)w_glDrawElementsBaseVertex, (proc_t)w_glDrawRangeElements, (proc_t)w_glFlush, (proc_t)w_glFinish,
    (proc_t)w_glFenceSync, (proc_t)w_glClientWaitSync, (proc_t)w_glWaitSync, (proc_t)w_glBlitFramebuffer,
    (proc_t)w_glReadPixels, (proc_t)w_glClear, (proc_t)w_glClearBufferfv, (proc_t)w_glCopyTexSubImage2D,
    (proc_t)w_glGenerateMipmap, (proc_t)w_glLinkProgram, (proc_t)w_glCompileShader, (proc_t)w_glUseProgram,
    (proc_t)w_glBindFramebuffer, (proc_t)w_glBindTexture, (proc_t)w_glUniform1uiv, (proc_t)w_glBindBufferRange,
    (proc_t)w_glBindBufferBase, (proc_t)w_eglMakeCurrent, (proc_t)w_eglSwapBuffers,
};

/* Returns a timed wrapper for `name` when profiling is on, else `real`. */
proc_t glprof_wrap(const char* name, proc_t real) {
    if (g_enabled < 0) g_enabled = getenv("FBEGL_PROF") && getenv("FBEGL_PROF")[0] == '1';
    if (!g_enabled || !real) return real;
    for (int i = 0; i < P_COUNT; i++) {
        if (strcmp(name, k_names[i]) == 0) {
            g_real[i] = real;
            return k_wrappers[i];
        }
    }
    return real;
}
