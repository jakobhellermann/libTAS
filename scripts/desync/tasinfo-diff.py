"""Compares two traces of the TasInfo payload by Unity frame (its `frame=` field) and shows the
first frame whose values differ. Works on valo's trace.log and on tasinfo-trace.lua output.

usage: tasinfo-diff.py <trace A> <trace B> [context frames] [ignored field...]
"""
import re
import sys

FIELDS = re.compile(r"(\w+)=(\S+)")


def load(path, ignored):
    """Unity frame -> fields of the last line reporting that frame."""
    frames = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            game = line.split("game=[", 1)[1].rsplit("]", 1)[0] if "game=[" in line else line.split("\t")[-1]
            fields = {k: v for k, v in FIELDS.findall(game) if k not in ignored}
            if "frame" in fields:
                frames[int(fields["frame"])] = fields
    return frames


def main():
    context = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    ignored = set(sys.argv[4:])
    a, b = load(sys.argv[1], ignored), load(sys.argv[2], ignored)
    common = sorted(set(a) & set(b))
    print(f"A: {len(a)} Unity frames, B: {len(b)}, common {common[0] if common else '-'}..{common[-1] if common else '-'}")
    first = next((f for f in common if a[f] != b[f]), None)
    if first is None:
        print("identical over the common frames")
        return
    keys = [k for k in a[first] if a[first].get(k) != b[first].get(k)]
    print(f"first difference at Unity frame {first} in {', '.join(keys)}")
    for f in range(first - context, first + context + 1):
        if f not in a or f not in b:
            continue
        diff = [k for k in sorted(set(a[f]) | set(b[f])) if a[f].get(k) != b[f].get(k)]
        print(f"  {f}: " + (" ".join(f"{k}: {a[f].get(k)} | {b[f].get(k)}" for k in diff) or "equal"))


main()
