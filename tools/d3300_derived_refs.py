#!/usr/bin/env python3
"""Conservatively track derived RAM references from objdump disassembly.

Tracks short instruction sequences starting from PC-relative RAM literals
through MOV, ADD, SUB, and reports derived LDR/STR accesses (e.g. structure
fields such as 0x1049157C when only a nearby base like 0x10491468 is in ROM).
"""

import argparse
import json
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from d3300_lab import BASE

RAM_START = 0x10000000
RAM_END = 0x11000000

# Matches typical objdump disassembly line:
# 9013b9aa: 483f       ldr  r0, [pc, #0xfc]   @ 0x9013baa8 <...>
LINE_REGEX = re.compile(
    r"^[ \t]*(?P<addr>[0-9a-fA-F]+):[ \t]+(?:[0-9a-fA-F]{2,8}[ \t]+)?(?P<mnemonic>[a-zA-Z0-9_\.]+)[ \t]*(?P<ops>[^@;]*)(?:[@;][ \t]*(?P<comment>.*))?$"
)

REG_SPLIT_REGEX = re.compile(r"[\s,\[\]#]+")


def _parse_immediate(token):
    token = token.strip()
    if token.startswith("#"):
        token = token[1:].strip()
    return int(token, 0)


def _is_ram_address(value):
    return RAM_START <= value < RAM_END


