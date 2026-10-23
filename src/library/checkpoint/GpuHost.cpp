/*
    Copyright 2015-2026 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "GpuHost.h"

#include "logging.h"
#include "GlobalState.h"
#include "ProcSelfMaps.h"
#include "ReservedMemory.h"
#include "MemArea.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <time.h>
#include <unistd.h>

namespace libtas {

/* Message types of kumquat's control connection, little-endian u32 first. */
static const uint32_t KUMQUAT_SNAPSHOT = 0x1;
static const uint32_t KUMQUAT_RESTORE = 0x2;
static const uint32_t KUMQUAT_LIST = 0x3;
static const uint32_t KUMQUAT_RESP_OK = 0x101;
static const uint32_t KUMQUAT_RESP_ERR = 0x102;
static const uint32_t KUMQUAT_RESOURCE = 0x103;
static const uint32_t KUMQUAT_REMAP = 0x104;
static const uint32_t KUMQUAT_END = 0x105;

static const int RESPONSE_TIMEOUT_MS = 10000;
static const size_t MAX_MESSAGE = 4 + 4096;

static int control = -1;
static char dir[1024];

/* Objects of kumquat the game holds, and where it holds them. Captured with
 * each savestate; living in saved memory, a load brings back the loaded state's. */
struct GpuObject { uint32_t resource; dev_t dev; ino_t ino; };
struct GpuMapping { void* addr; size_t size; int prot; uint32_t resource; };
struct GpuFd { int fd; bool cloexec; uint32_t resource; };
static GpuObject objects[256];
static GpuMapping mappings[1024];
static GpuFd fds[256];
static int object_count, mapping_count, fd_count;
static const int MAX_FD = 4096;

static int64_t nowNs()
{
    struct timespec ts;
    NATIVECALL(clock_gettime(CLOCK_MONOTONIC, &ts));
    return static_cast<int64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}

static int64_t* requestTime()
{
    return static_cast<int64_t*>(ReservedMemory::getAddr(ReservedMemory::GPU_TIMER_ADDR));
}

static double ms(int64_t ns)
{
    return ns / 1000000.0;
}

#define FAIL(...) do { LOG(LL_FATAL, LCF_CHECKPOINT, "GPU host: " __VA_ARGS__); abort(); } while (0)

/* Receives one message within the timeout, returns its type and payload length. */
static uint32_t receive(char* payload, size_t* len, int* fd)
{
    struct pollfd pfd = {control, POLLIN, 0};
    int ready;
    NATIVECALL(ready = poll(&pfd, 1, RESPONSE_TIMEOUT_MS));
    if (ready == 0)
        FAIL("kumquat did not answer in time");
    if (ready < 0)
        FAIL("poll: %s", strerror(errno));

    char message[MAX_MESSAGE];
    char cmsg[CMSG_SPACE(sizeof(int))];
    struct iovec iov = {message, sizeof(message)};
    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsg;
    msg.msg_controllen = sizeof(cmsg);
    ssize_t n;
    NATIVECALL(n = recvmsg(control, &msg, MSG_CMSG_CLOEXEC));
    if (n == 0)
        FAIL("kumquat exited");
    if (n < 4 || (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)))
        FAIL("malformed message (%zd bytes)", n);

    *fd = -1;
    struct cmsghdr* c = CMSG_FIRSTHDR(&msg);
    if (c && c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS)
        memcpy(fd, CMSG_DATA(c), sizeof(int));

    uint32_t type;
    memcpy(&type, message, 4);
    *len = n - 4;
    memcpy(payload, message + 4, *len);
    return type;
}

static void request(uint32_t type, int slot)
{
    char message[MAX_MESSAGE];
    memcpy(message, &type, 4);
    int len = snprintf(message + 4, MAX_MESSAGE - 4, "%s/state%d", dir, slot);
    *requestTime() = nowNs();
    ssize_t sent;
    NATIVECALL(sent = send(control, message, 4 + len, MSG_NOSIGNAL));
    if (sent != 4 + len)
        FAIL("send: %s", strerror(errno));
}

/* Returns false when kumquat reports that the operation failed, after logging why */
static bool expectOk(const char* operation)
{
    char payload[MAX_MESSAGE];
    size_t len;
    int fd;
    uint32_t type = receive(payload, &len, &fd);
    if (type == KUMQUAT_RESP_ERR) {
        LOG(LL_ERROR, LCF_CHECKPOINT, "GPU host: %s failed: %.*s", operation, static_cast<int>(len), payload);
        return false;
    }
    if (type != KUMQUAT_RESP_OK || fd != -1)
        FAIL("unexpected answer 0x%x to %s", type, operation);
    return true;
}

void GpuHost::init()
{
    const char* fd_str;
    const char* dir_str;
    NATIVECALL(fd_str = getenv("LIBTAS_GPU_CONTROL_FD"));
    NATIVECALL(dir_str = getenv("LIBTAS_GPU_DIR"));
    if (!fd_str || !dir_str)
        return;
    control = atoi(fd_str);
    snprintf(dir, sizeof(dir), "%s", dir_str);
    LOG(LL_INFO, LCF_CHECKPOINT, "GPU host control on fd %d, snapshots in %s", control, dir);
}

bool GpuHost::active()
{
    return control >= 0;
}

static const GpuObject* findObject(dev_t dev, ino_t ino)
{
    for (int i = 0; i < object_count; i++)
        if (objects[i].dev == dev && objects[i].ino == ino)
            return &objects[i];
    return nullptr;
}

