"""Check icon containers and their original PNG payloads without native tools."""

from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import make_icon


class IconTest(unittest.TestCase):
    def test_checked_in_containers_match_artwork(self):
        self.assertEqual((make_icon.ARTWORK / "OrthodoxReader.ico").read_bytes(), make_icon.windows_icon())
        self.assertEqual((make_icon.ARTWORK / "OrthodoxReader.icns").read_bytes(), make_icon.macos_icon())

    def test_windows_sizes_and_artwork(self):
        data = make_icon.windows_icon()
        reserved, kind, count = struct.unpack_from("<HHH", data)
        self.assertEqual((reserved, kind, count), (0, 1, 6))
        sizes = []
        end = 6 + count * 16
        for index in range(count):
            w, h, colors, reserved, planes, bits, length, offset = struct.unpack_from(
                "<BBBBHHII", data, 6 + index * 16)
            size = w or 256
            self.assertEqual((h or 256, colors, reserved, planes, bits), (size, 0, 0, 1, 32))
            self.assertEqual(offset, end)
            payload = data[offset:offset + length]
            self.assertEqual(struct.unpack_from(">II", payload, 16), (size, size))
            name = f"icon_{size}x{size}.png" if size != 64 else "icon_32x32@2x.png"
            self.assertEqual(payload, make_icon.png(name, size))
            sizes.append(size)
            end += length
        self.assertEqual(sizes, [16, 32, 48, 64, 128, 256])
        self.assertEqual(end, len(data))

    def test_macos_native_and_retina_artwork(self):
        data = make_icon.macos_icon()
        self.assertEqual(struct.unpack_from(">4sI", data), (b"icns", len(data)))
        records = {}
        offset = 8
        while offset < len(data):
            kind, size = struct.unpack_from(">4sI", data, offset)
            self.assertNotIn(kind, records)
            self.assertGreater(size, 8)
            payload = data[offset + 8:offset + size]
            self.assertEqual(payload[:8], b"\x89PNG\r\n\x1a\n")
            records[kind] = payload
            offset += size
        self.assertEqual(offset, len(data))
        self.assertEqual(len(records), 11)
        self.assertEqual(records[b"ic10"], make_icon.png("icon_512x512@2x.png", 1024))
        for kind, size, name in [
            (b"icp4", 16, "icon_16x16.png"), (b"icp5", 32, "icon_32x32.png"),
            (b"icp6", 64, "icon_32x32@2x.png"), (b"ic07", 128, "icon_128x128.png"),
            (b"ic08", 256, "icon_256x256.png"), (b"ic09", 512, "icon_512x512.png"),
            (b"ic11", 32, "icon_16x16@2x.png"), (b"ic12", 64, "icon_32x32@2x.png"),
            (b"ic13", 256, "icon_128x128@2x.png"), (b"ic14", 512, "icon_256x256@2x.png"),
        ]:
            self.assertEqual(records[kind], make_icon.png(name, size))

    def test_rejects_wrong_size_and_invalid_png(self):
        with tempfile.TemporaryDirectory() as directory:
            artwork = Path(directory)
            iconset = artwork / "OrthodoxReader.iconset"
            iconset.mkdir()
            image = iconset / "icon_16x16.png"
            with patch.object(make_icon, "ARTWORK", artwork):
                image.write_bytes(b"not a PNG")
                with self.assertRaises(ValueError):
                    make_icon.png(image.name, 16)
                image.write_bytes(b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR")
                with self.assertRaises(ValueError):
                    make_icon.png(image.name, 16)
                image.write_bytes(b"\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR" + struct.pack(">II", 32, 32))
                with self.assertRaises(ValueError):
                    make_icon.png(image.name, 16)


if __name__ == "__main__":
    unittest.main()
