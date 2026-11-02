"""NST-derived IPA to eSpeak NG Swedish phoneme codes.

Perikop stores and speaks Swedish pronunciations as eSpeak NG phoneme codes
(ph_swedish and base1, as in eSpeak NG 1.52). NST SAMPA is first converted to
the same IPA as the original pronunciation pack, then to eSpeak codes.
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


# NST SAMPA to the same IPA used by the original pronunciation pack.
SYMBOLS=sorted({'x\\':'ɧ',"s'":'ɕ','s`':'ʂ','n`':'ɳ','l`':'ɭ','t`':'ʈ','d`':'ɖ','u0':'ɵ','}':'ʉ','2':'ø','9':'œ',
                'E':'ɛ','O':'ɔ','U':'ʊ','I':'ɪ','Y':'ʏ','N':'ŋ','A':'ɑ','{':'æ','@':'ə',':':'ː','g':'ɡ',
                **{c:c for c in 'abdefhijklmnoprstuvwyz'}}.items(),key=lambda kv:-len(kv[0]))
VOWELS=set('ɵʉøœɛɔʊɪʏɑæəaeiouy')
def to_ipa(sampa):
    out=''
    for raw in sampa.split('$'):
        stress,phones,i='',[],0
        while i<len(raw):
            if raw[i] in '"%':
                stress='ˈ' if raw[i]=='"' else 'ˌ'
                i+=1
                continue
            for k,v in SYMBOLS:
                if raw.startswith(k,i):
                    phones.append(v);i+=len(k);break
            else:return None
        # The stress mark goes directly before the syllable's vowel; an unstressed short e is a schwa.
        text=''
        for j,p in enumerate(phones):
            long=j+1<len(phones) and phones[j+1]=='ː'
            if p in VOWELS and stress and not any(c in VOWELS for c in text):text+=stress
            text+='ə' if p=='e' and not stress and not long else p
        out+=text
    return out