/* Asks kumquat for its objects the game holds, then finds them in the game's mappings and fds. */
static void captureObjects()
{
    uint32_t type = KUMQUAT_LIST;
    ssize_t sent;
    NATIVECALL(sent = send(control, &type, 4, MSG_NOSIGNAL));
    if (sent != 4)
        FAIL("send: %s", strerror(errno));
    expectOk("list");

    object_count = 0;
    while (true) {
        char payload[MAX_MESSAGE];
        size_t len;
        int fd;
        type = receive(payload, &len, &fd);
        if (type == KUMQUAT_END)
            break;
        if (type != KUMQUAT_RESOURCE || len != 20 || fd != -1)
            FAIL("unexpected message 0x%x during list", type);
        if (object_count == sizeof(objects) / sizeof(objects[0]))
            FAIL("more than %d objects", object_count);
        uint64_t dev, ino;
        memcpy(&objects[object_count].resource, payload, 4);
        memcpy(&dev, payload + 4, 8);
        memcpy(&ino, payload + 12, 8);
        objects[object_count].dev = dev;
        objects[object_count].ino = ino;
        object_count++;
    }

    mapping_count = 0;
    ProcSelfMaps maps;
    Area area;
    while (maps.getNextArea(&area)) {
        const GpuObject* object = findObject(makedev(area.devmajor, area.devminor), area.inodenum);
        if (!object)
            continue;
        if (mapping_count == sizeof(mappings) / sizeof(mappings[0]))
            FAIL("more than %d mappings", mapping_count);
        mappings[mapping_count++] = {area.addr, area.size, area.prot, object->resource};
    }

    fd_count = 0;
    for (int fd = 0; fd < MAX_FD; fd++) {
        struct stat st;
        int ret;
        NATIVECALL(ret = fstat(fd, &st));
        if (ret != 0)
            continue;
        const GpuObject* object = findObject(st.st_dev, st.st_ino);
        if (!object)
            continue;
        if (fd_count == sizeof(fds) / sizeof(fds[0]))
            FAIL("more than %d fds", fd_count);
        int flags;
        NATIVECALL(flags = fcntl(fd, F_GETFD));
        fds[fd_count++] = {fd, (flags & FD_CLOEXEC) != 0, object->resource};
    }
    LOG(LL_INFO, LCF_CHECKPOINT, "GPU host: %d objects, %d mappings, %d fds", object_count, mapping_count, fd_count);
}

void GpuHost::startSnapshot(int slot)
{
    if (!active())
        return;
    captureObjects();
    request(KUMQUAT_SNAPSHOT, slot);
}

bool GpuHost::finishSnapshot()
{
    if (!active())
        return true;
    int64_t waited = nowNs();
    if (!expectOk("snapshot"))
        return false;
    int64_t done = nowNs();
    LOG(LL_INFO, LCF_CHECKPOINT, "GPU host snapshot answered %.1f ms after it was requested, the game waited %.1f ms for it after saving its memory",
        ms(done - *requestTime()), ms(done - waited));
    return true;
}

void GpuHost::startRestore(int slot)
{
    if (active())
        request(KUMQUAT_RESTORE, slot);
}

/* Puts kumquat's new object where the game held the replaced one: every mapping
 * at its address, every fd at its number. */
static void replace(uint32_t resource, int memory)
{
    bool known = false;
    for (int i = 0; i < object_count; i++)
        known |= objects[i].resource == resource;
    if (!known)
        FAIL("kumquat replaced resource %u, which the game did not hold", resource);

    for (int i = 0; i < mapping_count; i++) {
        if (mappings[i].resource != resource)
            continue;
        /* kumquat's clients map whole objects from offset 0. */
        void* addr;
        NATIVECALL(addr = mmap(mappings[i].addr, mappings[i].size, mappings[i].prot,
                               MAP_SHARED | MAP_FIXED, memory, 0));
        if (addr == MAP_FAILED)
            FAIL("mmap resource %u at %p: %s", resource, mappings[i].addr, strerror(errno));
    }

    bool placed = false;
    for (int i = 0; i < fd_count; i++) {
        if (fds[i].resource != resource)
            continue;
        int ret;
        if (fds[i].fd == memory) {
            placed = true;
            NATIVECALL(ret = fcntl(memory, F_SETFD, fds[i].cloexec ? FD_CLOEXEC : 0));
        }
        else {
            NATIVECALL(ret = dup3(memory, fds[i].fd, fds[i].cloexec ? O_CLOEXEC : 0));
        }
        if (ret < 0)
            FAIL("place resource %u at fd %d: %s", resource, fds[i].fd, strerror(errno));
    }
    if (!placed)
        NATIVECALL(close(memory));
}

bool GpuHost::restoresFd(int fd)
{
    if (!active())
        return false;
    for (int i = 0; i < fd_count; i++)
        if (fds[i].fd == fd)
            return true;
    return false;
}

void GpuHost::finishRestore()
{
    if (!active())
        return;
    int64_t waited = nowNs();
    if (!expectOk("restore"))
        FAIL("the game's memory is loaded, but kumquat's state is not");
    int64_t restored = nowNs();
    int remaps = 0;
    while (true) {
        char payload[MAX_MESSAGE];
        size_t len;
        int fd;
        uint32_t type = receive(payload, &len, &fd);
        if (type == KUMQUAT_END)
            break;
        if (type != KUMQUAT_REMAP || len != 4 || fd < 0)
            FAIL("unexpected message 0x%x during restore", type);
        uint32_t resource;
        memcpy(&resource, payload, 4);
        replace(resource, fd);
        remaps++;
    }
    if (remaps != object_count)
        FAIL("kumquat replaced %d of the %d objects the game holds", remaps, object_count);
    LOG(LL_INFO, LCF_CHECKPOINT, "GPU host restore answered %.1f ms after it was requested, the game waited %.1f ms for it after loading its memory, replacing %d objects took %.1f ms",
        ms(restored - *requestTime()), ms(restored - waited), remaps, ms(nowNs() - restored));
}

}
