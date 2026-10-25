#!/usr/bin/env bash
# Desync check: trace per-frame game state through a lua script and compare runs.
#   desync.sh run <label> <libtas repo or extracted AppImage> <soft|native|gpuhost> <stop frame>
#   desync.sh diff <label A> <label B> [context] [last frame]
# Environment:
#   GAME        game executable (required for run)
#   MOVIE       movie to play (required for run unless MOVIE_MODE=write)
#   MOVIE_MODE  "read" (default) or "write", which records a new movie in the run directory;
#               movies carry sync settings like time tracking, so -s overrides only win when writing
#   GAME_ARGS   replaces the configured game arguments, e.g. "-force-gfx-direct -force-vulkan"
#   LIBTAS_ARGS extra libTAS options, e.g. "-s logging_level=4"
#   TRACE_LUA   lua script writing one line per frame to $LIBTAS_TRACE (default: silksong-trace.lua)
#   REF         label of a reference run; the run stops MARGIN (default 30) frames after diverging from it
#   GDB_DEBUG_DIR extra gdb debug-file-directory (build-id layout) for stall backtraces
#   DESYNC_RUNS output directory (default ${TMPDIR:-/tmp}/libtas-desync)
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
runs=${DESYNC_RUNS:-${TMPDIR:-/tmp}/libtas-desync}
trace_lua=${TRACE_LUA:-$here/silksong-trace.lua}
cfg_dir=${XDG_CONFIG_HOME:-$HOME/.config}/libTAS
stall_secs=5
margin=${MARGIN:-30}

die() { echo "desync: $*" >&2; exit 1; }

mode_args() {
    local repo=$1 mode=$2
    # AppImages are built from this branch, which has the rendering setting
    if [ -x "$repo/AppRun" ] || grep -q 'key == "rendering"' "$repo/src/program/Config.cpp"; then
        case $mode in
            native) echo "-s rendering=0" ;;
            soft) echo "-s rendering=1" ;;
            gpuhost) echo "-s rendering=2" ;;
            *) die "unknown mode $mode" ;;
        esac
    else
        # this config parses every -s value as an integer
        case $mode in
            native) echo "-s opengl_soft=0" ;;
            soft) echo "-s opengl_soft=1" ;;
            gpuhost) die "$repo has no GPU host rendering" ;;
            *) die "unknown mode $mode" ;;
        esac
    fi
}

