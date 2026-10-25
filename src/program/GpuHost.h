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

#include <filesystem>
#include <string>

class Context;

/* kumquat: a separate process that renders the game's Vulkan calls (through
 * the gfxstream guest driver) on the real GPU, and can snapshot its state. */
namespace GpuHost {

/* Enabled by the GPU host rendering mode. LIBTAS_KUMQUAT_BIN and LIBTAS_GFXSTREAM_ICD
 * override the configured paths. */
bool enabled(Context* context);

/* kumquat's socket, log and snapshots, one libTAS instance at a time like its own socket */
std::filesystem::path directory();

/* Removes the snapshots of savestates, see remove_savestates() */
void removeSnapshots();

/* Starts kumquat and blocks until it serves. Returns the error, empty on success. */
std::string start(Context* context);

/* Points the game's Vulkan and GL at kumquat. Runs in the forked game process. */
void setGameEnv(Context* context);

void stop(Context* context);

/* How kumquat is doing and where its log is, empty without a running kumquat */
std::string describe(Context* context);

}

#endif
