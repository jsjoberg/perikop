#!/usr/bin/env python3
"""Run an external Flite/Clustergen voice without changing the application.

Swedish runs use the voice's own text frontend, not the project's curated IPA.
The separate English loader control checks local runtime readiness only.
Requires a locally supplied .flitevox model; does not download recordings,
request credentials, train a voice or grant redistribution rights.
"""
import argparse
import array
import hashlib
import json
from pathlib import Path
import resource
import statistics
import subprocess
import sys
import time
import wave


def run(command):
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'{command[0]} failed: {result.stderr.strip()}')
    return result.stdout


def inspect_wave(path):
    with wave.open(str(path), 'rb') as source:
        if source.getnchannels() != 1 or source.getsampwidth() != 2:
            raise ValueError('Expected mono PCM16 from the native voice')
        rate = source.getframerate()
        samples = array.array('h', source.readframes(source.getnframes()))
    if sys.byteorder != 'little':
        samples.byteswap()
    if not samples or not any(samples):
        raise ValueError('Empty or silent synthesis')
    return {'rate': rate, 'samples': len(samples), 'seconds': len(samples) / rate,
            'peak': max(map(abs, samples)) / 32768,
            'rail_samples': sum(abs(x) >= 32767 for x in samples),
            'wav_sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', required=True, type=Path)
    parser.add_argument('--voice-id', required=True,
                        choices=['SWESFB', 'SWESFV', 'english-loader-control'])
    parser.add_argument('--model-source', required=True)
    parser.add_argument('--flite-dir', type=Path,
                        default=root / 'build/clustergen-swedish-research/flite')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    model = args.model.resolve(strict=True)
    flite = args.flite_dir.resolve() / 'bin/flite'
    info = args.flite_dir.resolve() / 'bin/flitevox_info'
    folder = (args.output or root / 'build/clustergen-swedish-research' / args.voice_id).resolve()
    folder.mkdir(parents=True, exist_ok=True)
    language = run([str(info), '-voice', str(model), '-get', 'language']).strip()
    description = run([str(info), '-voice', str(model), '-get', 'name']).strip()
    control = args.voice_id == 'english-loader-control'
    if not control and language != 'language "cmu_grapheme_lang"':
        raise ValueError(f'Expected Wilderness grapheme voice, got {language}')
    if control:
        cases = [{'id': 'loader-control',
                  'text': 'This is an English model loader check. It does not evaluate Swedish.'}]
    else:
        suite = json.loads((Path(__file__).parent / 'fixtures/compact-swedish.json').read_text())
        # Explicit project phone overrides require an adapter before they can be tested.
        cases = [c for c in suite['cases'] if c['id'] != 'forced-phone']
    report = {'voice_id': args.voice_id, 'model_source': args.model_source,
              'model_sha256': hashlib.sha256(model.read_bytes()).hexdigest(),
              'model_bytes': model.stat().st_size, 'language': language,
              'name': description, 'frontend': 'upstream text frontend',
              'project_ipa_used': False, 'swedish_evaluation': not control,
              'listening_status': 'not applicable, English runtime control' if control else 'unreviewed',
              'redistribution_terms': 'not established by this trial',
              'timing_scope': 'process startup, model loading, text frontend, synthesis and WAV writing',
              'processing': 'native-rate raw output; no gain, EQ, limiter or time stretch',
              'default_app_voice_changed': False, 'cases': {}}
    for case in cases:
        case_dir = folder / case['id']
        case_dir.mkdir(exist_ok=True)
        input_path = case_dir / 'input.txt'
        input_path.write_text(case['text'])
        wav_path = case_dir / 'output.wav'
        command = [str(flite), '-voice', str(model), '-psdur', '-f', str(input_path), str(wav_path)]
        run(command)  # warm filesystem cache and exercise the loader before measurements
        measurements = []
        for _ in range(3):
            before = resource.getrusage(resource.RUSAGE_CHILDREN)
            started = time.perf_counter()
            segments = run(command)
            elapsed = time.perf_counter() - started
            after = resource.getrusage(resource.RUSAGE_CHILDREN)
            cpu = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
            measurements.append({'elapsed_seconds': elapsed, 'cpu_seconds': cpu})
        (case_dir / 'segments.txt').write_text(segments)
        audio = inspect_wave(wav_path)
        report['cases'][case['id']] = {'text': case['text'], 'audio': audio,
                                     'runs': measurements,
                                     'median_elapsed_seconds': statistics.median(
                                         r['elapsed_seconds'] for r in measurements),
                                     'median_cpu_seconds': statistics.median(
                                         r['cpu_seconds'] for r in measurements)}
    (folder / 'validation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(f'{args.voice_id}: {len(cases)} cases; report at {folder / "validation.json"}')


if __name__ == '__main__':
    main()