# Files the game writes only exist in memory while libTAS prevents writing to disk.
save_memfd() {
    local pid=$1 suffix=$2 out=$3 fd
    for fd in /proc/"$pid"/fd/*; do
        if [[ $(readlink "$fd") == *"$suffix (deleted)" ]]; then
            cp "$fd" "$out"
            return 0
        fi
    done
    echo "desync: no in-memory $suffix found for pid $pid" >&2
}

# Globals set by cmd_run, so they still exist when the EXIT trap runs.
restore_config() {
    cp -a "$cfg_used" "$run_out/config-used.ini" 2>/dev/null || true
    cp -a "$cfg_backup/." "$cfg_dir/"
    rm -f "${cfg_created[@]}"
    local f
    for f in "$cfg_backup"/*; do
        cmp -s "$f" "$cfg_dir/$(basename "$f")" || echo "desync: FAILED to restore $cfg_dir/$(basename "$f")" >&2
    done
}

cmd_run() {
    [ $# -eq 4 ] || die "usage: run <label> <repo> <soft|native|gpuhost> <stop frame>"
    : "${GAME:?GAME must point to the game executable}"
    local movie_args
    case ${MOVIE_MODE:-read} in
        read) : "${MOVIE:?MOVIE must point to the movie file}"; movie_args=(--read "$MOVIE") ;;
        write) movie_args=(--write "$runs/$1/movie.ltm") ;;
        *) die "unknown MOVIE_MODE $MOVIE_MODE" ;;
    esac
    local label=$1 repo; repo=$(cd "$2" && pwd); local mode=$3 stop=$4
    local out=$runs/$label
    local margs; margs=$(mode_args "$repo" "$mode")
    local game_name; game_name=$(basename "$GAME")
    local game_comm=${game_name:0:15}

    pgrep -x libTAS >/dev/null && die "another libTAS is running"
    pgrep -x "$game_comm" >/dev/null && die "the game is already running"

    rm -rf "$out"; mkdir -p "$out"
    # appending: loading a state restores the game's file offsets, which would overwrite the log
    : > "$out/libtas.log"
    # libTAS saves its config on exit, which would persist the overrides
    cfg_backup=$out/config-backup
    mkdir -p "$cfg_backup"
    cfg_created=()
    local f
    for f in "$cfg_dir/libTAS.ini" "$cfg_dir/$game_name.ini"; do
        if [ -e "$f" ]; then cp -a "$f" "$cfg_backup/"; else cfg_created+=("$f"); fi
    done
    cfg_used=$cfg_dir/$game_name.ini
    run_out=$out
    trap restore_config EXIT

    {
        echo "repo=$repo"
        echo "revision=$(cd "$repo" && jj log -r @ --no-graph -T 'commit_id.short()' 2>/dev/null || echo "not a repo")"
        echo "mode=$mode ($margs)"
        echo "game=$GAME"
        echo "movie=${movie_args[*]}"
        echo "stop=$stop"
        echo "game_args=${GAME_ARGS-<config>}"
        echo "libtas_args=${LIBTAS_ARGS-}"
    } > "$out/meta"

    # shellcheck disable=SC2086
    local libtas=("$repo/build/src/program/libTAS" --libtas-so-path "$repo/build/src/library/libtas.so")
    [ -x "$repo/AppRun" ] && libtas=("$repo/AppRun")
    LIBTAS_TRACE=$out/trace.txt "${libtas[@]}" -n $margs ${LIBTAS_ARGS-} \
        -l "$trace_lua" "${movie_args[@]}" "$GAME" \
        ${GAME_ARGS+$GAME_ARGS} >> "$out/libtas.log" 2>&1 &
    local pid=$!

    local ref_trace=""
    if [ -n "${REF:-}" ]; then
        ref_trace=$runs/$REF/trace.txt
        [ -f "$ref_trace" ] || die "no reference trace $ref_trace"
        echo "ref=$REF" >> "$out/meta"
    fi

    local frames=0 game="" last_growth=$SECONDS failure="" diverged="" last_check=0 started=$SECONDS
    while true; do
        [ -z "$game" ] && game=$(pgrep -P $pid -x "$game_comm" || true)
        local now; now=$( [ -f "$out/trace.txt" ] && wc -l < "$out/trace.txt" || echo 0)
        if [ "$now" -gt "$frames" ]; then
            [ "$frames" -eq 0 ] && echo "first_frame_after=$((SECONDS - started))s" >> "$out/meta"
            frames=$now; last_growth=$SECONDS
        fi

        if [ "$frames" -ge "$stop" ]; then break; fi
        if [ -n "$ref_trace" ] && [ -z "$diverged" ] && [ $((SECONDS - last_check)) -ge 1 ]; then
            last_check=$SECONDS
            diverged=$(python3 -I "$here/tracediff.py" --first-diff "$ref_trace" "$out/trace.txt")
        fi
        if [ -n "$diverged" ] && [ "$frames" -ge $((diverged + margin)) ]; then break; fi
        if ! kill -0 $pid 2>/dev/null; then failure="libTAS exited"; break; fi
        local game_state=""
    [ -n "$game" ] && game_state=$(awk '{print $3}' /proc/"$game"/stat 2>/dev/null || echo gone)
    if [ "$game_state" = Z ] || [ "$game_state" = gone ]; then failure="the game exited"; break; fi
        if grep -q "The connection to the game was" "$out/libtas.log"; then
            failure="the connection to the game was lost"; break
        fi
        # a stopped game (SIGSTOP, debugger) is not stalling
        if [ -n "$game" ] && [[ $(awk '{print $3}' /proc/"$game"/stat 2>/dev/null) == [Tt] ]]; then
            last_growth=$SECONDS
        fi
        if [ $((SECONDS - last_growth)) -ge "$stall_secs" ]; then
            failure="no new frame for ${stall_secs}s"; break
        fi
        sleep 0.1
    done

    if [ -n "$game" ] && kill -0 "$game" 2>/dev/null; then
        save_memfd "$game" "/Player.log" "$out/Player.log"
        if [[ $failure == "no new frame"* ]]; then
            timeout 120 gdb -p "$game" -batch -iex "set debuginfod enabled off" \
                ${GDB_DEBUG_DIR:+-iex "set debug-file-directory $GDB_DEBUG_DIR:/usr/lib/debug"} \
                -ex "thread apply all bt 30" > "$out/stall-backtraces.txt" 2>&1 || true
            echo "desync: main thread of the stalled game (all threads in $out/stall-backtraces.txt):" >&2
            awk '/^Thread 1 \(/{p=1; next} /^Thread [0-9]+ \(/{p=0} p && /^#/' "$out/stall-backtraces.txt" \
                | head -15 | cut -c1-160 >&2
        fi
    fi
    # without its game, libTAS exits on its own after reporting the crash
    if [[ $failure == "the game exited" || $failure == "the connection to the game was lost" ]]; then
        local deadline=$((SECONDS + stall_secs))
        while kill -0 $pid 2>/dev/null && [ $SECONDS -lt $deadline ]; do sleep 0.1; done
    fi
    kill $pid 2>/dev/null || true
    wait $pid 2>/dev/null || true
    # libTAS installs signal handlers in the game, so only SIGKILL is reliable
    if [ -n "$game" ]; then
        kill -9 "$game" 2>/dev/null || true
        while kill -0 "$game" 2>/dev/null; do sleep 0.2; done
    fi
    if [ -f "$out/Player.log" ]; then
        grep -E "^(Renderer:|Forcing GfxDevice|\[Vulkan init\] Physical Device)" "$out/Player.log" >> "$out/meta" || true
    fi

    # libTAS has written the settings it ran with by now
    grep -E "^(game_specific_sync|game_specific_timing|async_events|wait_timeout|rendering|opengl_soft|framerate_num|gameargs)=" \
        "$cfg_used" | sed 's/^/config: /' >> "$out/meta" || echo "config: $cfg_used missing" >> "$out/meta"

    if [ -n "$failure" ]; then
        echo "result=FAILED at frame $frames: $failure" >> "$out/meta"
        grep -E "ERROR|terminate|what\(\)|Exiting|^Game |^GPU host:|^Crash report" "$out/libtas.log" | tail -5 >&2 || true
        die "$label: $failure at frame $frames, see $out/libtas.log"
    fi
    if [ -n "$diverged" ]; then
        echo "result=diverged from $REF at frame $diverged, stopped at $frames" >> "$out/meta"
        echo "$label: diverged from $REF at frame $diverged, stopped at frame $frames"
    else
        echo "result=stopped at frame $frames" >> "$out/meta"
        echo "$label: stopped at frame $frames${ref_trace:+, identical to $REF}"
    fi
    grep -E "^(Renderer:|Forcing GfxDevice)" "$out/meta" || true
}

main() {
    case ${1:-} in
        run) shift; cmd_run "$@" ;;
        diff) shift; python3 -I "$here/tracediff.py" "$runs/$1/trace.txt" "$runs/$2/trace.txt" "${3:-3}" ${4:+"$4"} ;;
        *) die "usage: run <label> <repo> <soft|native|gpuhost> <stop frame> | diff <A> <B> [context] [last frame]" ;;
    esac
}

# parsed in one go, so editing this file during a long run cannot break it
main "$@"; exit
