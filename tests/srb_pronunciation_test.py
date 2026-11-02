#!/usr/bin/env python3
"""The supplemental lexicon must not guess ambiguous pronunciations or acronyms."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/content"))
from build_srb_pronunciations import nst_pronunciations


def row(word, phonemes):
    fields = [word] + [""] * 10 + [phonemes]
    return ";".join(fields).encode("latin-1")


class PronunciationTest(unittest.TestCase):
    def test_inflection_and_case(self):
        rows = [row("klentrogna", '\"\"kle:n$%tru:$gna'), row("KLENTROGNA", '\"ko:')]
        self.assertEqual(nst_pronunciations(rows, {"klentrogna"}), {"klentrogna": "kl'e:ntR,u:gna"})

    def test_ambiguity_and_unsupported_variant(self):
        rows = [row("prov", '\"pru:v'), row("prov", '\"pro:v'),
                row("okänd", '\"ko:'), row("okänd", "?"), row("annan", '\"an:an')]
        self.assertEqual(nst_pronunciations(rows, {"prov", "okänd"}), {})


if __name__ == "__main__":
    unittest.main()
