# Logs madvise calls that drop page contents, with the libTAS frame and the callers.
import gdb

DROPPING = {4: "MADV_DONTNEED", 8: "MADV_FREE", 9: "MADV_REMOVE"}


class MadviseBreakpoint(gdb.Breakpoint):
    def stop(self):
        advice = int(gdb.parse_and_eval("$rdx"))
        if advice not in DROPPING:
            return False
        addr = int(gdb.parse_and_eval("$rdi")) & 0xffffffffffffffff
        length = int(gdb.parse_and_eval("$rsi"))
        frame = int(gdb.parse_and_eval("libtas::framecount"))
        callers = []
        f = gdb.newest_frame().older()
        while f is not None and len(callers) < 4:
            lib = (gdb.solib_name(f.pc()) or "exe").rsplit("/", 1)[-1]
            callers.append(f"{f.name() or hex(f.pc())}@{lib}")
            f = f.older()
        print(f"f={frame} {DROPPING[advice]} {addr:#x}-{addr + length:#x} ({length >> 12} pages) by {' < '.join(callers)}", flush=True)
        return False


MadviseBreakpoint("madvise", internal=True)
