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
#include "Context.h"
#include "utils.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

/* Message types of kumquat's control connection, little-endian u32 first. */
static const uint32_t KUMQUAT_READY = 0x100;

static const int READY_TIMEOUT_MS = 10000;
/* A healthy kumquat is gone 3-5 ms after SIGKILL */
static const int EXIT_TIMEOUT_MS = 50;

static std::string withLog(Context* context, const std::string& what)
{
    return "GPU host: " + what + " (see " + (context->kumquat_dir / "kumquat.log").string() + ")";
}

static std::string kumquatPath(Context* context)
{
    const char* env = getenv("LIBTAS_KUMQUAT_BIN");
    return env ? env : context->config.kumquat_path.string();
}

static std::string gfxstreamIcd(Context* context)
{
    const char* env = getenv("LIBTAS_GFXSTREAM_ICD");
    return env ? env : context->config.gfxstream_icd.string();
}

std::filesystem::path GpuHost::directory()
{
    return std::filesystem::temp_directory_path() / "libtas-gpu";
}

void GpuHost::removeSnapshots()
{
    /* Same slots as remove_savestates(), kumquat writes a directory per slot */
    for (int i = 0; i <= 10; i++)
        std::filesystem::remove_all(directory() / ("state" + std::to_string(i)));
}

bool GpuHost::enabled(Context* context)
{
    return context->config.sc.rendering == SharedConfig::RENDERING_GPU_HOST;
}

std::string GpuHost::start(Context* context)
{
    std::string kumquat = kumquatPath(context);
    if (kumquat.empty() || gfxstreamIcd(context).empty())
        return "GPU host: kumquat_path and gfxstream_icd (or LIBTAS_KUMQUAT_BIN and LIBTAS_GFXSTREAM_ICD) are required";

    std::string features_arg = "--renderer-features=VulkanSnapshots:enabled";
    int udmabuf = open("/dev/udmabuf", O_RDONLY | O_CLOEXEC);
    if (udmabuf >= 0) {
        close(udmabuf);
        features_arg += ",VulkanAllocateHostVisibleAsUdmabuf:enabled";
    }
    else {
        std::cerr << "GPU host: cannot open /dev/udmabuf (" << strerror(errno)
                  << "), sharing exported device memory instead; savestates can be slow. "
                  << std::endl;
    }

    context->kumquat_dir = directory();
    std::filesystem::create_directories(context->kumquat_dir);
    std::filesystem::remove(context->kumquat_dir / "gpu.sock");
    std::filesystem::path socket = context->kumquat_dir / "gpu.sock";
    std::filesystem::path log = context->kumquat_dir / "kumquat.log";

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sv) != 0)
        return withLog(context, std::string("socketpair: ") + strerror(errno));

    std::string socket_arg = "--gpu-socket-path=" + socket.string();
    std::string control_arg = "--control-fd=" + std::to_string(sv[1]);
    pid_t parent = getpid();

    pid_t pid = fork();
    if (pid == 0) {
        /* No kumquat outlives libTAS. */
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (getppid() != parent)
            _exit(1);
        int logfd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd < 0)
            _exit(1);
        dup2(logfd, 1);
        dup2(logfd, 2);
        fcntl(sv[1], F_SETFD, 0);
        /* kumquat renders with the host's real driver. */
        unsetenv("VK_DRIVER_FILES");
        unsetenv("VK_ICD_FILENAMES");
        unsetenv("VK_LOADER_DRIVERS_SELECT");
        unsetenv("LD_PRELOAD");
        setenv("GFXSTREAM_LOG_LEVEL", "info", 0);
        execlp(kumquat.c_str(), "kumquat",
              "--capset-names=gfxstream-vulkan", socket_arg.c_str(), control_arg.c_str(),
              features_arg.c_str(), static_cast<char*>(nullptr));
        dprintf(2, "exec %s: %s\n", kumquat.c_str(), strerror(errno));
        _exit(127);
    }
    close(sv[1]);
    if (pid < 0) {
        close(sv[0]);
        return withLog(context, std::string("fork: ") + strerror(errno));
    }
    context->kumquat_pid = pid;
    context->kumquat_control = sv[0];

    std::string error;
    struct pollfd pfd = {sv[0], POLLIN, 0};
    int ready = poll(&pfd, 1, READY_TIMEOUT_MS);
    uint32_t message = 0;
    if (ready == 0)
        error = "kumquat did not report ready";
    else if (ready < 0)
        error = std::string("poll: ") + strerror(errno);
    else if (recv(sv[0], &message, sizeof(message), 0) != sizeof(message) || message != KUMQUAT_READY)
        error = "kumquat exited before it was ready";
    if (error.empty())
        return error;
    stop(context);
    return withLog(context, error);
}

void GpuHost::setGameEnv(Context* context)
{
    setenv("VIRTGPU_KUMQUAT", "1", 1);
    setenv("VIRTGPU_KUMQUAT_SOCKET_PATH", (context->kumquat_dir / "gpu.sock").c_str(), 1);
    setenv("VK_DRIVER_FILES", gfxstreamIcd(context).c_str(), 1);

    /* Present by copying CPU images to the window, see the software rendering mode. */
    // TODO alternative queue one OUT_OF_DATE swapchain to invalidate xshmfence mappings
    appendEnvList("MESA_VK_WSI_DEBUG", "sw");
    appendEnvList("MESA_VK_WSI_DEBUG", "noshm");

    /* GL renders through zink on top of that Vulkan driver. */
    setenv("__GLX_VENDOR_LIBRARY_NAME", "mesa", 1);
    setenv("MESA_LOADER_DRIVER_OVERRIDE", "zink", 1);
    unsetenv("LIBGL_ALWAYS_SOFTWARE");

    /* libtas.so asks kumquat for snapshots and restores, so the game inherits the control connection. */
    fcntl(context->kumquat_control, F_SETFD, 0);
    setenv("LIBTAS_GPU_CONTROL_FD", std::to_string(context->kumquat_control).c_str(), 1);
    setenv("LIBTAS_GPU_DIR", context->kumquat_dir.c_str(), 1);
    unsetenv("VK_LOADER_DRIVERS_SELECT");
}

void GpuHost::stop(Context* context)
{
    if (!context->kumquat_pid)
        return;
    int pidfd = static_cast<int>(syscall(SYS_pidfd_open, context->kumquat_pid, 0));
    if (pidfd < 0 || kill(context->kumquat_pid, SIGKILL) != 0) {
        std::cerr << withLog(context, std::string("killing kumquat ") + std::to_string(context->kumquat_pid) + ": " + strerror(errno)) << std::endl;
    }
    else {
        struct pollfd pfd = {pidfd, POLLIN, 0};
        if (poll(&pfd, 1, EXIT_TIMEOUT_MS) == 1)
            waitpid(context->kumquat_pid, nullptr, 0);
        else
            std::cerr << withLog(context, "kumquat " + std::to_string(context->kumquat_pid) + " did not exit within " + std::to_string(EXIT_TIMEOUT_MS) + " ms after SIGKILL, leaving it behind") << std::endl;
    }
    if (pidfd >= 0)
        close(pidfd);
    close(context->kumquat_control);
    context->kumquat_control = -1;
    context->kumquat_pid = 0;
}
