//# link_flags: -lGL -lpthread
// Joining a game thread from code libTAS runs natively must wait for it, like a driver joining
// its own threads while it presents. An X error handler runs when libTAS reads the X error
// during the frame boundary, and joins a thread that is still sleeping there. Prints one "TEST:"
// line per frame and exits at frame END_FRAME.
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define END_FRAME 10
#define JOIN_FRAME 5

static pthread_t sleeper;
static volatile int sleeper_done;
static int join_result = -1;
static int joined_done = -1;

static void *sleep_then_finish(void *arg) {
    (void)arg;
    struct timespec ts = {0, 200 * 1000 * 1000};
    nanosleep(&ts, NULL);
    sleeper_done = 1;
    return NULL;
}

static int on_x_error(Display *dpy, XErrorEvent *ev) {
    (void)dpy;
    (void)ev;
    if (join_result == -1) {
        join_result = pthread_join(sleeper, NULL);
        joined_done = sleeper_done;
    }
    return 0;
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
    XSetErrorHandler(on_x_error);

    for (int frame = 1;; frame++) {
        fprintf(stderr, "TEST: frame %d\n", frame);
        if (join_result >= 0) {
            fprintf(stderr, "TEST: join returned %d, thread finished %d\n", join_result, joined_done);
            join_result = -2;
        }
        if (frame == END_FRAME) break;
        if (frame == JOIN_FRAME) {
            if (pthread_create(&sleeper, NULL, sleep_then_finish, NULL) != 0) {
                fprintf(stderr, "TEST: FAIL pthread_create\n");
                return 1;
            }
            // The error reply arrives while libTAS reads from the connection natively.
            XFreePixmap(dpy, 0x1fffffff);
        }
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(dpy, win);
    }
    return 0;
}
