"""Package the checked-in PNG artwork as Windows ICO and macOS ICNS.

Run: uv run --locked tools/make_icon.py [output-directory]
Only Python's standard library is required. No build invokes this tool.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct


ARTWORK = Path(__file__).resolve().parents[1] / "resources" / "icons"


def png(name: str, size: int) -> bytes:
    path = ARTWORK / "OrthodoxReader.iconset" / name
    data = path.read_bytes()
    if (len(data) < 24 or data[:16] != b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR"
            or struct.unpack_from(">II", data, 16) != (size, size)):
        raise ValueError(f"Expected a {size}x{size} PNG: {path}")
    return data


def windows_icon() -> bytes:
    sizes = (16, 32, 48, 64, 128, 256)
    images = [png(f"icon_{s}x{s}.png" if s != 64 else "icon_32x32@2x.png", s)
              for s in sizes]
    entries = []
    offset = 6 + 16 * len(images)
    for size, image in zip(sizes, images):
        entries.append(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0,
                                   1, 32, len(image), offset))
        offset += len(image)
    return struct.pack("<HHH", 0, 1, len(images)) + b"".join(entries + images)


def macos_icon() -> bytes:
    # Native and Retina representations use PNG records supported by macOS 13.4+.
    representations = (
        (b"icp4", 16, "icon_16x16.png"),
        (b"icp5", 32, "icon_32x32.png"),
        (b"icp6", 64, "icon_32x32@2x.png"),
        (b"ic07", 128, "icon_128x128.png"),
        (b"ic08", 256, "icon_256x256.png"),
        (b"ic09", 512, "icon_512x512.png"),
        (b"ic10", 1024, "icon_512x512@2x.png"),
        (b"ic11", 32, "icon_16x16@2x.png"),
        (b"ic12", 64, "icon_32x32@2x.png"),
        (b"ic13", 256, "icon_128x128@2x.png"),
        (b"ic14", 512, "icon_256x256@2x.png"),
    )
    records = []
    for kind, size, name in representations:
        image = png(name, size)
        records.append(struct.pack(">4sI", kind, len(image) + 8) + image)
    body = b"".join(records)
    return struct.pack(">4sI", b"icns", len(body) + 8) + body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", type=Path, default=ARTWORK)
    args = parser.parse_args()
    ico, icns = windows_icon(), macos_icon()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "OrthodoxReader.ico").write_bytes(ico)
    (args.output / "OrthodoxReader.icns").write_bytes(icns)
    if args.output.resolve() == ARTWORK:
        manifest_path = ARTWORK.parent / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        assets = manifest["assets"]
        known = {asset.get("path") for asset in assets}
        for path in sorted(ARTWORK.rglob("*")):
            if not path.is_file():
                continue
            relative = path.relative_to(ARTWORK.parents[1]).as_posix()
            if relative not in known:
                assets.append({"path": relative, "license": "MIT"})
        for asset in assets:
            if asset.get("path", "").startswith("resources/icons/"):
                path = ARTWORK.parents[1] / asset["path"]
                asset["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Packaged Windows ICO and macOS ICNS in {args.output}")


if __name__ == "__main__":
    main()
