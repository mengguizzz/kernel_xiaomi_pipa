#!/usr/bin/env python3
"""Add the pipa Novatek reset GPIO to a DTBO without rebuilding its overlay."""

import argparse
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path


DT_TABLE_MAGIC = 0xD7B7AB1E
HEADER = struct.Struct(">8I")
ENTRY = struct.Struct(">8I")


def run(*args: str) -> str:
    return subprocess.check_output(args, text=True).strip()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True, help="known-good pipa dtbo.img")
    parser.add_argument("--output", required=True, help="patched dtbo.img")
    parser.add_argument("--match", default="pipa", help="unique overlay search string")
    parser.add_argument("--phandle", default="0x65", help="phandle of tlmm")
    parser.add_argument("--gpio", default="0x21", help="GPIO number; 0x21 is GPIO33")
    parser.add_argument(
        "--node",
        default="/fragment@127/__overlay__/novatek@0",
        help="Novatek node path inside the pipa overlay",
    )
    args = parser.parse_args()

    if shutil.which("fdtput") is None or shutil.which("fdtget") is None:
        raise SystemExit("fdtput/fdtget are required (device-tree-compiler)")

    blob = Path(args.base).read_bytes()
    magic, _, header_size, entry_size, count, entries_offset, page_size, version = (
        HEADER.unpack_from(blob)
    )
    if magic != DT_TABLE_MAGIC or header_size != HEADER.size or entry_size != ENTRY.size:
        raise SystemExit("unsupported or invalid DTBO image")

    entries = []
    matches = []
    for index in range(count):
        fields = ENTRY.unpack_from(blob, entries_offset + index * entry_size)
        size, offset, dt_id, revision, *custom = fields
        data = blob[offset : offset + size]
        if len(data) != size:
            raise SystemExit(f"DTBO entry {index} is truncated")
        entries.append(
            {
                "size": size,
                "id": dt_id,
                "rev": revision,
                "custom": custom,
                "data": data,
            }
        )
        if args.match.encode() in data:
            matches.append(index)

    if len(matches) != 1:
        raise SystemExit(f"expected one overlay matching {args.match!r}, got {matches}")

    target = matches[0]
    with tempfile.TemporaryDirectory(prefix="pipa-dtbo-") as temp_dir:
        overlay = Path(temp_dir) / "pipa-overlay.dtbo"
        overlay.write_bytes(entries[target]["data"])
        subprocess.run(
            [
                "fdtput",
                "-t",
                "x",
                str(overlay),
                args.node,
                "novatek,reset-gpio",
                args.phandle,
                args.gpio,
                "0x00",
            ],
            check=True,
        )
        actual = run(
            "fdtget",
            "-t",
            "x",
            str(overlay),
            args.node,
            "novatek,reset-gpio",
        )
        expected = f"{int(args.phandle, 0):x} {int(args.gpio, 0):x} 0"
        if actual != expected:
            raise SystemExit(f"reset property verification failed: {actual!r}")
        entries[target]["data"] = overlay.read_bytes()
        entries[target]["size"] = len(entries[target]["data"])

    data_offset = entries_offset + count * entry_size
    payload = bytearray()
    offsets = []
    for entry in entries:
        offsets.append(data_offset + len(payload))
        payload.extend(entry["data"])

    total_size = data_offset + len(payload)
    output = bytearray(blob[:data_offset])
    HEADER.pack_into(
        output,
        0,
        DT_TABLE_MAGIC,
        total_size,
        header_size,
        entry_size,
        count,
        entries_offset,
        page_size,
        version,
    )
    for index, (entry, offset) in enumerate(zip(entries, offsets)):
        ENTRY.pack_into(
            output,
            entries_offset + index * entry_size,
            entry["size"],
            offset,
            entry["id"],
            entry["rev"],
            *entry["custom"],
        )
    output.extend(payload)
    output.extend(b"\0" * (len(blob) - len(output)))
    Path(args.output).write_bytes(output)

    print(f"patched DTBO entry {target}")
    print(f"novatek,reset-gpio = <{expected}>")
    print(f"wrote {args.output} ({len(output)} bytes)")


if __name__ == "__main__":
    main()
