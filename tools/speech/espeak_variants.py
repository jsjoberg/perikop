#!/usr/bin/env python3
"""Render our Swedish pronunciation through an installed eSpeak NG with each candidate setting.

Writes loudness-matched WAVs and a page listing them, for choosing settings by ear.
"""
import argparse
from array import array
import html
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import wave

from cesar_benchmark import rms
from espeak_swedish import to_espeak

TEXTS = ['Herren är min herde, mig skall intet fattas.',
         'Saliga äro de som hungra och törsta efter rättfärdighet, ty de skola varda mättade.']
# (label, voice, extra arguments, unstress function words[, low-pass Hz]). Round 4 picked the
# default synthesizer; the listener then chose stock voices and documented API settings only.
# -s and -P are the command-line forms of espeakRATE and espeakRANGE.
SETTINGS = [
    ('Round 4 row 2 (custom voice file: consonants 110, pitch 80-125)', 'sv+perikop-default', ['-s', '150'], True),
    ('Stock sv, rate 150', 'sv', ['-s', '150'], True),
    ('Stock sv, rate 150, pitch range 60', 'sv', ['-s', '150', '-P', '60'], True),
    ('Stock sv, rate 150, pitch range 70', 'sv', ['-s', '150', '-P', '70'], True),
    ('Round 4 row 6 (custom female voice file)', 'sv+perikop-f2-default', ['-s', '150'], True),
    ('Stock sv+f2, rate 150', 'sv+f2', ['-s', '150'], True),
    ('Stock sv+f2, rate 150, pitch range 60', 'sv+f2', ['-s', '150', '-P', '60'], True),
]
# Our own voice files (klatt ignores breath, tone, roughness and flutter). eSpeak only finds
# variants inside its data folder, so a folder of links to the installed data is built with these.
# The f2 rows reuse eSpeak's f2 numbers for research; a shipped female voice needs its own values.
F2 = ('formant 0 105 80 150\nformant 1 110 80 160\nformant 2 110 70 150\nformant 3 110 70 150\n'
      'formant 4 115 80 150\nformant 5 115 80 150\nformant 6 110 70 150\nformant 7 110 70 150\n'
      'formant 8 110 70 150\nstressAdd 0 0 -10 -10 0 0 10 40\necho 140 10\n')
MALE = 'consonants 110 110\npitch 80 125\n'
FEMALE = 'gender female\npitch 140 230\nconsonants 110 110\n' + F2
CUSTOM = {f'perikop-{who}{synth}': settings + klatt
          for who, settings in [('', MALE), ('f2-', FEMALE)]
          for synth, klatt in [('k6', 'klatt 6\n'), ('default', ''), ('k3', 'klatt 3\n')]}
TARGET_RMS = 0.08


def lowpass(values, rate, cutoff):
    """Second-order Butterworth low-pass (RBJ biquad)."""
    w = 2 * math.pi * cutoff / rate
    alpha = math.sin(w) / math.sqrt(2)
    b0 = b2 = (1 - math.cos(w)) / 2
    b1, a0, a1, a2 = 1 - math.cos(w), 1 + alpha, -2 * math.cos(w), 1 - alpha
    out, x1, x2, y1, y2 = [], 0, 0, 0, 0
    for x in values:
        y = (b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
        out.append(y)
        x1, x2, y1, y2 = x, x1, y, y1
    return out


def linked_data(folder):
    """A copy of the installed eSpeak data folder made of links, plus CUSTOM's voice files.

    Only the shipped variants that SETTINGS uses are linked: eSpeak lists at most 349 voices and
    silently drops the rest, which would make a custom variant fall back to the default voice.
    """
    version = subprocess.run(['espeak-ng', '--version'], check=True, capture_output=True, text=True).stdout
    installed = Path(version.split('Data at:')[1].strip())
    variants = folder / 'espeak-ng-data/voices/!v'
    variants.mkdir(parents=True)
    for source, target in [(installed, folder / 'espeak-ng-data'), (installed / 'voices', variants.parent)]:
        for item in source.iterdir():
            if not (target / item.name).exists():
                (target / item.name).symlink_to(item)
    for _, voice, *_ in SETTINGS:
        shipped = installed / 'voices/!v' / voice.partition('+')[2]
        if shipped.is_file() and not (variants / shipped.name).exists():
            (variants / shipped.name).symlink_to(shipped)
    for name, settings in CUSTOM.items():
        (variants / name).write_text(f'language variant\nname {name}\n{settings}')
    return folder


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pack', type=Path, default=root / 'build/kokoro-pack')
    parser.add_argument('--probe', type=Path, default=root / 'build/cmake/bin/ortho-cesar-audio-probe')
    parser.add_argument('--output', type=Path, default=root / 'build/espeak-variants/round5')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    with tempfile.TemporaryDirectory() as scratch:
        scratch = Path(scratch)
        data = linked_data(scratch)
        ipas = []
        for text in TEXTS:
            (scratch / 'text.txt').write_text(text)
            out = subprocess.run([str(args.probe), str(args.pack), str(scratch / 'text.txt'), str(scratch / 'x.wav')],
                                 check=True, capture_output=True, text=True).stdout
            ipas.append(next(line[4:] for line in out.splitlines() if line.startswith('ipa=')))
        for index, (label, voice, extra, unstress, *cutoff) in enumerate(SETTINGS):
            frames, rate = array('h'), None
            for ipa in ipas:
                subprocess.run(['espeak-ng', f'--path={data}', '-v', voice, *extra, '-w', str(scratch / 'part.wav'),
                                to_espeak(ipa, unstress)], check=True)
                with wave.open(str(scratch / 'part.wav'), 'rb') as part:
                    rate = part.getframerate()
                    frames.frombytes(part.readframes(part.getnframes()))
            if sys.byteorder != 'little':
                frames.byteswap()
            values = [x / 32768 for x in frames]
            if cutoff:
                values = lowpass(values, rate, cutoff[0])
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
