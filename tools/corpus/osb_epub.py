"""Read the user's local OSB reference for development; never ship its wording."""

from collections import OrderedDict
from pathlib import Path
import re
import xml.etree.ElementTree as ET
import zipfile

from audit_osb import ALIASES, ANCHOR


def read(path: Path):
    verses = OrderedDict()
    current = None
    pending = []

    def flush():
        nonlocal pending
        text = " ".join("".join(pending).split())
        if current and text:
            if current in verses:
                raise ValueError(f"Repeated OSB verse: {current}")
            verses[current] = text
        pending = []

    def walk(element):
        nonlocal current
        tag = element.tag.rsplit("}", 1)[-1]
        match = ANCHOR.fullmatch(element.get("id", ""))
        if match:
            flush()
            prefix, chapter, verse = match.groups()
            current = (ALIASES.get(prefix, prefix), int(chapter), int(verse), "")
            # Chapter initials and superscripts contain the reference label, not words.
            if tag == "sup" or element.get("class") == "chbeg":
                return
        if tag == "sup":
            label = "".join(element.itertext()).strip()
            if current and current[0] == "Esth" and re.fullmatch(r"\d+[a-z]?", label):
                flush()
                number, suffix = re.fullmatch(r"(\d+)([a-z]?)", label).groups()
                current = ("Esth", current[1], int(number), suffix)
            return  # Notes, variants and liturgical callouts are not Scripture.
        if element.text:
            pending.append(element.text)
        for child in element:
            walk(child)
            if child.tail:
                pending.append(child.tail)

    with zipfile.ZipFile(path) as archive:
        # Spine order matters for split book files. Do not use ZIP entry order.
        container = ET.fromstring(archive.read("META-INF/container.xml"))
        opf_name = next(
            e.get("full-path") for e in container.iter() if e.tag.endswith("rootfile")
        )
        opf = ET.fromstring(archive.read(opf_name))
        manifest = {
            e.get("id"): e.get("href") for e in opf.iter() if e.tag.endswith("}item")
        }
        for item in opf.iter():
            if not item.tag.endswith("}itemref"):
                continue
            name = str(Path(opf_name).parent / manifest[item.get("idref")])
            root = ET.fromstring(archive.read(name))
            if not any(ANCHOR.fullmatch(e.get("id", "")) for e in root.iter()):
                continue

            # Body blocks are nested in plain wrapper divs. Side articles use styled divs.
            def blocks(element):
                tag = element.tag.rsplit("}", 1)[-1]
                if tag == "div" and element.get("style"):
                    return
                if tag in ("p", "ol"):
                    if element.get("class") in {
                        "chapter1",
                        "rindent",
                        "psalm2",
                        "olstyle",
                    }:
                        yield element
                    return
                for child in element:
                    yield from blocks(child)

            current = None
            for block in blocks(root):
                if current or any(
                    ANCHOR.fullmatch(e.get("id", "")) for e in block.iter()
                ):
                    walk(block)
                    pending.append(" ")
            flush()
    return verses
