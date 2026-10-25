import re
import sys

NO_INFO = "<no info>"


DRAW_FLAGS = ("draw", "nodraw")


def load(path):
    """Maps frame to "draw-flag<TAB>info"; traces without the draw column get None as flag."""
    frames = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            frame, _, rest = line.rstrip("\n").partition("\t")
            flag, _, info = rest.partition("\t")
            if flag not in DRAW_FLAGS:
                flag, info = None, rest
            frames[int(frame)] = (flag, info)
    return frames


def flatten(frames, with_flag):
    return {fr: f"{flag}\t{info}" if with_flag else info for fr, (flag, info) in frames.items()}


def first_info(frames):
    return next((fr for fr in sorted(frames) if not frames[fr].endswith(NO_INFO)), None)


def without_tft(info):
    return re.sub(r"T-FT [0-9.]+ ms", "T-FT", info)


def first_diff(a, b, shift, norm=lambda s: s):
    """First frame of A whose info differs from frame (A + shift) of B."""
    for fr in sorted(a):
        if fr + shift in b and norm(a[fr]) != norm(b[fr + shift]):
            return fr
    return None


def show(a, b, shift, frame, context):
    for fr in range(frame - context, frame + context + 1):
        if fr not in a or fr + shift not in b:
            continue
        same = a[fr] == b[fr + shift]
        print(f"{'  ' if same else '!!'} A {fr}: {a[fr]}")
        if not same:
            print(f"   B {fr + shift}: {b[fr + shift]}")


def print_first_diff(a_path, b_path):
    """Prints the first frame present in both traces whose info differs, or nothing."""
    a, b = load(a_path), load(b_path)
    with_flag = all(flag is not None for fr in (a, b) for flag, _ in fr.values())
    a, b = flatten(a, with_flag), flatten(b, with_flag)
    diff = first_diff(a, b, 0)
    if diff is not None:
        print(diff)


def main():
    if sys.argv[1] == "--first-diff":
        print_first_diff(sys.argv[2], sys.argv[3])
        return
    a_path, b_path = sys.argv[1], sys.argv[2]
    context = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    until = int(sys.argv[4]) if len(sys.argv) > 4 else None
    a, b = load(a_path), load(b_path)
    with_flag = all(flag is not None for fr in (a, b) for flag, _ in fr.values())
    if not with_flag:
        print("note: a trace has no draw column, comparing the info only")
    a, b = flatten(a, with_flag), flatten(b, with_flag)
    if until is not None:
        a = {fr: info for fr, info in a.items() if fr <= until}
        b = {fr: info for fr, info in b.items() if fr <= until}
    print(f"A {a_path}: {len(a)} frames, info from frame {first_info(a)}")
    print(f"B {b_path}: {len(b)} frames, info from frame {first_info(b)}")

    diff = first_diff(a, b, 0)
    if diff is None:
        print("identical over the common frames")
        return
    print(f"\nfirst difference at frame {diff}")
    show(a, b, 0, diff, context)

    exact = [s for s in range(-10, 11) if s and first_diff(a, b, s) is None]
    if exact:
        print(f"\nB matches A exactly, shifted by {exact[0]:+d} frames")
        return
    best = max(range(-10, 11), key=lambda s: first_diff(a, b, s, without_tft) or max(a) + 1)
    best_diff = first_diff(a, b, best, without_tft)
    if best != 0 and (best_diff is None or best_diff > diff + 10):
        print(f"\nignoring T-FT, B matches A shifted by {best:+d} frames until frame {best_diff} of A")
        if best_diff is not None:
            show(a, b, best, best_diff, context)


main()
