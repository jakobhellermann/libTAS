//# link_flags: -lGL -ludev
// Enumerating with several subsystem matches must still find libTAS' emulated joystick devices:
// the matches are stored in a vector that grows while they are added. Prints the matching devices
// once, then one "TEST:" line per frame and exits at frame END_FRAME.
#define _GNU_SOURCE
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <libudev.h>
#include <stdio.h>
#include <string.h>

#define END_FRAME 3

static void print_input_devices(void) {
    struct udev *udev = udev_new();
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    const char *subsystems[] = {"input", "hidraw", "usb", "sound", "drm"};
    for (size_t i = 0; i < sizeof(subsystems) / sizeof(subsystems[0]); i++)
        udev_enumerate_add_match_subsystem(enumerate, subsystems[i]);
    udev_enumerate_scan_devices(enumerate);
    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {
        const char *path = udev_list_entry_get_name(entry);
        const char *name = strrchr(path, '/');
        if (strncmp(name, "/event", 6) == 0 || strncmp(name, "/js", 3) == 0)
            fprintf(stderr, "TEST: device %s\n", name + 1);
    }
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
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

    print_input_devices();

    for (int frame = 1;; frame++) {
        fprintf(stderr, "TEST: frame %d\n", frame);
        if (frame == END_FRAME) break;
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(dpy, win);
    }
    return 0;
}
