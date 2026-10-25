# TODO: incremental savestates restore pages dropped by MADV_DONTNEED as zero

Not GPU related: reproduces with software rendering (llvmpipe, OpenGL) as well as GPU host (Vulkan).

## Symptom

Silksong 1.0.28891, `testing.ltm`: save at frame 1813 (during the scene load into `Tut_01`), play on, load the state.
The game then waits forever on Unity's preload queue (`Helper_PreloadManager_WaitForQueueToProcess`
logs "timeout waiting for operation ... to be processed by Loading.Preload thread"), while the
`Loading.Preload` thread spins at 100% CPU in `Remapper::GetOrGenerateInstanceID`.

Works with `savestate_settings` without `SS_INCREMENTAL` (24 instead of 25): all frames after the load
match the reference.

## Reproduce

```
GAME=<Silksong executable> MOVIE=<testing.ltm> REF=<reference run label> \
LIBTAS_SAVESTATE_TEST="s1:1813 l1:2500" LIBTAS_ARGS="-l $PWD/scripts/desync/savestate-roundtrip.lua" \
scripts/desync/desync.sh run ss-roundtrip . soft 3200
```

Stalls at frame 2500 within a minute. Add `-s savestate_settings=24` to `LIBTAS_ARGS` for the passing run.

## Root cause

1. Saving clears the soft-dirty bits (`/proc/self/clear_refs`).
2. Right after the save (same frame), Unity's allocator releases the old Remapper hash table:
   `madvise(MADV_DONTNEED)` on 1025 pages at offset 0x1000 of a 64 MB arena, called from `UnityPlayer.so`
   (caught with `scripts/desync/madvise.gdb.py`). The kernel drops the pages without setting soft-dirty;
   later reads map the zero page, still not soft-dirty.
3. Loading the incremental state: the pages are `BASE_PAGE` and not soft-dirty, so `readAnArea`
   (`Checkpoint.cpp`, the `flag == Area::BASE_PAGE` branch) assumes they are unchanged and skips them.
4. The table stays all zero. Its open-addressing probe only stops at a matching key or an empty slot
   (`0xffffffff`); zero is neither, so the lookup never terminates.

Evidence from the hung process: the 4 MB bucket array (131072 slots of 0x20 bytes) was all zero, pages
present but not soft-dirty; the base savestate held these pages as `COMPRESSED_PAGE`, the incremental one
as `BASE_PAGE`.

## Proposed fix

The load path assumes "not soft-dirty => unchanged". Without a write (which sets soft-dirty) the only way
a page changes is being dropped, after which it reads as zero. So before skipping a `BASE_PAGE` that is
not soft-dirty, restore it if it is now not present or zero while the base savestate has content for it
(base flag not `ZERO_PAGE`).

- Covers every source of dropped pages (Unity, glibc's internal `__madvise` in `shrink_heap`/`malloc_trim`,
  raw syscalls) without hooking anything.
- The save path already handles this: `writeAnArea` checks `isZeroPage` before the soft-dirty/`BASE_PAGE`
  logic, so dropped pages are saved as `ZERO_PAGE`.
- Cost: skipped pages are read once for the zero check (non-present ones are decided from pagemap).
- Edge case: `MADV_DONTNEED` on a private file mapping reverts the page to the file content instead of
  zero; detect via the pagemap file bit (61) like the save path does.

Rejected: hooking `madvise` and tracking dropped ranges. It misses glibc-internal and raw-syscall drops and
needs lock-free bookkeeping because the checkpoint runs with other threads suspended at arbitrary points.
