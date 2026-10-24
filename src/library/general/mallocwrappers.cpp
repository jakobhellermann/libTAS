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


#include "mallocwrappers.h"

#include "GlobalState.h"
#include "checkpoint/SaveStateManager.h"

#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>

namespace libtas {

/* The allocator may hold locks, such as a glibc arena lock, that the thread
 * doing a savestate needs while the other threads are suspended. A thread is
 * therefore never suspended inside these functions: the suspend signal only
 * marks it, and it suspends itself when it leaves the allocator. The signal
 * handler reads these, hence initial-exec TLS. */
static thread_local volatile int alloc_depth __attribute__((tls_model("initial-exec"))) = 0;
static thread_local volatile bool suspend_deferred __attribute__((tls_model("initial-exec"))) = false;
static thread_local bool resolving __attribute__((tls_model("initial-exec"))) = false;

/* dlsym allocates while the allocator functions themselves are resolved */
alignas(max_align_t) static char bootstrap_heap[4096];
static size_t bootstrap_used = 0;

static void *bootstrapAlloc(size_t size)
{
    size = (size + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1);
    if (size > sizeof(bootstrap_heap) - bootstrap_used)
        abort();
    void *ptr = bootstrap_heap + bootstrap_used;
    bootstrap_used += size;
    return ptr;
}

static bool isBootstrap(void *ptr)
{
    return ptr >= bootstrap_heap && ptr < bootstrap_heap + sizeof(bootstrap_heap);
}

/* The next definition, so that a game's own allocator such as tcmalloc keeps working */
template <typename F>
static F next(F &slot, const char *name)
{
    if (!slot) {
        resolving = true;
        void *sym;
        NATIVECALL(sym = dlsym(RTLD_NEXT, name));
        resolving = false;
        if (!sym)
            abort();
        slot = reinterpret_cast<F>(sym);
    }
    return slot;
}

namespace {
struct AllocatorScope {
    /* Some custom implementation of malloc can call a function that we hook,
     * and our function ultimately calls malloc, which creates a deadlock.
     * Example: Antichamber (Unreal) uses tcmalloc, and has the following softlock:
     * - malloc() -> libtas::getpid() -> backtrace_symbols() -> malloc()
     * Marking the call as native can mitigate the deadlock. */
    GlobalNative gn;

    AllocatorScope() { alloc_depth = alloc_depth + 1; }
    ~AllocatorScope()
    {
        alloc_depth = alloc_depth - 1;
        if (alloc_depth == 0 && suspend_deferred) {
            suspend_deferred = false;
            raise(SaveStateManager::sigSuspend());
        }
    }
};
}

bool deferSuspendInAllocator()
{
    if (alloc_depth == 0)
        return false;
    suspend_deferred = true;
    return true;
}

static void *(*next_calloc)(size_t, size_t);
static void *(*next_realloc)(void *, size_t);
static void *(*next_reallocarray)(void *, size_t, size_t);
static void (*next_free)(void *);
static void *(*next_memalign)(size_t, size_t);
static int (*next_posix_memalign)(void **, size_t, size_t);
static void *(*next_aligned_alloc)(size_t, size_t);
static void *(*next_valloc)(size_t);
static void *(*next_pvalloc)(size_t);

void *malloc (size_t size) __THROW
{
    if (resolving)
        return bootstrapAlloc(size);
    AllocatorScope scope;
    /* Zeroed, so that the game does not depend on leftover memory contents */
    return next(next_calloc, "calloc")(1, size);
}

void *calloc (size_t nmemb, size_t size) __THROW
{
    if (resolving) {
        if (nmemb && size > SIZE_MAX / nmemb)
            abort();
        return bootstrapAlloc(nmemb * size);
    }
    AllocatorScope scope;
    return next(next_calloc, "calloc")(nmemb, size);
}

void *realloc (void *ptr, size_t size) __THROW
{
    if (isBootstrap(ptr))
        abort();
    AllocatorScope scope;
    return next(next_realloc, "realloc")(ptr, size);
}

void *reallocarray (void *ptr, size_t nmemb, size_t size) __THROW
{
    if (isBootstrap(ptr))
        abort();
    AllocatorScope scope;
    return next(next_reallocarray, "reallocarray")(ptr, nmemb, size);
}

void free (void *ptr) __THROW
{
    if (isBootstrap(ptr))
        return;
    AllocatorScope scope;
    next(next_free, "free")(ptr);
}

void *memalign (size_t alignment, size_t size) __THROW
{
    AllocatorScope scope;
    return next(next_memalign, "memalign")(alignment, size);
}

int posix_memalign (void **memptr, size_t alignment, size_t size) __THROW
{
    AllocatorScope scope;
    return next(next_posix_memalign, "posix_memalign")(memptr, alignment, size);
}

void *aligned_alloc (size_t alignment, size_t size) __THROW
{
    AllocatorScope scope;
    return next(next_aligned_alloc, "aligned_alloc")(alignment, size);
}

void *valloc (size_t size) __THROW
{
    AllocatorScope scope;
    return next(next_valloc, "valloc")(size);
}

void *pvalloc (size_t size) __THROW
{
    AllocatorScope scope;
    return next(next_pvalloc, "pvalloc")(size);
}

}
