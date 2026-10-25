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

#ifndef LIBTAS_MALLOC_H_INCL
#define LIBTAS_MALLOC_H_INCL

#include "hook.h"

#include <cstddef>

namespace libtas {

OVERRIDE void *malloc (size_t size) __THROW;
OVERRIDE void *calloc (size_t nmemb, size_t size) __THROW;
OVERRIDE void *realloc (void *ptr, size_t size) __THROW;
OVERRIDE void *reallocarray (void *ptr, size_t nmemb, size_t size) __THROW;
OVERRIDE void free (void *ptr) __THROW;
OVERRIDE void *memalign (size_t alignment, size_t size) __THROW;
OVERRIDE int posix_memalign (void **memptr, size_t alignment, size_t size) __THROW;
OVERRIDE void *aligned_alloc (size_t alignment, size_t size) __THROW;
OVERRIDE void *valloc (size_t size) __THROW;
OVERRIDE void *pvalloc (size_t size) __THROW;

/* Called by the suspend signal handler: returns true, and suspends the thread
 * once it leaves the allocator, if the signal interrupted an allocator call. */
bool deferSuspendInAllocator();

}

#endif
