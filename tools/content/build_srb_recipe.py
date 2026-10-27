#!/usr/bin/env python3
"""Build a text-free extraction recipe for two pinned SRB 2026 PDFs (pypdf 6.10).

PDFs and extracted text are local inputs/diagnostics only, never distribution resources.
The recipe holds byte positions, generic font mappings, and verse coordinates.
"""
import argparse, hashlib, json, re, struct, zlib
from pathlib import Path
from pypdf import PdfReader
from pypdf._cmap import get_encoding
from pypdf.generic import ContentStream, TextStringObject, ByteStringObject

ROOT = Path(__file__).resolve().parents[2]
FILES = [
    (
        "SRBhelaGT.pdf",
        "f4b3c7f1acb68dd8cb976c1a518bd5338dab8b7d8bda7340e90fdcfaaabd4b70",
    ),
    (
        "SRBNT-UNnoter.pdf",
        "59046fcd7f3633bcfd3e46b1778712101a1c820a2714069a757619cbe8b63b75",
    ),
]
BOOKS = "Gen Exod Lev Num Deut Josh Judg Ruth 1Sam 2Sam 1Kgs 2Kgs 1Chr 2Chr Ezra Neh Esth Job Ps Prov Eccl Song Isa Jer Lam Ezek Dan Hos Joel Amos Obad Jonah Micah Nah Hab Zeph Hag Zech Mal Matt Mark Luke John Acts Rom 1Cor 2Cor Gal Eph Phil Col 1Thess 2Thess 1Tim 2Tim Titus Philemon Heb James 1Peter 2Peter 1John 2John 3John Jude Rev".split()
STARTS = [
    [
        7,
        81,
        143,
        188,
        248,
        301,
        336,
        372,
        377,
        423,
        462,
        509,
        553,
        594,
        643,
        657,
        678,
        689,
        729,
        830,
        862,
        874,
        880,
        953,
        1034,
        1042,
        1115,
        1138,
        1150,
        1155,
        1164,
        1166,
        1169,
        1177,
        1180,
        1183,
        1187,
        1190,
        1203,
    ],
    [
        1213,
        1266,
        1301,
        1359,
        1401,
        1456,
        1479,
        1501,
        1516,
        1524,
        1532,
        1538,
        1543,
        1548,
        1551,
        1558,
        1563,
        1566,
        1568,
        1585,
        1591,
        1598,
        1602,
        1608,
        1609,
        1610,
        1612,
    ],
]


def strings(data):
    """Only PDF string tokens; identical small scanner is used at runtime."""
    out = []
    i = 0
    while i < len(data):
        c = data[i]
        i += 1
        if c == 37:
            while i < len(data) and data[i] not in (10, 13):
                i += 1
        elif c == 40:
            b = bytearray()
            depth = 1
            while depth:
                c = data[i]
                i += 1
                if c == 92:
                    c = data[i]
                    i += 1
                    if c in b"01234567":
                        n = bytes([c])
                        for _ in range(2):
                            if i < len(data) and data[i] in b"01234567":
                                n += data[i : i + 1]
                                i += 1
                            else:
                                break
                        b.append(int(n, 8) & 255)
                    elif c in (10, 13):
                        if c == 13 and i < len(data) and data[i] == 10:
                            i += 1
                    else:
                        b.append({110: 10, 114: 13, 116: 9, 98: 8, 102: 12}.get(c, c))
                elif c == 40:
                    depth += 1
                    b.append(c)
                elif c == 41:
                    depth -= 1
                    if depth:
                        b.append(c)
                else:
                    b.append(c)
            out.append(bytes(b))
        elif c == 60:
            if i < len(data) and data[i] == 60:
                i += 1
                continue
            end = data.index(b">", i)
            h = re.sub(rb"\s", b"", data[i:end])
            out.append(bytes.fromhex((h + b"0" * (len(h) % 2)).decode()))
            i = end + 1
    return out


