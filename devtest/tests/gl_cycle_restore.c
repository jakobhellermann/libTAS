//# link_flags: -lGL
// Like gl_buffer_restore, but alternates saving and loading: each load restores a state saved
// after the previous load. Presenting must not use fences shared with the X server, whose state a
// load cannot rewind.
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define END_FRAME 200

static int maps_contain(const char *needle) {
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[512];
    int found = 0;
    while (maps && fgets(line, sizeof(line), maps))
        found |= strstr(line, needle) != NULL;
    if (maps) fclose(maps);
    return found;
}

static void check_gl(const char *what) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "TEST: FAIL %s: GL error 0x%x\n", what, err);
        exit(1);
    }
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "TEST: FAIL no display\n");
        return 1;
    }
    int attrs[] = {GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, None};
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), attrs);
    if (!vi) {
        fprintf(stderr, "TEST: FAIL no visual\n");
        return 1;
    }
    Window root = DefaultRootWindow(dpy);
    XSetWindowAttributes swa = {.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone)};
    Window win = XCreateWindow(dpy, root, 0, 0, 1920, 1080, 0, vi->depth, InputOutput, vi->visual,
                               CWColormap, &swa);
    XMapWindow(dpy, win);
    GLXContext ctx = glXCreateContext(dpy, vi, NULL, True);
    if (!ctx || !glXMakeCurrent(dpy, win, ctx)) {
        fprintf(stderr, "TEST: FAIL no GL context\n");
        return 1;
    }
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    fprintf(stderr, "INFO: renderer %s\n", renderer);
    const char *expect = getenv("TEST_EXPECT_GL");
    if (expect && !strstr(renderer, expect)) {
        fprintf(stderr, "TEST: FAIL renderer is not %s\n", expect);
        return 1;
    }

    GLuint buf;
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    uint32_t init[2] = {0, 0xC0FFEE};
    glBufferData(GL_ARRAY_BUFFER, sizeof(init), init, GL_DYNAMIC_DRAW);

    GLuint tex, fbo;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "TEST: FAIL incomplete framebuffer\n");
        return 1;
    }
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    check_gl("setup");

    for (uint32_t frame = 1;; frame++) {
        uint32_t expected = frame - 1;
        uint32_t got[2];
        glBindBuffer(GL_ARRAY_BUFFER, buf);
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(got), got);
        uint8_t pixel[4];
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        check_gl("readback");
        if (got[0] != expected || got[1] != 0xC0FFEE || pixel[0] != (expected & 0xff)) {
            fprintf(stderr, "TEST: FAIL frame %u: buffer %u/0x%x, texture %u, expected %u/0xc0ffee\n",
                    frame, got[0], got[1], pixel[0], expected);
            return 1;
        }
        fprintf(stderr, "TEST: frame %u ok\n", frame);
        if (frame == END_FRAME) {
            if (maps_contain("memfd:xshmfence")) {
                fprintf(stderr, "TEST: FAIL presenting uses xshmfences\n");
                return 1;
            }
            break;
        }

        glBufferSubData(GL_ARRAY_BUFFER, 0, 4, &frame);
        uint8_t texel[4] = {frame & 0xff, 0, 0, 0xff};
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, texel);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor((frame % 8) / 8.0f, 0.2f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        check_gl("draw");
        glXSwapBuffers(dpy, win);
    }
    return 0;
}
