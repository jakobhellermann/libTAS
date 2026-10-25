#!/usr/bin/env bash
# Runs every rendering mode of two builds up to a frame and diffs each against the base build in native mode.
#   BASE_REPO=<repo> GPU_REPO=<repo> GAME=... MOVIE=... experiments.sh [stop frame]
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
: "${BASE_REPO:?}" "${GPU_REPO:?}"
stop=${1:-1900}
d=$here/desync.sh

run() { "$d" run "$@" "$stop" || echo "FAILED $1"; }
run base-native "$BASE_REPO" native
run base-soft "$BASE_REPO" soft
run gpu-native "$GPU_REPO" native
run gpu-soft "$GPU_REPO" soft
GAME_ARGS="-force-gfx-direct -force-vulkan" run gpu-native-vk "$GPU_REPO" native
run gpuhost-1 "$GPU_REPO" gpuhost
run gpuhost-2 "$GPU_REPO" gpuhost

for r in base-soft gpu-native gpu-soft gpu-native-vk gpuhost-1 gpuhost-2; do
    echo "===== base-native vs $r"
    "$d" diff base-native $r 1
done
echo "===== gpuhost-1 vs gpuhost-2"
"$d" diff gpuhost-1 gpuhost-2 1