def operand_strings(v):
    if isinstance(v, (TextStringObject, ByteStringObject)):
        yield v.original_bytes if isinstance(v, TextStringObject) else bytes(v)
    elif isinstance(v, list):
        for x in v:
            yield from operand_strings(x)
    elif isinstance(v, dict):
        for x in v.values():
            yield from operand_strings(x)


class Extractor:
    def __init__(self):
        self.fonts = []
        self.streams = []
        self.verses = []
        self.current = []
        self.key = None
        self.last = None
        self.drop = False
        self.headers = ""
        self.pending_dot = False
        self.diagnostics = []
        self.gt_line = []
        self.gt_y = None
        self.gt_drop = False
        self.body_end = None
        self.body_y = None
        self.body_last = ""
        self.body_drop = False

    def flush(self):
        if self.key is None:
            return
        while self.current and self.current[-1][4].isspace():
            self.current.pop()
        text = " ".join("".join(x[4] for x in self.current).split())
        assert text.strip(), self.key
        # Convert consecutive bytes with one map into one copy operation.
        pieces = []
        for stream, off, n, font, char in self.current:
            if (
                pieces
                and stream == pieces[-1][0]
                and font == pieces[-1][3]
                and off == pieces[-1][1] + pieces[-1][2]
            ):
                pieces[-1][2] += n
            else:
                pieces.append([stream, off, n, font])
        self.verses.append((self.key, pieces, text))
        self.current = []

    def begin(self, book, ch, v):
        if self.key == (book, ch, v) and not self.current:
            return
        self.flush()
        self.key = (book, ch, v)
        self.last = None
        self.drop = False
        self.pending_dot = False

    def add(self, chars, y, drop=False):
        if not chars:
            return
        if self.pending_dot:
            while chars and chars[0][4] in ". ":
                chars.pop(0)
            self.pending_dot = False
        if not chars:
            return
        if (
            self.current
            and self.last is not None
            and abs(y - self.last) > 2
            and not self.drop
        ):
            while self.current and self.current[-1][4].isspace():
                self.current.pop()
            while chars and chars[0][4].isspace():
                chars.pop(0)
            if not chars:
                return
            if self.current[-1][4] == "-":
                if chars[0][4].islower():
                    self.current.pop()
            elif not self.current[-1][4].isspace() and not chars[0][4].isspace():
                self.current.append((0, 0, 1, 255, " "))
        # Soft hyphens are discretionary PDF typesetting; never speak them.
        self.current.extend(chars)
        self.last = y
        self.drop = drop

    def flush_gt(self):
        if not self.gt_line:
            return
        chars = self.gt_line
        self.gt_line = []
        text = "".join(x[4] for x in chars)
        m = re.match(r"^\s*(\d+)(?:\.\s*|\s+(?=[A-ZÅÄÖ])|(?=[A-ZÅÄÖ]))", text)
        if m and self.key and int(m[1]) == self.key[2] + 1:
            self.begin(self.key[0], self.key[1], int(m[1]))
            chars = chars[m.end() :]
        self.add(chars, self.gt_y, self.gt_drop)

    def add_gt(self, chars, y, drop=False):
        if self.gt_line and abs(y - self.gt_y) > 2:
            self.flush_gt()
        self.gt_line.extend(chars)
        self.gt_y = y
        self.gt_drop = drop

    def document(self, path, half):
        reader = PdfReader(path)
        raw = path.read_bytes()
        previous_book = None
        chapter = 0
        starts = [x - (5 if half == 0 else 1210) for x in STARTS[half]]
        for pno, page in enumerate(reader.pages):
            if pno < starts[0]:
                continue
            bi = max(i for i, p in enumerate(starts) if p <= pno) + (
                0 if half == 0 else 39
            )
            if bi != previous_book:
                self.flush()
                self.key = None
                self.headers = ""
                chapter = (
                    1
                    if BOOKS[bi] in ("Obad", "Philemon", "2John", "3John", "Jude")
                    else 0
                )
                previous_book = bi
            if chapter and self.key is None:
                self.begin(bi, chapter, 1)
            content = page.raw_get("/Contents")
            parts = (
                list(content.get_object())
                if isinstance(content.get_object(), list)
                else [content]
            )
            tokens = []
            offsets = []
            for ref in parts:
                obj = ref.get_object()
                decoded = obj.get_data()
                part_tokens = strings(decoded)
                at = 0
                sid = len(self.streams)
                for t in part_tokens:
                    offsets.append((sid, at))
                    at += len(t)
                tokens.extend(part_tokens)
                object_offset = reader.xref[0][ref.idnum]
                m = re.search(rb"\bstream\r?\n", raw[object_offset:])
                assert m
                offset = object_offset + m.end()
                length = int(
                    re.search(rb"/Length\s+(\d+)\b", raw[object_offset:offset]).group(1)
                )
                key = reader._encryption._make_crypt_filter(
                    ref.idnum, 0
                ).stm_crypt.alg.key
                self.streams.append((half, offset, length, len(decoded), key))
            maps = {}
            names = {}
            for name, f in page["/Resources"]["/Font"].items():
                f = f.get_object()
                names[name] = str(f["/BaseFont"])
                enc, cmap = get_encoding(f)
                mapping = []
                for c in range(256):
                    s = (
                        enc.get(c, chr(c))
                        if isinstance(enc, dict)
                        else (
                            bytes([c]).decode(enc, errors="replace")
                            if enc != "utf-16-be"
                            else chr(c)
                        )
                    )
                    mapping.append(
                        "".join(cmap.get(ch, ch) for ch in s)
                        .replace("\u00ad", "-")
                        .replace("\u00a0", " ")
                    )
                if mapping not in self.fonts:
                    self.fonts.append(mapping)
                maps[name] = self.fonts.index(mapping)
            font = None
            size = 1
            tm = [1, 0, 0, 1, 0, 0]
            leading = 0
            token = 0
            number_pending = ""
            advance = 0
            tc = 0
            tw = 0
            for args, op in ContentStream(page["/Contents"], reader).operations:
                refs = []
                for b in operand_strings(args):
                    assert tokens[token] == b, (pno, token, b, tokens[token])
                    refs.append((offsets[token], b))
                    token += 1
                if op == b"Tf":
                    font = args[0]
                    size = float(args[1])
                elif op == b"Tm":
                    tm = list(map(float, args))
                    advance = 0
                elif op == b"Tc":
                    tc = float(args[0])
                elif op == b"Tw":
                    tw = float(args[0])
                elif op in (b"Td", b"TD"):
                    tx, ty = map(float, args)
                    tm[4] += tx * tm[0] + ty * tm[2]
                    tm[5] += tx * tm[1] + ty * tm[3]
                    advance = 0
                    if op == b"TD":
                        leading = -ty
                elif op in (b"T*", b"'", b'"'):
                    tm[4] -= leading * tm[2]
                    tm[5] -= leading * tm[3]
                    advance = 0
                if op not in (b"Tj", b"TJ", b"'", b'"') or not refs:
                    continue
                name = names.get(font, "")
                fs = abs(tm[3] * size)
                y = tm[5]
                chars = []
                fid = maps[font]
                mapping = self.fonts[fid]
                for (sid, start), b in refs:
                    for j, c in enumerate(b):
                        chars.append((sid, start + j, 1, fid, mapping[c]))
                text = "".join(x[4] for x in chars)
                x = tm[4] + advance * tm[0]
                f = page["/Resources"]["/Font"][font].get_object()
                widths = f.get("/Widths", [])
                first = int(f.get("/FirstChar", 0))
                shown = args[0] if op == b"TJ" else args[-1:]
                for value in shown:
                    if isinstance(value, (TextStringObject, ByteStringObject)):
                        raw_bytes = (
                            value.original_bytes
                            if isinstance(value, TextStringObject)
                            else bytes(value)
                        )
                        for code in raw_bytes:
                            width = (
                                float(widths[code - first])
                                if 0 <= code - first < len(widths)
                                else 500
                            )
                            advance += (
                                width / 1000 * size + tc + (tw if code == 32 else 0)
                            )
                    elif isinstance(
                        value, (int, float)
                    ) or value.__class__.__name__ in ("FloatObject", "NumberObject"):
                        advance -= float(value) / 1000 * size
                end_x = tm[4] + advance * tm[0]
                if (
                    half == 0
                    and "BoldMT" in name
                    and 11.5 <= fs <= 12.5
                    and text.strip() == "."
                ):
                    self.add_gt(chars, y)
                elif "BoldMT" in name and 11.5 <= fs <= 12.5:
                    if half == 0:
                        self.flush_gt()
                    self.headers += text
                    m = re.search(
                        r"(?:Kapitel\s*(\d+)|Psalm\s*(\d+)|(\d+)\s*Kapitlet)",
                        self.headers,
                    )
                    if m:
                        chapter = int(next(x for x in m.groups() if x))
                        self.begin(bi, chapter, 1)
                        self.headers = ""
                elif ("Sabon" in name and 8.0 <= fs < 35) or (
                    "TimesNewRomanPSMT" in name
                    and 22 < fs < 35
                    and len(text.strip()) == 1
                    and text.strip().isupper()
                ):
                    if not chapter:
                        raise ValueError(("Body before chapter", half, pno, text))
                    if half == 1 and 9 <= fs <= 10 and text.isdigit():
                        number_pending += text
                        continue
                    if number_pending:
                        self.begin(bi, chapter, int(number_pending))
                        self.pending_dot = True
                        number_pending = ""
                    # Small-caps display headings at 18 pt are not Scripture.
                    if 15 < fs < 22:
                        continue
                    if (
                        chars
                        and self.body_end is not None
                        and self.body_y is not None
                        and abs(y - self.body_y) < 2
                        and not self.body_drop
                        and x - self.body_end > 1.0
                        and self.body_last
                        and self.body_last[-1].isalpha()
                        and chars[0][4]
                        and chars[0][4][0].isalpha()
                    ):
                        chars.insert(0, (0, 0, 1, 255, " "))
                    self.body_end = end_x
                    self.body_y = y
                    self.body_last = text
                    self.body_drop = fs > 22
                    if half == 0:
                        self.add_gt(chars, y, drop=fs > 22)
                    else:
                        m = re.match(r"^\s*(\d+)\.\s*", text)
                        if m and self.key and int(m[1]) == self.key[2] + 1:
                            self.begin(bi, chapter, int(m[1]))
                            chars = chars[m.end() :]
                        self.add(chars, y, drop=fs > 22)
                elif (
                    half == 1
                    and "TimesNewRomanPSMT" in name
                    and 8.9 < fs < 10
                    and re.fullmatch(r"\d+\.?", text)
                    and y < 555
                ):
                    number_pending += text.rstrip(".")
                elif (
                    half == 0 and "TimesNewRomanPSMT" in name and fs >= 9.4 and y < 555
                ):
                    if re.fullmatch(r"[0-9.\s]+", text):
                        self.add_gt(chars, y)
                    else:
                        self.diagnostics.append((half, pno, fs, text))
            if half == 0:
                self.flush_gt()
            assert token == len(tokens), (pno, token, len(tokens))
        self.flush()
        self.key = None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pdf_directory", type=Path)
    ap.add_argument("--diagnostics", type=Path, required=True)
    ap.add_argument(
        "--output", type=Path, default=ROOT / "resources/content/srb2026.recipe"
    )
    args = ap.parse_args()
    ex = Extractor()
    for i, (name, sha) in enumerate(FILES):
        path = args.pdf_directory / name
        assert hashlib.sha256(path.read_bytes()).hexdigest() == sha
        ex.document(path, i)
    import collections

    actual = collections.defaultdict(list)
    for (bi, c, v), _, _ in ex.verses:
        actual[BOOKS[bi], c].append(v)
    expected = {}
    for line in (
        (ROOT / "resources/content/srb2026-chapters.tsv").read_text().splitlines()
    ):
        if line and not line.startswith("#"):
            b, c, n = line.split("\t")
            expected[b, int(c)] = int(n)
    assert len(actual) == 1189 and len(ex.verses) == 31171
    assert {
        key: len(refs) for key, refs in actual.items()
    } == expected, "Chapter inventory changed"
    assert all(
        refs == list(range(1, max(refs) + 1)) for refs in actual.values()
    ), "Incomplete chapter"
    assert all(
        v != 1 or not text[0].islower() for (_, _, v), _, text in ex.verses
    ), "Chapter opening lost its drop capital"
    args.diagnostics.mkdir(exist_ok=True, parents=True)
    (args.diagnostics / "verses.json").write_text(
        json.dumps(
            [(BOOKS[b], c, v, text) for (b, c, v), _, text in ex.verses],
            ensure_ascii=False,
            indent=1,
        )
    )
    (args.diagnostics / "unclassified.json").write_text(
        json.dumps(ex.diagnostics, ensure_ascii=False, indent=1)
    )
    # Reject duplicate coordinates before encoding the recipe.
    expected = {}
    for (b, c, v), pieces, text in ex.verses:
        if (b, c, v) in expected:
            raise ValueError(("Duplicate verse", BOOKS[b], c, v))
        expected[b, c, v] = text
    data = bytearray(b"SRB1")
    data += struct.pack("<H", len(ex.fonts))
    for mapping in ex.fonts:
        for text in mapping:
            b = text.encode()
            data += struct.pack("<B", len(b)) + b
    data += struct.pack("<I", len(ex.streams))
    for half, offset, length, decoded, key in ex.streams:
        data += struct.pack("<BIII", half, offset, length, decoded) + key
    data += struct.pack("<I", len(ex.verses))
    for (book, ch, v), pieces, text in ex.verses:
        data += struct.pack("<BBBH", book, ch, v, len(pieces))
        for stream, offset, n, font in pieces:
            data += struct.pack("<HIHB", stream, offset, n, font)
    from srb_alignment import rows, SOURCES

    alignment = rows(ex.verses, BOOKS)
    data += struct.pack("<I", len(alignment))

    def string(text):
        b = text.encode()
        return struct.pack("<B", len(b)) + b

    for source, target, kind, book, f0, f1, tbook, t0, t1 in alignment:
        data += (
            struct.pack("<BBB", SOURCES.index(source), SOURCES.index(target), kind)
            + string(book)
            + string(tbook)
        )
        for c, v, suffix in (f0, f1, t0, t1):
            data += struct.pack("<HH", c, v) + string(suffix)
    data += (
        hashlib.sha256("".join(text + "\n" for _, _, text in ex.verses).encode())
        .hexdigest()
        .encode()
    )
    args.output.write_bytes(struct.pack("<I", len(data)) + zlib.compress(data, 9))
    manifest_path = ROOT / "resources/manifest.json"
    manifest = json.loads(manifest_path.read_text())
    for asset in manifest["assets"]:
        if asset["path"] == "resources/content/srb2026.recipe":
            asset["sha256"] = hashlib.sha256(args.output.read_bytes()).hexdigest()
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    print(
        "fonts",
        len(ex.fonts),
        "streams",
        len(ex.streams),
        "verses",
        len(ex.verses),
        "recipe",
        args.output.stat().st_size,
        "unclassified",
        len(ex.diagnostics),
    )
    print(
        [(b, sum(1 for (i, c, v) in expected if i == j)) for j, b in enumerate(BOOKS)]
    )


if __name__ == "__main__":
    main()
