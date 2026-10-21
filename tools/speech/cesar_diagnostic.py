#!/usr/bin/env python3
"""Build a blind Cesar listening test whose answers say why an item failed.

Each item is rendered through the app's own front end by Cesar (production settings) and by
Björn as a same-instructions reference, matched in RMS with constant gain only. The page hides
the text until the listener has typed what they heard, then records error categories.
"""
import argparse
import base64
import json
from pathlib import Path
import subprocess
import tempfile

from cesar_benchmark import floats, rms, write_wav

# (id, focus, text). Sentences s1–s4 are deliberately meaningless so context cannot fill gaps.
ITEMS = [
    ('s1', 'unpredictable sentence', 'Den gröna stolen dricker ett snabbt berg.'),
    ('s2', 'unpredictable sentence', 'Varför sjunger lampan under det kalla fönstret?'),
    ('s3', 'unpredictable sentence', 'En tyst kyrka säljer sju röda skjortor.'),
    ('s4', 'unpredictable sentence', 'Fåglarna läser vägen bakom den mjuka sjön.'),
    ('q1', 'vowel length', 'tak, tack'),
    ('q2', 'vowel length', 'vila, villa'),
    ('q3', 'vowel length', 'glas, glass'),
    ('q4', 'vowel length', 'väg, vägg'),
    ('v1', 'vowels', 'sil, syl, sul'),
    ('v2', 'vowels', 'bok, bock'),
    ('v3', 'vowels', 'hus, hund'),
    ('c1', 'sj-sound', 'sjö, sju, skjorta'),
    ('c2', 'tj-sound', 'kyrka, tjugo, kör'),
    ('c3', 'r combinations', 'barn, bord, fors'),
    ('a1', 'word accent 1 on "anden" (duck)', 'Anden simmar i dammen.'),
    ('a2', 'word accent 2 on "Ande"', 'Herrens Ande vilar över honom.'),
    ('n1', 'name', 'Nebukadnessar, kung i Babylon.'),
    ('n2', 'name', 'Jesus gick till Getsemane.'),
    ('n3', 'numbers', 'Kapitel 23, vers 14.'),
    ('k1', 'compounds', 'himmelriket och kyrkogården'),
    ('p1', 'question melody', 'Vem är min nästa?'),
    ('p2', 'familiar text', 'Herren är min herde, mig skall intet fattas.'),
]
TARGET_RMS, PEAK = 0.08, 0.98


def gain(values):
    return min(TARGET_RMS / rms(values), PEAK / max(abs(x) for x in values))


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack', type=Path, default=root / 'build/kokoro-pack')
    parser.add_argument('--bin', type=Path, default=root / 'build/cmake/bin')
    parser.add_argument('--output', type=Path, default=root / 'build/cesar-diagnostic')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    items = []
    with tempfile.TemporaryDirectory() as scratch:
        scratch = Path(scratch)
        for key, focus, text in ITEMS:
            source = scratch / 'input.txt'
            source.write_text(text)
            result = subprocess.run([str(args.bin / 'ortho-cesar-audio-probe'), str(args.pack), str(source),
                                     str(scratch / 'cesar.wav'), '--raw', str(scratch / 'cesar.f32')],
                                    check=True, capture_output=True, text=True).stdout
            ipa = next(line[4:] for line in result.splitlines() if line.startswith('ipa='))
            tokens = subprocess.run([str(args.bin / 'ortho-kokoro-text-probe'), str(args.pack)], input=text,
                                    check=True, capture_output=True, text=True).stdout
            (scratch / 'tokens.txt').write_text(tokens)
            subprocess.run([str(args.bin / 'ortho-kokoro-audio-probe'), str(args.pack), str(scratch / 'tokens.txt'),
                            'bjorn', str(scratch / 'bjorn.f32')], check=True, capture_output=True)
            audio = {}
            for voice in ('cesar', 'bjorn'):
                values = floats(scratch / f'{voice}.f32')
                wav = args.output / f'{key}-{voice}.wav'
                write_wav(wav, values, gain(values))
                audio[voice] = 'data:audio/wav;base64,' + base64.b64encode(wav.read_bytes()).decode()
            items.append({'id': key, 'focus': focus, 'text': text, 'ipa': ipa, **audio})
            print(f'{key}: {ipa}')
    page = (Path(__file__).parent / 'cesar_diagnostic.html').read_text()
    (args.output / 'index.html').write_text(page.replace('/*ITEMS*/[]', json.dumps(items, ensure_ascii=False)))
    print(f'{len(items)} items; open {args.output / "index.html"}')


if __name__ == '__main__':
    main()
