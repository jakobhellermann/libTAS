//# link_flags: -lGL
// Two distinct shared memfds with the same name, mapped back to back, must both hold the previous
// frame number, also after loading a state. Prints one "TEST:" line per frame, exits at frame
// END_FRAME.
#define _GNU_SOURCE
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define END_FRAME 30

static volatile uint32_t *map_memfd(void *addr) {
    int fd = memfd_create("libtas-test-shm", 0);
    if (fd < 0 || ftruncate(fd, 4096) != 0) {
        fprintf(stderr, "TEST: FAIL memfd\n");
        exit(1);
    }
    void *p = mmap(addr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    if (p == MAP_FAILED) {
        fprintf(stderr, "TEST: FAIL mmap\n");
        exit(1);
    }
    return p;
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "TEST: FAIL no display\n");
        return 1;
    }
    int attrs[] = {GLX_RGBA, GLX_DOUBLEBUFFER, None};
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), attrs);
    if (!vi) {
        fprintf(stderr, "TEST: FAIL no visual\n");
        return 1;
    }
    Window root = DefaultRootWindow(dpy);
    XSetWindowAttributes swa = {.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone)};
    Window win = XCreateWindow(dpy, root, 0, 0, 320, 240, 0, vi->depth, InputOutput, vi->visual,
                               CWColormap, &swa);
    XMapWindow(dpy, win);
    GLXContext ctx = glXCreateContext(dpy, vi, NULL, True);
    if (!ctx || !glXMakeCurrent(dpy, win, ctx)) {
        fprintf(stderr, "TEST: FAIL no GL context\n");
        return 1;
    }

    // Reserve two adjacent pages, then replace each with its own memfd.
    char *base = mmap(NULL, 2 * 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        fprintf(stderr, "TEST: FAIL reserve\n");
        return 1;
    }
    volatile uint32_t *first = map_memfd(base);
    volatile uint32_t *second = map_memfd(base + 4096);
    *first = 0;
    *second = 0;

    for (uint32_t frame = 1;; frame++) {
        uint32_t expected = frame - 1;
        if (*first != expected || *second != expected) {
            fprintf(stderr, "TEST: FAIL frame %u: first %u, second %u, expected %u\n", frame, *first,
                    *second, expected);
            return 1;
        }
        fprintf(stderr, "TEST: frame %u ok\n", frame);
        if (frame == END_FRAME) break;
        *first = frame;
        *second = frame;
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(dpy, win);
    }
    return 0;
}
