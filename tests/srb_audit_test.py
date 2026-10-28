#!/usr/bin/env python3
"""The independent audit must retain unfamiliar fonts, italics and word gaps."""
from pathlib import Path
import re
import sys
import unittest
import xml.etree.ElementTree as ET
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/content"))
from audit_srb import extract


class SrbAuditTest(unittest.TestCase):
    def test_fingerprints_match_the_bundled_recipe(self):
        root = Path(__file__).resolve().parents[1]
        recipe = zlib.decompress((root / "resources/content/srb2026.recipe").read_bytes()[4:])
        header = (root / "src/storage/srb2026_fingerprint.hpp").read_text()
        self.assertEqual(re.search(r'srb_heading_sha256\s*=\s*"([a-f0-9]{64})"', header)[1], recipe[-128:-64].decode())
        self.assertEqual(re.search(r'srb_verse_sha256\s*=\s*"([a-f0-9]{64})"', header)[1], recipe[-64:].decode())

    def test_fonts_notes_and_punctuation_gap(self):
        root = ET.fromstring('''<pdf2xml><page number="4">
          <fontspec id="h" size="18" family="Heading"/>
          <fontspec id="d" size="43" family="UnexpectedInitialFont"/>
          <fontspec id="b" size="16" family="UnexpectedBodyFont"/>
          <fontspec id="i" size="16" family="Italic"/>
          <fontspec id="n" size="9" family="Note"/>
          <fontspec id="v" size="14" family="Number"/>
          <text top="38" left="100" width="100" font="b" height="18">Running header</text>
          <text top="100" left="70" width="100" font="h" height="18">1 Kapitlet</text>
          <text top="140" left="40" width="30" font="d" height="18">A</text>
          <text top="147" left="70" width="70" font="b" height="18">lfa,</text>
          <text top="147" left="143" width="30" font="i" height="18">beta</text>
          <text top="148" left="173" width="5" font="n" height="18">1</text>
          <text top="147" left="178" width="40" font="b" height="18"> gamma.</text>
          <text top="170" left="40" width="7" font="v" height="18">2</text>
          <text top="170" left="47" width="100" font="b" height="18">. Delad&#173;</text>
          <text top="190" left="40" width="90" font="b" height="18">text och x&#173;y.</text>
        </page></pdf2xml>''')
        self.assertEqual(extract(root, 1)[0], {("Matt", 1, 1): "Alfa, beta gamma.",
                                        ("Matt", 1, 2): "Deladtext och x-y."})

    def test_skipped_verse_is_not_silently_renumbered(self):
        root = ET.fromstring('''<pdf2xml><page number="4">
          <fontspec id="h" size="18"/><fontspec id="b" size="16"/>
          <text top="100" left="40" width="100" font="h" height="18">1 Kapitlet</text>
          <text top="140" left="40" width="100" font="b" height="18">Alfa.</text>
          <text top="160" left="40" width="100" font="b" height="18">3. Beta.</text>
        </page></pdf2xml>''')
        verses, _ = extract(root, 1)
        self.assertEqual(set(verses), {("Matt", 1, 1)})
        self.assertIn("3. Beta.", verses["Matt", 1, 1])

    def test_old_testament_furniture_small_body_and_bold_period(self):
        root = ET.fromstring('''<pdf2xml><page number="3">
          <fontspec id="h" size="18"/><fontspec id="s" size="14" family="SameAsBody"/>
          <fontspec id="b" size="15"/><fontspec id="n" size="16"/>
          <text top="100" left="40" width="100" height="25" font="h">Kapitel 1</text>
          <text top="125" left="70" width="150" height="18" font="s">Chapter summary.</text>
          <text top="150" left="40" width="70" height="18" font="b">Small body</text>
          <text top="146" left="110" width="5" height="25" font="h">.</text>
          <text top="180" left="40" width="100" height="18" font="n">2. Next - </text>
          <text top="200" left="40" width="100" height="18" font="n">paragraph.</text>
          <text top="200" left="400" width="5" height="18" font="n">.</text>
          <text top="230" left="40" width="100" height="18" font="n">3. Part-</text>
          <text top="250" left="40" width="100" height="18" font="n">word.</text>
        </page></pdf2xml>''')
        self.assertEqual(extract(root, 0)[0], {("Gen", 1, 1): "Small body.",
                                             ("Gen", 1, 2): "Next - paragraph.",
                                             ("Gen", 1, 3): "Partword."})

    def test_psalm_superscription_is_scripture(self):
        root = ET.fromstring('''<pdf2xml><page number="725">
          <fontspec id="h" size="18"/><fontspec id="b" size="16"/>
          <text top="100" left="40" width="100" height="25" font="h">Psalm 1</text>
          <text top="150" left="40" width="100" height="18" font="b">Superscription.</text>
          <text top="180" left="40" width="100" height="18" font="b">2. Text.</text>
        </page></pdf2xml>''')
        self.assertEqual(extract(root, 0)[0], {("Ps", 1, 1): "Superscription.", ("Ps", 1, 2): "Text."})

if __name__ == "__main__":
    unittest.main()
