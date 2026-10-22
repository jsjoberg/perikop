"""NST-derived IPA to eSpeak NG Swedish phoneme codes.

Perikop stores and speaks Swedish pronunciations as eSpeak NG phoneme codes
(ph_swedish and base1, as in eSpeak NG 1.52). NST publishes SAMPA, which
build_homographs.py reads as IPA first; this table finishes that conversion.
eSpeak's r is English, so Swedish r is R. Retroflex stops have no code and are
written r + dental, as eSpeak's own rules do. Some IPA distinctions, such as
vowel length in ʉ, ø, œ and ɑ, have no eSpeak counterpart.
"""

CODES = {
    "iː": "i:", "i": "i", "ɪ": "I", "eː": "e:", "e": "e", "ɛː": "E:", "ɛ": "E",
    "ə": "E-", "yː": "y:", "y": "y", "ʏ": "y", "ʉː": "u-", "ʉ": "u-", "ɨː": "u-",
    "ɵ": "8", "øː": "Y:", "ø": "Y:", "œː": "W", "œ": "W", "æː": "&:", "æ": "&",
    "ɑː": "A:", "ɑ": "A:", "a": "a", "oː": "o:", "o": "o", "ɔ": "O", "uː": "u:",
    "u": "u", "ʊ": "U", "p": "p", "b": "b", "t": "t", "d": "d", "k": "k",
    "ɡ": "g", "m": "m", "n": "n", "ŋ": "N", "f": "f", "v": "v", "s": "s",
    "h": "h", "l": "l", "j": "j", "r": "R", "ɕ": "S;", "ɧ": "sx", "ʂ": "s.",
    "ʈ": "Rt", "ɖ": "Rd", "ɳ": "n.", "ɭ": "l.", "ʧ": "tS", "ˈ": "'", "ˌ": ",",
}


def phones(ipa):
    """One symbol per phone, with a following length mark attached; spaces are ignored."""
    result = []
    for symbol in ipa.replace(" ", ""):
        if symbol == "ː" and result:
            result[-1] += symbol
        else:
            result.append(symbol)
    return result


def to_espeak(ipa, separator=""):
    """eSpeak codes for one word's IPA; raises on a symbol without a code."""
    try:
        return separator.join(CODES[phone] for phone in phones(ipa))
    except KeyError as missing:
        raise ValueError(f"No eSpeak code for {missing} in {ipa!r}") from None
