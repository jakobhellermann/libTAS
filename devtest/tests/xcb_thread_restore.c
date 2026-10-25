//# link_flags: -lGL -lX11-xcb -lxcb -lpthread
// A second thread keeps issuing requests through libxcb's generated request functions on the
// game's X connection, like Mesa's WSI present thread. Saving and loading a state while it is
// inside such a request must not deadlock. Prints one "TEST:" line per frame and exits at frame
// END_FRAME.
#define _GNU_SOURCE
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <link.h>
#include <stdint.h>
#include <string.h>
#include <sys/uio.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#define END_FRAME 200

static xcb_connection_t *conn;
static Window win;

typedef unsigned int (*send_request_fn)(xcb_connection_t *, int, struct iovec *,
                                        const xcb_protocol_request_t *);

// libxcb built with -Bsymbolic-functions, as on Debian and Ubuntu, sends the requests of its
// generated functions without going through libTAS' xcb_send_request. libTAS hooks dlsym, so
// the function is looked up in libxcb's own symbol table.
static send_request_fn libxcb_send_request;

static int find_send_request(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    (void)data;
    if (!strstr(info->dlpi_name, "libxcb.so"))
        return 0;
    const ElfW(Dyn) *dyn = NULL;
    for (int i = 0; i < info->dlpi_phnum; i++)
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
            dyn = (const ElfW(Dyn) *)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
    const ElfW(Sym) *symtab = NULL;
    const char *strtab = NULL;
    const uint32_t *gnu_hash = NULL;
    for (; dyn && dyn->d_tag != DT_NULL; dyn++) {
        if (dyn->d_tag == DT_SYMTAB) symtab = (const ElfW(Sym) *)dyn->d_un.d_ptr;
        if (dyn->d_tag == DT_STRTAB) strtab = (const char *)dyn->d_un.d_ptr;
        if (dyn->d_tag == DT_GNU_HASH) gnu_hash = (const uint32_t *)dyn->d_un.d_ptr;
    }
    if (!symtab || !strtab || !gnu_hash)
        return 0;
    uint32_t nbuckets = gnu_hash[0], symoffset = gnu_hash[1], bloom_size = gnu_hash[2];
    const uint32_t *buckets = gnu_hash + 4 + bloom_size * (sizeof(ElfW(Addr)) / 4);
    const uint32_t *chain = buckets + nbuckets;
    for (uint32_t b = 0; b < nbuckets; b++) {
        for (uint32_t i = buckets[b]; i >= symoffset; i++) {
            if (symtab[i].st_shndx != SHN_UNDEF && !strcmp(strtab + symtab[i].st_name, "xcb_send_request")) {
                libxcb_send_request = (send_request_fn)(info->dlpi_addr + symtab[i].st_value);
                return 1;
            }
            if (chain[i - symoffset] & 1)
                break;
        }
    }
    return 0;
}

static xcb_get_geometry_cookie_t get_geometry(xcb_drawable_t drawable) {
    static const xcb_protocol_request_t req = {
        .count = 2, .ext = NULL, .opcode = XCB_GET_GEOMETRY, .isvoid = 0};
    xcb_get_geometry_request_t out = {.drawable = drawable};
    struct iovec parts[4];
    parts[2].iov_base = &out;
    parts[2].iov_len = sizeof(out);
    parts[3].iov_base = NULL;
    parts[3].iov_len = -parts[2].iov_len & 3;
    xcb_get_geometry_cookie_t cookie = {
        libxcb_send_request(conn, XCB_REQUEST_CHECKED, parts + 2, &req)};
    return cookie;
}

static void *query_geometry(void *arg) {
    (void)arg;
    for (;;)
        free(xcb_get_geometry_reply(conn, get_geometry(win), NULL));
    return NULL;
}

int main(void) {
    // Xlib only locks the display, and only hands the socket back under that lock, when threaded.
    XInitThreads();
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
    win = XCreateWindow(dpy, root, 0, 0, 320, 240, 0, vi->depth, InputOutput, vi->visual,
                        CWColormap, &swa);
    XMapWindow(dpy, win);
    GLXContext ctx = glXCreateContext(dpy, vi, NULL, True);
    if (!ctx || !glXMakeCurrent(dpy, win, ctx)) {
        fprintf(stderr, "TEST: FAIL no GL context\n");
        return 1;
    }

    conn = XGetXCBConnection(dpy);
    dl_iterate_phdr(find_send_request, NULL);
    if (!libxcb_send_request) {
        fprintf(stderr, "TEST: FAIL no libxcb xcb_send_request\n");
        return 1;
    }
    pthread_t thread;
    if (pthread_create(&thread, NULL, query_geometry, NULL) != 0) {
        fprintf(stderr, "TEST: FAIL pthread_create\n");
        return 1;
    }

    for (int frame = 1;; frame++) {
        fprintf(stderr, "TEST: frame %d\n", frame);
        if (frame == END_FRAME) break;
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(dpy, win);
    }
    return 0;
}
