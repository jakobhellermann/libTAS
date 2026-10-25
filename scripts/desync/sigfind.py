# /// script
# dependencies = ["capstone", "pyelftools"]
# ///
"""Finds functions of a symbolized ELF in a stripped build of nearly the same code.

usage: uv run sigfind.py <reference .so> <reference debug file> <target .so> <symbol>...
       uv run sigfind.py --name <reference .so> <reference debug file> <target .so> <target offset>...

Bytes are masked where they depend on the build (relative branch targets, RIP-relative
displacements). The first form searches each reference symbol in the target; --name
searches the code around a target offset in the reference and names the function there.
"""
import re
import sys

from capstone import CS_ARCH_X86, CS_MODE_64, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP
from elftools.elf.elffile import ELFFile


def symbols(debug_path):
    with open(debug_path, "rb") as f:
        elf = ELFFile(f)
        table = elf.get_section_by_name(".symtab")
        return {s.name: (s["st_value"], s["st_size"]) for s in table.iter_symbols() if s["st_size"]}


def exec_segments(path):
    with open(path, "rb") as f:
        elf = ELFFile(f)
        data = open(path, "rb").read()
        return [(seg["p_vaddr"], data[seg["p_offset"]:seg["p_offset"] + seg["p_filesz"]])
                for seg in elf.iter_segments() if seg["p_type"] == "PT_LOAD" and seg["p_flags"] & 1]


def read_at(segments, addr, size):
    for base, data in segments:
        if base <= addr < base + len(data):
            return data[addr - base:addr - base + size]
    raise ValueError(f"{addr:#x} is not in an executable segment")


def masked_pattern(md, code, addr):
    """Regex bytes for code, with build-dependent operand bytes as wildcards."""
    pattern = b""
    for insn in md.disasm(code, addr):
        raw = bytearray(insn.bytes)
        wild = set()
        if insn.disp_size and any(op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP for op in insn.operands):
            wild.update(range(insn.disp_offset, insn.disp_offset + insn.disp_size))
        if insn.group(1) or insn.group(2) or insn.group(7):  # jump, call, relative branch
            if insn.imm_size and any(op.type == X86_OP_IMM for op in insn.operands):
                wild.update(range(insn.imm_offset, insn.imm_offset + insn.imm_size))
        for i, byte in enumerate(raw):
            pattern += b"." if i in wild else re.escape(bytes([byte]))
    return pattern


def name_offsets(ref_so, ref_debug, target_so, offsets, md):
    syms = sorted((addr, size, name) for name, (addr, size) in symbols(ref_debug).items())
    ref_segs, target_segs = exec_segments(ref_so), exec_segments(target_so)
    for addr in offsets:
        # grow the window around the offset until it matches exactly once in the reference
        for before, after in ((16, 32), (32, 64), (64, 128), (128, 256), (256, 512)):
            code = read_at(target_segs, addr - before, before + after)
            # decode from the first start that lands on an instruction at the offset itself
            skip = next((k for k in range(16)
                         if any(i.address == addr for i in md.disasm(code[k:], addr - before + k))), 0)
            regex = re.compile(masked_pattern(md, code[skip:], addr - before + skip), re.DOTALL)
            hits = [base + m.start() + before - skip for base, data in ref_segs for m in regex.finditer(data)]
            if len(hits) == 1:
                break
        if len(hits) != 1:
            print(f"{addr:#x}: {len(hits)} matches in the reference")
            continue
        ref = hits[0]
        owner = [(a, n) for a, size, n in syms if a <= ref < a + size]
        label = f"{owner[-1][1]}+{ref - owner[-1][0]:#x}" if owner else "no symbol"
        print(f"{addr:#x}: reference {ref:#x} {label}")


def main():
    if sys.argv[1] == "--name":
        md = Cs(CS_ARCH_X86, CS_MODE_64)
        md.detail = True
        ref_so, ref_debug, target_so, *offsets = sys.argv[2:]
        name_offsets(ref_so, ref_debug, target_so, [int(o, 16) for o in offsets], md)
        return
    ref_so, ref_debug, target_so, *names = sys.argv[1:]
    syms = symbols(ref_debug)
    ref_segs, target_segs = exec_segments(ref_so), exec_segments(target_so)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    for name in names:
        if name not in syms:
            print(f"{name}: not in the reference symbols")
            continue
        addr, size = syms[name]
        code = read_at(ref_segs, addr, size)
        regex = re.compile(masked_pattern(md, code, addr), re.DOTALL)
        hits = [base + m.start() for base, data in target_segs for m in regex.finditer(data)]
        found = " ".join(f"{h:#x}" for h in hits[:5]) or "-"
        print(f"{name} (ref {addr:#x}, {size} bytes): {len(hits)} match(es) {found}")


main()
