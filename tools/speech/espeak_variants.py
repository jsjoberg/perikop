#!/usr/bin/env python3
"""Render our Swedish pronunciation through an installed eSpeak NG with each candidate setting.

Writes loudness-matched WAVs and a page listing them, for choosing settings by ear.
"""
import argparse
from array import array
import html
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

from cesar_benchmark import rms
from espeak_swedish import to_espeak

TEXTS = ['Herren är min herde, mig skall intet fattas.',
         'Saliga äro de som hungra och törsta efter rättfärdighet, ty de skola varda mättade.']
# (label, voice, extra arguments, unstress function words)
SETTINGS = [
    ('As you heard it: default voice, every word stressed', 'sv', [], False),
    ('Default voice, function words unstressed', 'sv', [], True),
    ('Slower (150 words/min)', 'sv', ['-s', '150'], True),
    *((f'Variant {v}', f'sv+{v}', [], True)
      for v in ['klatt', 'klatt2', 'klatt3', 'klatt4', 'klatt6', 'm1', 'm3', 'm7', 'david', 'f2']),
]
TARGET_RMS = 0.08


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack', type=Path, default=root / 'build/kokoro-pack')
    parser.add_argument('--probe', type=Path, default=root / 'build/cmake/bin/ortho-cesar-audio-probe')
    parser.add_argument('--output', type=Path, default=root / 'build/espeak-variants')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    with tempfile.TemporaryDirectory() as scratch:
        scratch = Path(scratch)
        ipas = []
        for text in TEXTS:
            (scratch / 'text.txt').write_text(text)
            out = subprocess.run([str(args.probe), str(args.pack), str(scratch / 'text.txt'), str(scratch / 'x.wav')],
                                 check=True, capture_output=True, text=True).stdout
            ipas.append(next(line[4:] for line in out.splitlines() if line.startswith('ipa=')))
        for index, (label, voice, extra, unstress) in enumerate(SETTINGS):
            frames, rate = array('h'), None
            for ipa in ipas:
                subprocess.run(['espeak-ng', '-v', voice, *extra, '-w', str(scratch / 'part.wav'),
                                to_espeak(ipa, unstress)], check=True)
                with wave.open(str(scratch / 'part.wav'), 'rb') as part:
                    rate = part.getframerate()
                    frames.frombytes(part.readframes(part.getnframes()))
            if sys.byteorder != 'little':
                frames.byteswap()
            values = [x / 32768 for x in frames]
            gain = min(TARGET_RMS / rms(values), 0.98 / max(abs(x) for x in values))
            pcm = array('h', (round(x * gain * 32767) for x in values))
            if sys.byteorder != 'little':
                pcm.byteswap()
            name = f'{index:02d}.wav'
            with wave.open(str(args.output / name), 'wb') as out:
                out.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
                out.writeframes(pcm.tobytes())
            rows.append(f'<li><p>{index + 1}. {html.escape(label)}</p><audio controls preload="none" src="{name}"></audio></li>')
            print(f'{name}: {label}')
    (args.output / 'index.html').write_text(f'''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>eSpeak settings</title>
<style>:root{{--bg:#fafaf8;--ink:#1c1c1a;--muted:#6b6b66}}
@media (prefers-color-scheme: dark){{:root{{--bg:#161615;--ink:#ecece6;--muted:#a3a39b}}}}
body{{margin:0;background:var(--bg);color:var(--ink);font:16px/1.5 system-ui,sans-serif}}
main{{max-width:720px;margin:0 auto;padding:24px 16px}} p{{margin:16px 0 4px}} audio{{width:100%}}
ul{{list-style:none;padding:0}} .muted{{color:var(--muted)}}</style>
<main><h1>eSpeak settings</h1><p class="muted">Our pronunciation data in every row, matched in loudness.
Text: {html.escape(' '.join(TEXTS))}</p><ul>{''.join(rows)}</ul></main></html>
''')


if __name__ == '__main__':
    main()
