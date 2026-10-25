# Logs Unity's time updates and presents with the libTAS frame they happen in.
# The breakpoints are picked by the build id of the loaded UnityPlayer.so.
import gdb

BEFORE_UPDATE = "_ZZ23InitPlayerLoopCallbacksvEN41EarlyUpdatePresentBeforeUpdateRegistrator7ForwardEv"
AFTER_DRAW_6000 = "_ZZ23InitPlayerLoopCallbacksvEN41PostLateUpdatePresentAfterDrawRegistrator7ForwardEv"
TIME_UPDATE = "TimeManager::Update(double)"

# build id -> {breakpoint location: label}; int locations are offsets into a stripped
# UnityPlayer.so, located through its exported PlayerMain at "player_main"
BUILDS = {
    # Hollow Knight 1.5.78, Unity 2020.2.2f1, UnityPlayer_s.debug next to it
    "827916278b32650a": {
        TIME_UPDATE: "TimeManager::Update",
        BEFORE_UPDATE: "EarlyUpdate.PresentBeforeUpdate",
        "PresentAfterDraw(GfxDevicePresentMode)": "PostLateUpdate.PresentAfterDraw",
    },
    # Hollow Knight, Unity 6000.0.61f1, UnityPlayer_s.debug next to it
    "687ec82915ce757a0bf9ebcd6068f093707479e4": {
        TIME_UPDATE: "TimeManager::Update",
        BEFORE_UPDATE: "EarlyUpdate.PresentBeforeUpdate",
        AFTER_DRAW_6000: "PostLateUpdate.PresentAfterDraw",
    },
    # Silksong 1.0.28891, Unity 6000.0.50f1 custom build without symbols, offsets from sigfind.py
    "71778514cbf704690b6c8e1652656ba2b22f3f30": {
        "player_main": 0x158ac10,
        0x1273e50: "TimeManager::Update",
        0x12ec7f0: "EarlyUpdate.PresentBeforeUpdate",
        0x12f7780: "PostLateUpdate.PresentAfterDraw",
    },
}


class LogBreakpoint(gdb.Breakpoint):
    def __init__(self, spec, label):
        super().__init__(spec, internal=True)
        self.label = label

    def stop(self):
        extra = ""
        if self.label == "TimeManager::Update":
            extra = f"({float(gdb.parse_and_eval('$xmm0.v2_double[0]')):.6f})"
        print(f"f={int(gdb.parse_and_eval('libtas::framecount'))} {self.label}{extra}", flush=True)
        return False


def setup(objfile):
    if not objfile.filename.endswith("/UnityPlayer.so"):
        return
    build = dict(BUILDS.get(objfile.build_id) or {})
    if not build:
        raise gdb.GdbError(f"no breakpoints known for UnityPlayer.so build id {objfile.build_id}")
    player_main = build.pop("player_main", None)
    if player_main is not None:
        base = int(gdb.parse_and_eval("(long)&'_Z10PlayerMainiPPc'")) - player_main
    for location, label in build.items():
        LogBreakpoint(f"*{base + location:#x}" if isinstance(location, int) else location, label)
    print(f"UnityPlayer.so {objfile.build_id}: breakpoints set", flush=True)


for loaded in gdb.objfiles():
    setup(loaded)
gdb.events.new_objfile.connect(lambda event: setup(event.new_objfile))
LogBreakpoint("libtas::frameBoundary", "    libTAS frame boundary")