def find_derived_refs(
    lines,
    target=None,
    literals=None,
    image_data=None,
    base=BASE,
    max_window=10,
):
    """Scan disassembly lines and yield derived RAM accesses.

    Parameters:
        lines: Iterable of objdump disassembly text lines.
        target: Optional integer address to filter results.
        literals: Optional dict mapping ROM address (int) -> RAM value (int).
        image_data: Optional bytes of the binary image to read literals.
        base: ROM base address (default 0x90020000).
        max_window: Maximum number of instructions to track a register after load.

    Yields dicts describing each derived access.
    """
    if literals is None:
        literals = {}

    def resolve_literal(rom_addr):
        if rom_addr in literals:
            return literals[rom_addr]
        if image_data is not None:
            text_offset = 0x100 if image_data.startswith(b"\x7fELF") else 0
            offset = text_offset + (rom_addr - base)
            if 0 <= offset <= len(image_data) - 4:
                return struct.unpack_from("<I", image_data, offset)[0]
        return None

    # Tracked registers: reg_name -> {
    #   "value": int,
    #   "base_literal": int,
    #   "start_addr": int,
    #   "steps": int,
    # }
    tracked = {}

    for raw_line in lines:
        line = raw_line.strip()
        if not line:
            continue

        match = LINE_REGEX.match(line)
        if not match:
            # Check for synthetic .word literal lines: e.g. "9000000c: .word 0x10491468"
            word_match = re.search(r"^[ \t]*([0-9a-fA-F]+):.*\.word[ \t]+(0x[0-9a-fA-F]+)", line)
            if word_match:
                w_addr = int(word_match.group(1), 16)
                w_val = int(word_match.group(2), 16)
                literals[w_addr] = w_val
            continue

        addr = int(match.group("addr"), 16)
        mnemonic = match.group("mnemonic").lower()
        ops_str = match.group("ops").strip()
        comment = match.group("comment") or ""

        # Increment step count for all tracked registers; drop expired
        expired = []
        for reg, state in tracked.items():
            state["steps"] += 1
            if state["steps"] > max_window:
                expired.append(reg)
        for reg in expired:
            del tracked[reg]

        # Reset tracked registers on unconditional branches / subroutines / returns
        if mnemonic in ("b", "bx", "bl", "blx", "cbz", "cbnz", "pop", "pop.w", "push", "push.w"):
            if mnemonic.startswith("pop") or mnemonic in ("bx", "b"):
                tracked.clear()
            continue

        # Parse operands
        tokens = [token for token in REG_SPLIT_REGEX.split(ops_str) if token]

        # Check for LDR reg, [pc, #imm]
        if (mnemonic.startswith("ldr") and len(tokens) >= 3
                and tokens[1].lower() == "pc"
                and not mnemonic.startswith("ldrd") and not mnemonic.startswith("ldm")):
            dest_reg = tokens[0].lower()
            imm = _parse_immediate(tokens[2])
            # PC in Thumb: (addr + 4) & ~3
            literal_addr = ((addr + 4) & ~3) + imm

            val = None
            # Check comment for explicit RAM value or literal address
            comment_hex = re.findall(r"0x[0-9a-fA-F]+", comment)
            for hex_str in comment_hex:
                num = int(hex_str, 16)
                if _is_ram_address(num):
                    val = num
                    break

            if val is None:
                val = resolve_literal(literal_addr)

            if val is not None and _is_ram_address(val):
                tracked[dest_reg] = {
                    "value": val,
                    "base_literal": val,
                    "start_addr": addr,
                    "steps": 0,
                }
            elif dest_reg in tracked:
                del tracked[dest_reg]
            continue

        # Check for MOV / MOVS reg_dest, reg_src
        if mnemonic in ("mov", "movs", "mov.w") and len(tokens) == 2:
            dest_reg = tokens[0].lower()
            src_reg = tokens[1].lower()
            if src_reg in tracked:
                tracked[dest_reg] = dict(tracked[src_reg])
                tracked[dest_reg]["steps"] = 0
            elif dest_reg in tracked:
                del tracked[dest_reg]
            continue

        # Check for ADD / ADDS / ADDW
        if mnemonic in ("add", "adds", "addw", "add.w"):
            if len(tokens) == 2:
                # add reg, #imm
                reg = tokens[0].lower()
                try:
                    imm = _parse_immediate(tokens[1])
                    if reg in tracked:
                        tracked[reg]["value"] += imm
                except ValueError:
                    if reg in tracked:
                        del tracked[reg]
            elif len(tokens) >= 3:
                # add dest, src, #imm
                dest_reg = tokens[0].lower()
                src_reg = tokens[1].lower()
                try:
                    imm = _parse_immediate(tokens[2])
                    if src_reg in tracked:
                        tracked[dest_reg] = {
                            "value": tracked[src_reg]["value"] + imm,
                            "base_literal": tracked[src_reg]["base_literal"],
                            "start_addr": tracked[src_reg]["start_addr"],
                            "steps": 0,
                        }
                    elif dest_reg in tracked:
                        del tracked[dest_reg]
                except ValueError:
                    if dest_reg in tracked:
                        del tracked[dest_reg]
            continue

        # Check for SUB / SUBS / SUBW
        if mnemonic in ("sub", "subs", "subw", "sub.w"):
            if len(tokens) == 2:
                reg = tokens[0].lower()
                try:
                    imm = _parse_immediate(tokens[1])
                    if reg in tracked:
                        tracked[reg]["value"] -= imm
                except ValueError:
                    if reg in tracked:
                        del tracked[reg]
            elif len(tokens) >= 3:
                dest_reg = tokens[0].lower()
                src_reg = tokens[1].lower()
                try:
                    imm = _parse_immediate(tokens[2])
                    if src_reg in tracked:
                        tracked[dest_reg] = {
                            "value": tracked[src_reg]["value"] - imm,
                            "base_literal": tracked[src_reg]["base_literal"],
                            "start_addr": tracked[src_reg]["start_addr"],
                            "steps": 0,
                        }
                    elif dest_reg in tracked:
                        del tracked[dest_reg]
                except ValueError:
                    if dest_reg in tracked:
                        del tracked[dest_reg]
            continue

        # Check for STR / STRB / STRH / STR.W
        if mnemonic.startswith("str") and not mnemonic.startswith("strd") and not mnemonic.startswith("stm"):
            if len(tokens) >= 2:
                base_reg = tokens[1].lower()
                imm = _parse_immediate(tokens[2]) if len(tokens) >= 3 else 0
                if base_reg in tracked:
                    derived = tracked[base_reg]["value"] + imm
                    if target is None or derived == target:
                        yield {
                            "address": "0x%08X" % addr,
                            "mnemonic": mnemonic,
                            "register": base_reg,
                            "base_address": "0x%08X" % tracked[base_reg]["base_literal"],
                            "offset": derived - tracked[base_reg]["base_literal"],
                            "derived_address": "0x%08X" % derived,
                            "access": "store",
                            "instruction": ops_str,
                        }
            continue

        # Check for LDR / LDRB / LDRH / LDR.W (non PC-relative)
        if mnemonic.startswith("ldr") and not mnemonic.startswith("ldrd") and not mnemonic.startswith("ldm"):
            if len(tokens) >= 2:
                dest_reg = tokens[0].lower()
                base_reg = tokens[1].lower()
                imm = _parse_immediate(tokens[2]) if len(tokens) >= 3 else 0
                if base_reg in tracked:
                    derived = tracked[base_reg]["value"] + imm
                    if target is None or derived == target:
                        yield {
                            "address": "0x%08X" % addr,
                            "mnemonic": mnemonic,
                            "register": base_reg,
                            "base_address": "0x%08X" % tracked[base_reg]["base_literal"],
                            "offset": derived - tracked[base_reg]["base_literal"],
                            "derived_address": "0x%08X" % derived,
                            "access": "load",
                            "instruction": ops_str,
                        }
                if dest_reg in tracked:
                    del tracked[dest_reg]
            continue

        # If destination register is modified by other instruction, clobber it
        if tokens:
            dest_reg = tokens[0].lower()
            if dest_reg in tracked:
                del tracked[dest_reg]


