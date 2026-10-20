#!/usr/bin/env python3
"""Replay the reviewed Cesar melody and render a compact candidate, without ML packages.

This checks reproducibility and unchanged segment timing. It does not score naturalness.
"""
import argparse
from array import array
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import wave


def floats(path):
    values = array('f')
    values.frombytes(path.read_bytes())
    if sys.byteorder != 'little':
        values.byteswap()
    if not values or not all(math.isfinite(x) for x in values):
        raise ValueError(f'Invalid audio: {path}')
    return values


def rms(values):
    return math.sqrt(math.fsum(x * x for x in values) / len(values))


def write_wav(path, values, gain):
    pcm = array('h', (round(max(-1, min(1, x * gain)) * 32767) for x in values))
    if sys.byteorder != 'little':
        pcm.byteswap()
    with wave.open(str(path), 'wb') as output:
        output.setparams((1, 2, 24000, 0, 'NONE', 'not compressed'))
        output.writeframes(pcm.tobytes())


def pcm(path):
    with wave.open(str(path), 'rb') as source:
        if (source.getnchannels(), source.getsampwidth(), source.getframerate()) != (1, 2, 24000):
            raise ValueError('Unexpected listening fixture format')
        values = array('h')
        values.frombytes(source.readframes(source.getnframes()))
        if sys.byteorder != 'little':
            values.byteswap()
        return values


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, default=root / 'build/cmake/bin/ortho-cesar-audio-probe')
    parser.add_argument('--output', type=Path, default=root / 'build/cesar-benchmark')
    args = parser.parse_args()
    fixtures = root / 'tests/fixtures/cesar-prosody'
    reviewed = json.loads((fixtures / 'psalm.review.json').read_text(encoding='utf-8'))
    for name, digest in reviewed['sha256'].items():
        if hashlib.sha256((fixtures / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f'Reviewed fixture changed: {name}')
    args.output.mkdir(parents=True, exist_ok=True)
    audio, timings = {}, {}
    for name, switches in [('baseline', []), ('accepted', ['--pitch', str(fixtures / 'psalm.pitch.tsv')]),
                           ('compact', ['--compact'])]:
        prefix = args.output / name
        command = [str(args.probe), '-', str(fixtures / 'psalm.ipa.txt'), str(prefix.with_suffix('.wav')),
                   '--ipa', '--trace', str(prefix), '--raw', str(prefix.with_suffix('.f32')), *switches]
        subprocess.run(command, check=True)
        audio[name] = floats(prefix.with_suffix('.f32'))
        with prefix.with_suffix('.timing.tsv').open(encoding='utf-8', newline='') as source:
            timings[name] = list(csv.DictReader(source, delimiter='\t'))
        if len(audio[name]) != reviewed['samples']:
            raise ValueError(f'{name} changed the reviewed passage duration')
        if timings[name] != timings['baseline']:
            raise ValueError(f'{name} changed a sound, its duration, or a pause')
    gains = {name: 0.09 / rms(samples) for name, samples in audio.items()}
    headroom = max(1, *(max(abs(x) for x in values) * gains[name] / 0.98 for name, values in audio.items()))
    for name, values in audio.items():
        write_wav(args.output / f'{name}.wav', values, gains[name] / headroom)
    differences = {}
    for name in ['baseline', 'accepted']:
        before, after = pcm(fixtures / f'psalm.{name}.wav'), pcm(args.output / f'{name}.wav')
        if len(before) != len(after):
            raise ValueError('Listening fixture length mismatch')
        error = rms([a - b for a, b in zip(before, after)])
        differences[name] = error
        # Trig implementations/compiler rounding and normalization may differ slightly.
        # The stored WAV, not an approximate pitch correlation, is the review artifact.
        if error > 2:
            raise ValueError(f'{name} no longer reproduces the reviewed audio (PCM RMS error {error})')
    result = {'samples': reviewed['samples'], 'seconds': reviewed['seconds'],
              'identical_sound_timing_and_pauses': True, 'fixture_pcm_rms_error': differences,
              'compact_listening_status': 'unreviewed prototype; not the default app voice',
              'compact_runtime': 'native rules only; no reference recording or neural model'}
    (args.output / 'validation.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
