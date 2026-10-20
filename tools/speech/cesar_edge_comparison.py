#!/usr/bin/env python3
"""Isolate polynomial pulse closure and control-step changes, with frozen prosody.

Requires the native audio probe and ffmpeg. No LF source, neural inference,
output limiting, EQ, resampling or time stretch is used.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import time

from cesar_benchmark import floats, write_wav
from cesar_source_comparison import measure


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, default=root / 'build/cmake/bin/ortho-cesar-audio-probe')
    parser.add_argument('--output', type=Path, default=root / 'build/cesar-edge-comparison')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    suite_path = Path(__file__).parent / 'fixtures/compact-swedish.json'
    suite = json.loads(suite_path.read_text())
    variants = {'original': [], 'closure-only': ['--smooth-closure'],
                'controls-only': ['--smooth-controls'],
                'edges': ['--smooth-controls', '--smooth-closure']}
    report = {'source': 'original polynomial, all variants',
              'reference_roles': {'alice_bjorn': 'sound quality', 'espeak_sv': 'clarity and absence of clicks'},
              'suite_sha256': hashlib.sha256(suite_path.read_bytes()).hexdigest(),
              'changes': variants, 'cases': {}, 'listening_status': 'unreviewed edge experiment',
              'default_app_voice_changed': False}
    for case in suite['cases']:
        folder = args.output / case['id']
        folder.mkdir(exist_ok=True)
        ipa_path = folder / 'input.ipa.txt'
        ipa_path.write_text(case['ipa'])
        audio, timing, pitch, rows = {}, {}, {}, {}
        for name, switches in variants.items():
            prefix = folder / name
            started = time.perf_counter()
            subprocess.run([str(args.probe), '-', str(ipa_path), str(prefix.with_suffix('.raw.wav')),
                            '--ipa', '--compact', '--source', 'klatt', *switches,
                            '--trace', str(prefix), '--raw', str(prefix.with_suffix('.f32'))], check=True)
            wall = time.perf_counter() - started
            audio[name] = floats(prefix.with_suffix('.f32'))
            timing[name] = prefix.with_suffix('.timing.tsv').read_bytes()
            pitch[name] = prefix.with_suffix('.pitch.tsv').read_bytes()
            if (len(audio[name]) != len(audio['original']) or timing[name] != timing['original']
                    or pitch[name] != pitch['original']):
                raise ValueError(f'{case["id"]}: source processing changes duration or prosody')
            if any(abs(x) >= 0.89996 for x in audio[name]):
                raise ValueError('Source/filter output reaches its safety clamp')
            raw_lufs = measure(args.ffmpeg, prefix.with_suffix('.raw.wav'))
            rows[name] = {'raw_lufs': raw_lufs, 'raw_peak': max(map(abs, audio[name])),
                          'render_process_wall_seconds': wall,
                          'raw_sha256': hashlib.sha256(prefix.with_suffix('.f32').read_bytes()).hexdigest()}
        gains = {name: 10 ** ((-21 - row['raw_lufs']) / 20) for name, row in rows.items()}
        headroom = max(1, *(max(map(abs, values)) * gains[name] / 0.95
                           for name, values in audio.items()))
        levels = []
        for name, values in audio.items():
            output = folder / f'{name}.wav'
            gain = gains[name] / headroom
            write_wav(output, values, gain)
            level = measure(args.ffmpeg, output)
            levels.append(level)
            rows[name].update({'listening_lufs': level, 'gain': gain,
                              'listening_sha256': hashlib.sha256(output.read_bytes()).hexdigest()})
        if max(levels) - min(levels) > 0.2:
            raise ValueError('Listening variants are not level matched')
        with (folder / 'original.timing.tsv').open() as source:
            sounds = list(csv.DictReader(source, delimiter='\t'))
        report['cases'][case['id']] = {'text': case['text'], 'ipa': case['ipa'],
                                     'samples': len(audio['original']),
                                     'seconds': len(audio['original']) / 24000,
                                     'identical_pitch_duration_and_pause_traces': True,
                                     'vowel_intervals': [{'start': int(s['start_sample']), 'samples': int(s['samples'])}
                                                         for s in sounds if int(s['vowel'])],
                                     'variants': rows}
    (args.output / 'validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print('All four variants preserve every pitch control, sound duration and pause in all seven cases.')


if __name__ == '__main__':
    main()