def _find_objdump():
    for name in (
        "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/llvm-objdump",
        "llvm-objdump",
        "arm-none-eabi-objdump",
        "arm-linux-gnueabi-objdump",
        "objdump",
    ):
        path = shutil.which(name) if not name.startswith("/") else (name if Path(name).exists() else None)
        if path:
            return path
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="ELF file, decoded b860_102.bin, or disassembly text file")
    parser.add_argument("--target", type=lambda x: int(x, 0), help="target RAM address to find (e.g. 0x1049157C)")
    parser.add_argument("--window", type=int, default=10, help="maximum instruction window to track registers (default 10)")
    parser.add_argument("--json", action="store_true", help="output matches as JSON array")
    parser.add_argument("--start", type=lambda x: int(x, 0), help="start address for disassembly")
    parser.add_argument("--stop", type=lambda x: int(x, 0), help="stop address for disassembly")
    args = parser.parse_args(argv)

    input_path = Path(args.input)
    if not input_path.exists():
        parser.error(f"File not found: {args.input}")

    data = None
    lines = []

    # If input is already disassembly text
    try:
        sample = input_path.read_text(errors="strict")
        lines = sample.splitlines()
    except UnicodeDecodeError:
        # Binary or ELF: run objdump
        data = input_path.read_bytes()
        objdump = _find_objdump()
        if not objdump:
            parser.error("No objdump found in PATH or Xcode toolchain")
        cmd = [objdump, "-d", "--triple=thumbv7-none-eabi"]
        if args.start is not None:
            cmd.extend([f"--start-address={args.start:#x}"])
        if args.stop is not None:
            cmd.extend([f"--stop-address={args.stop:#x}"])
        cmd.append(str(input_path))
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        lines = proc.stdout.splitlines()

    matches = list(find_derived_refs(
        lines,
        target=args.target,
        image_data=data,
        max_window=args.window,
    ))

    if args.json:
        print(json.dumps(matches, indent=2))
    else:
        for m in matches:
            print(f"{m['address']}: {m['access']} -> {m['derived_address']} (base {m['base_address']} + 0x{m['offset']:X}) via {m['instruction']}")


if __name__ == "__main__":
    main()
