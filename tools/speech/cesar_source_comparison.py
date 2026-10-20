#!/usr/bin/env python3
"""Compare Klatt and LF timbres with identical melody, sound timing and pauses.

Requires the native probe and ffmpeg for EBU R128 loudness measurement. Only a
constant gain is applied to listening copies; no EQ, compression or time stretch.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import time

from cesar_benchmark import floats, rms, write_wav


def measure(ffmpeg, path):
    result = subprocess.run([ffmpeg, '-hide_banner', '-nostdin', '-nostats', '-i', str(path),
                             '-af', 'ebur128=peak=true', '-f', 'null', '-'],
                            capture_output=True, text=True, check=True)
    values = re.findall(r'\bI:\s+(-?[\d.]+) LUFS', result.stderr)
    if not values:
        raise ValueError(f'Cannot measure integrated loudness: {path}')
    path.with_suffix('.loudness.log').write_text(result.stderr, encoding='utf-8')
    return float(values[-1])


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, default=root / 'build/cmake/bin/ortho-cesar-audio-probe')
    parser.add_argument('--output', type=Path, default=root / 'build/cesar-source-comparison')
    parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
    parser.add_argument('--target-lufs', type=float, default=-21)
    parser.add_argument('--reference-melody', action='store_true',
                        help='Use the earlier accepted reference contour instead of the compact milestone')
    parser.add_argument('--refine', action='store_true',
                        help='Also render shorter LF return, with and without smooth articulation controls')
    args = parser.parse_args()
    if not args.ffmpeg or not math.isfinite(args.target_lufs) or not -40 <= args.target_lufs <= -10:
        parser.error('ffmpeg and a finite loudness target between -40 and -10 LUFS are required')
    fixtures = root / 'tests/fixtures/cesar-prosody'
    review = json.loads((fixtures / 'psalm.review.json').read_text(encoding='utf-8'))
    for name, digest in review['sha256'].items():
        if hashlib.sha256((fixtures / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f'Reviewed fixture changed: {name}')
    args.output.mkdir(parents=True, exist_ok=True)
    melody = ['--pitch', str(fixtures / 'psalm.pitch.tsv')] if args.reference_melody else ['--compact']
    audio, gains, measurements, timing, pitch = {}, {}, {}, {}, {}
    variants = [('current', ['--source', 'klatt']), ('lf', ['--source', 'lf'])]
    if args.refine:
        brighter = ['--source', 'lf', '--lf-shape', '0.40', '0.65', '0.008']
        variants += [('lf-bright', brighter), ('lf-clear', [*brighter, '--smooth-controls'])]
    for name, switches in variants:
        prefix = args.output / name
        started = time.perf_counter()
        subprocess.run([str(args.probe), '-', str(fixtures / 'psalm.ipa.txt'), str(prefix.with_suffix('.raw.wav')),
                        '--ipa', *melody, *switches, '--trace', str(prefix),
                        '--raw', str(prefix.with_suffix('.f32'))], check=True)
        elapsed = time.perf_counter() - started
        audio[name] = floats(prefix.with_suffix('.f32'))
        if len(audio[name]) != review['samples']:
            raise ValueError(f'{name} changes the passage duration')
        # Cesar's legacy final scale is 0.9 after its synth limiter. Any samples
        # here would indicate clipped source/filter output, invalidating the comparison.
        limiter_samples = sum(abs(x) >= 0.89996 for x in audio[name])
        if limiter_samples:
            raise ValueError(f'{name} reaches the synthesizer limiter ({limiter_samples} samples)')
        timing[name] = prefix.with_suffix('.timing.tsv').read_bytes()
        pitch[name] = prefix.with_suffix('.pitch.tsv').read_bytes()
        if timing[name] != timing['current'] or pitch[name] != pitch['current']:
            raise ValueError('Source substitution changes sound timing, pauses or pitch controls')
        measured = measure(args.ffmpeg, prefix.with_suffix('.raw.wav'))
        gains[name] = 10 ** ((args.target_lufs - measured) / 20)
        measurements[name] = {'raw_lufs': measured, 'raw_rms': rms(audio[name]),
                              'raw_peak': max(abs(x) for x in audio[name]),
                              'limiter_samples': limiter_samples, 'render_wall_seconds': elapsed}
    # If needed, reduce both together to retain the level match without limiting.
    headroom = max(1, *(max(abs(x) for x in values) * gains[name] / 0.95 for name, values in audio.items()))
    expected = args.target_lufs - 20 * math.log10(headroom)
    for name, values in audio.items():
        gain = gains[name] / headroom
        path = args.output / f'{name}.wav'
        write_wav(path, values, gain)
        actual = measure(args.ffmpeg, path)
        if abs(actual - expected) > 0.2:
            raise ValueError(f'{name} loudness normalization failed: {actual} LUFS')
        measurements[name].update({'listening_lufs': actual, 'constant_gain': gain,
                                   'wav_sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    with (args.output / 'current.timing.tsv').open(encoding='utf-8', newline='') as source:
        sounds = list(csv.DictReader(source, delimiter='\t'))
    pauses = [{'start_sample': int(s['start_sample']), 'samples': int(s['samples'])}
              for s in sounds if int(s['pause'])]
    result = {'samples': review['samples'], 'seconds': review['seconds'],
              'melody': 'accepted-reference' if args.reference_melody else 'compact-milestone',
              'identical_sound_timing_pauses_and_pitch': True, 'pauses': pauses,
              'lf_parameters': {'tp': 0.40, 'te': 0.65, 'ta': 0.04},
              'lf_refinement': ({'tp': 0.40, 'te': 0.65, 'ta': 0.008,
                                 'lf-bright': 'LF return change only',
                                 'lf-clear': 'shorter return plus sample-wise articulation controls and tapered breath/noise modulation'}
                                if args.refine else None),
              'fixed_controls': ['pitch clock and flutter', 'formant target tables',
                                 'sound sequence, durations and pauses'],
              'normalization': 'EBU R128 integrated loudness, constant gain only',
              'measurements': measurements, 'listening_status': 'unreviewed experiment',
              'default_app_source_changed': False}
    (args.output / 'validation.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
