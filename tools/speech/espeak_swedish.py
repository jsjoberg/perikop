#!/usr/bin/env python3
"""Convert the front end's Swedish IPA to eSpeak NG's Swedish phoneme codes ([[...]] input).

eSpeak is GPL and is only run as an installed external program; no eSpeak code or data is used.
Codes come from eSpeak NG 1.52.0's ph_swedish and base1 tables. Retroflex stops have no code
there, so they are written r + dental, as eSpeak's own Swedish rules write "kort" and "bord".
Run directly to check that eSpeak parses every converted symbol back unchanged.
"""
import subprocess
import sys

CODES = {
    'iː': 'i:', 'i': 'i', 'ɪ': 'I', 'eː': 'e:', 'e': 'e', 'ɛː': 'E:', 'ɛ': 'E', 'ə': 'E-',
    'yː': 'y:', 'y': 'y', 'ʏ': 'y', 'ʉː': 'u-', 'ʉ': 'u-', 'ɨː': 'u-', 'ɵ': '8',
    'øː': 'Y:', 'ø': 'Y:', 'œː': 'W', 'œ': 'W', 'æː': '&:', 'æ': '&',
    'ɑː': 'A:', 'ɑ': 'A:', 'a': 'a', 'oː': 'o:', 'o': 'o', 'ɔ': 'O', 'uː': 'u:', 'u': 'u', 'ʊ': 'U',
    'p': 'p', 'b': 'b', 't': 't', 'd': 'd', 'k': 'k', 'ɡ': 'g', 'm': 'm', 'n': 'n', 'ŋ': 'N',
    'f': 'f', 'v': 'v', 's': 's', 'h': 'h', 'l': 'l', 'j': 'j', 'r': 'R',
    'ɕ': 'S;', 'ɧ': 'sx', 'ʂ': 's.', 'ʈ': 'Rt', 'ɖ': 'Rd', 'ɳ': 'n.', 'ɭ': 'l.', 'ʧ': 'tS',
    'ˈ': "'", 'ˌ': ',', ' ': ' ',
}
PUNCTUATION = ',.?!;:'


def symbols(ipa):
    result = []
    for char in ipa:
        if char == 'ː' and result:
            result[-1] += char
        else:
            result.append(char)
    return result


def to_espeak(ipa):
    """eSpeak input text: phoneme codes in [[...]], with the IPA's punctuation kept outside."""
    out, word = [], []
    for symbol in symbols(ipa):
        if symbol in PUNCTUATION:
            out.append(f'[[{"".join(word).strip()}]]{symbol} ')
            word = []
        elif symbol in CODES:
            word.append(CODES[symbol])
        else:
            raise ValueError(f'No eSpeak code for {symbol!r} in {ipa!r}')
    if ''.join(word).strip():
        out.append(f'[[{"".join(word).strip()}]]')
    return ''.join(out).strip()


def main():
    # Every lexicon phone, as in tests/cesar_voice_test.cpp, plus a punctuated phrase.
    phones = ('a b d e eː f h i iː j k l m n o oː p r s t uː v y yː øː ŋ œ ɑː ɔ ɕ ɖ ə ɛ ɛː ɡ ɧ ɨː ɪ ɭ ɳ ɵ ʂ ʈ '
              'ʉː ʊ ʏ ʧ').split()
    for ipa in [*(f'ˈa{p}a' for p in phones), 'hˈɛrən ˈɛːr mˈiːn hˈeːɖə,mˈɛj skˈal ˈɪntət fˈatas.']:
        text = to_espeak(ipa)
        parsed = subprocess.run(['espeak-ng', '-v', 'sv', '-q', '-x', text], check=True,
                                capture_output=True, text=True).stdout
        # An unknown code silently drops the rest of the word, so compare the sounds; stress,
        # punctuation and eSpeak's added ; (palatal variants) are ignored.
        bare = lambda s: ''.join(c for c in s.replace('[[', '').replace(']]', '') if c not in "',.;?! \n")
        if bare(parsed) != bare(text):
            sys.exit(f'eSpeak changed {ipa!r}: sent {text!r}, parsed {parsed.strip()!r}')
    print(f'{len(phones)} phones and a phrase round-trip through eSpeak')


if __name__ == '__main__':
    main()
