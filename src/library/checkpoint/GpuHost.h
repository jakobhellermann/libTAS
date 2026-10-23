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

#ifndef LIBTAS_GPUHOST_H_INCLUDED
#define LIBTAS_GPUHOST_H_INCLUDED

namespace libtas {

/* Asks kumquat, the process that renders the game's Vulkan calls, to snapshot
 * and restore its state along with the game's savestates. Inactive unless the
 * program started kumquat. */
namespace GpuHost {

void init();

/* Starts kumquat's snapshot of a savestate slot; it runs while the memory is saved. */
void startSnapshot(int slot);
/* Returns false, after logging why, when kumquat could not snapshot */
bool finishSnapshot();

/* Starts kumquat's restore of a savestate slot; it runs while the memory is loaded. */
void startRestore(int slot);
void finishRestore();

bool active();

/* Whether finishRestore() places a GPU host object at this fd of the loaded state */
bool restoresFd(int fd);

}
}

#endif
