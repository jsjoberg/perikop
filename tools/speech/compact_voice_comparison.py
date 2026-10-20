#!/usr/bin/env python3
"""CPU-only Swedish acoustic comparison, bypassing alternate text phonemizers.

Requires numpy, scipy, soundfile, onnxruntime, ffmpeg, a C++23 compiler, the
existing Kokoro pack, and a separately downloaded Swedish Piper NST model.
No model downloads, product integration or eSpeak runtime dependency.
"""
import argparse
import ctypes
import hashlib
import html
import json
import math
from pathlib import Path
import platform
import resource
import re
import statistics
import subprocess
import sys
import time

from cesar_source_comparison import measure


def peak_mib():
    value = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return value / (1024 ** 2 if sys.platform == 'darwin' else 1024)


def worker(args, root, cases):
    import numpy as np
    import onnxruntime as ort
    import soundfile as sf
    from scipy.signal import filtfilt, iirnotch

    baseline_memory = peak_mib()
    started, cpu_start = time.perf_counter(), time.process_time()
    options = ort.SessionOptions()
    options.intra_op_num_threads = args.threads
    options.inter_op_num_threads = 1
    options.add_session_config_entry('session.force_spinning_stop', '1')
    options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    audit = {}
    if args.engine == 'cesar':
        library = ctypes.CDLL(str(args.output / 'cesar.dylib'))
        library.compact_cesar.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_size_t)]
        library.compact_cesar.restype = ctypes.POINTER(ctypes.c_float)
        library.compact_cesar_free.argtypes = [ctypes.POINTER(ctypes.c_float)]
        rate = 24000

        def generate(case):
            count = ctypes.c_size_t()
            pointer = library.compact_cesar(case['ipa'].encode(), ctypes.byref(count))
            if not pointer:
                raise ValueError('Native Cesar failed')
            try:
                return np.ctypeslib.as_array(pointer, shape=(count.value,)).copy()
            finally:
                library.compact_cesar_free(pointer)
    elif args.engine.startswith('piper'):
        config = json.loads(args.piper.with_suffix('.onnx.json').read_text())
        rate = config['audio']['sample_rate']
        id_map = config['phoneme_id_map']
        session = ort.InferenceSession(str(args.piper), options, providers=['CPUExecutionProvider'])
        # Explicit IPA ligature expansion; never drop an unknown phone or substitute
        # an English vowel. Vocabulary coverage still does not prove realization.
        def encode(ipa):
            expanded = ipa.replace('ʧ', 'tʃ')
            missing = sorted(set(expanded) - id_map.keys())
            if missing:
                raise ValueError(f'Piper has no IDs for: {missing}')
            ids = list(id_map['^'])
            for phone in expanded:
                ids.extend(id_map[phone])
                ids.extend(id_map['_'])
            return ids + id_map['$']

        for case in cases:
            encode(case['ipa'])
        symbols = set()
        for filename in ['lexicon.tsv', 'custom_lexicon.tsv', 'g2p-corpus.tsv']:
            for line in (args.pack / filename).read_text().splitlines():
                if not line.startswith('#') and '\t' in line:
                    symbols.update(line.split('\t', 1)[1].replace(' ', ''))
        audit = {'missing_lexicon_symbols_before_expansion': sorted(symbols - id_map.keys()),
                 'explicit_expansion': {'ʧ': 'tʃ'},
                 'missing_after_expansion': sorted(set(''.join(symbols).replace('ʧ', 'tʃ')) - id_map.keys()),
                 'dropped_symbols': [], 'phonemizer': 'our frozen KokoroText::ipa; no eSpeak',
                 'warning': 'Matching IDs does not establish trained acoustic realization or lexical accents'}
        if audit['missing_after_expansion']:
            raise ValueError('Incomplete lexicon coverage')

        def generate(case):
            pieces = (re.findall(r'[^,;.!?]+[,;.!?]?', case['ipa'])
                      if args.engine == 'piper-clauses' else [case['ipa']])
            if ''.join(pieces) != case['ipa']:
                raise ValueError('Clause segmentation loses phonetic input')
            output = []
            for piece in pieces:
                ids = np.asarray([encode(piece)], dtype=np.int64)
                output.append(session.run(None, {'input': ids,
                                                'input_lengths': np.asarray([ids.shape[1]], dtype=np.int64),
                                                'scales': np.asarray([0.667, 1.0, 0.8], dtype=np.float32)})[0].ravel())
            return np.concatenate(output)
    else:
        rate = 24000
        session = ort.InferenceSession(str(args.pack / 'kokoro.onnx'), options,
                                       providers=['CPUExecutionProvider'])
        styles = np.fromfile(args.pack / 'bjorn.bin', dtype='<f4').reshape(510, 256)

        def generate(case):
            phones = case['kokoro_tokens']
            if not 0 < len(phones) <= 510:
                raise ValueError('Kokoro context exceeded')
            audio = session.run(['audio'], {'input_ids': np.asarray([[0, *phones, 0]], dtype=np.int64),
                                            'ref_s': styles[len(phones) - 1:len(phones)]})[0].ravel()
            # Match the native Bjorn decoder cleanup (zero-phase notches, pad=9).
            for frequency in [2400, 4800, 7200, 9600]:
                b, a = iirnotch(frequency, 35, fs=rate)
                audio = filtfilt(b, a, audio, padlen=9)
            return audio.astype(np.float32)

    result = {'engine': args.engine, 'threads': args.threads, 'onnxruntime': ort.__version__,
              'platform': platform.platform(), 'machine': platform.machine(),
              'load_wall_seconds': time.perf_counter() - started,
              'load_cpu_seconds': time.process_time() - cpu_start,
              'baseline_process_peak_mib': baseline_memory, 'phone_audit': audit, 'cases': {}}
    result['suite_sha256'] = hashlib.sha256(args.suite.read_bytes()).hexdigest()
    for case in cases:
        # One untimed warm-up per shape. Three measured runs; take medians.
        generate(case)
        walls, cpus, durations = [], [], []
        for _ in range(3):
            start, cpu = time.perf_counter(), time.process_time()
            audio = generate(case)
            walls.append(time.perf_counter() - start)
            cpus.append(time.process_time() - cpu)
            if not len(audio) or not np.isfinite(audio).all():
                raise ValueError('Invalid acoustic output')
            durations.append(len(audio) / rate)
        prefix = args.output / f'{args.engine}-{args.threads}' / case['id']
        prefix.parent.mkdir(parents=True, exist_ok=True)
        sf.write(prefix.with_suffix('.raw.wav'), audio, rate, subtype='FLOAT')
        lufs = measure(args.ffmpeg, prefix.with_suffix('.raw.wav'))
        gain = 10 ** ((-21 - lufs) / 20)
        gain = min(gain, 0.95 / float(np.max(np.abs(audio))))
        sf.write(prefix.with_suffix('.wav'), audio * gain, rate, subtype='PCM_16')
        matched = measure(args.ffmpeg, prefix.with_suffix('.wav'))
        row = {'audio_seconds': statistics.median(durations), 'sample_rate': rate,
               'wall_seconds': statistics.median(walls), 'cpu_seconds': statistics.median(cpus),
               'wall_runs': walls, 'cpu_runs': cpus, 'duration_runs': durations,
               'wall_rtf': statistics.median(w / d for w, d in zip(walls, durations)),
               'cpu_rtf': statistics.median(c / d for c, d in zip(cpus, durations)),
               'raw_lufs': lufs, 'lufs': matched, 'peak': float(np.max(np.abs(audio))) * gain,
               'listening_sha256': hashlib.sha256(prefix.with_suffix('.wav').read_bytes()).hexdigest()}
        result['cases'][case['id']] = row
        print(args.engine, args.threads, case['id'], 'wall RTF', round(row['wall_rtf'], 4), flush=True)
    result['process_peak_mib'] = peak_mib()
    result['peak_above_import_baseline_mib'] = peak_mib() - baseline_memory
    (args.output / f'{args.engine}-{args.threads}.json').write_text(json.dumps(result, indent=2) + '\n')


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=root / 'build/compact-voice-comparison')
    parser.add_argument('--pack', type=Path, default=root / 'build/kokoro-pack')
    parser.add_argument('--piper', type=Path,
                        default=root / 'build/compact-voice-comparison/models/nst/sv_SE-nst-medium.onnx')
    parser.add_argument('--suite', type=Path, default=Path(__file__).parent / 'fixtures/compact-swedish.json')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--engine', choices=['all', 'cesar', 'piper', 'piper-clauses', 'bjorn'], default='all')
    parser.add_argument('--threads', type=int, choices=[1, 4], default=1)
    parser.add_argument('--normalize-only', action='store_true', help='Re-level existing completed runs')
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    suite = json.loads(args.suite.read_text())
    if json.loads((args.pack / 'voice-pack.json').read_text())['id'] != suite['pack_id']:
        raise ValueError('Pronunciation fixture and voice pack do not match')
    if args.normalize_only:
        normalize(args, suite)
        return
    if args.engine != 'all':
        worker(args, root, suite['cases'])
        return
    provenance = args.piper.parent / 'provenance.json'
    if not provenance.is_file():
        raise ValueError('Pinned model provenance.json is required beside the Piper model')
    manifest = json.loads(provenance.read_text())
    for filename, expected in manifest['files'].items():
        with (args.piper.parent / filename).open('rb') as source:
            digest = hashlib.file_digest(source, 'sha256').hexdigest()
        if digest != expected['sha256']:
            raise ValueError(f'Model provenance mismatch: {filename}')
    subprocess.run(['c++', '-std=c++23', '-Os', '-shared', '-fPIC', f'-I{root / "src"}',
                    str(root / 'tools/speech/cesar_benchmark_bridge.cpp'),
                    *[str(root / 'src/speech' / f'{x}.cpp')
                      for x in ['cesar_voice', 'klatt', 'swedish_prosody', 'lf_source']],
                    '-o', str(args.output / 'cesar.dylib')], check=True)
    # Separate processes prevent earlier model allocations contaminating later peaks.
    settings = [('cesar', 1), ('piper', 1), ('bjorn', 1), ('piper', 4), ('bjorn', 4), ('piper-clauses', 1)]
    for engine, threads in settings:
        subprocess.run([sys.executable, __file__, '--engine', engine, '--threads', str(threads),
                        '--output', str(args.output), '--pack', str(args.pack), '--piper', str(args.piper),
                        '--suite', str(args.suite), '--ffmpeg', args.ffmpeg], check=True)
    normalize(args, suite)


def normalize(args, suite):
    # A model can have a large transient that prevents -21 LUFS without clipping.
    # Lower ALL renditions of that case together; never limit just one model.
    import numpy as np
    import soundfile as sf
    settings = [('cesar', 1), ('piper', 1), ('bjorn', 1), ('piper', 4), ('bjorn', 4), ('piper-clauses', 1)]
    reports = {f'{engine}-{threads}': json.loads((args.output / f'{engine}-{threads}.json').read_text())
               for engine, threads in settings}
    for case in suite['cases']:
        material, ceiling = {}, -21
        for name, report in reports.items():
            prefix = args.output / name / case['id']
            audio, rate = sf.read(prefix.with_suffix('.raw.wav'))
            raw_lufs = measure(args.ffmpeg, prefix.with_suffix('.raw.wav'))
            peak = float(np.max(np.abs(audio)))
            ceiling = min(ceiling, raw_lufs + 20 * math.log10(0.95 / peak))
            material[name] = audio, rate, raw_lufs
        for name, (audio, rate, raw_lufs) in material.items():
            path = args.output / name / f'{case["id"]}.wav'
            gain = 10 ** ((ceiling - raw_lufs) / 20)
            sf.write(path, audio * gain, rate, subtype='PCM_16')
            actual = measure(args.ffmpeg, path)
            if abs(actual - ceiling) > 0.2:
                raise ValueError('Common loudness matching failed')
            reports[name]['cases'][case['id']].update(
                {'raw_lufs': raw_lufs, 'lufs': actual, 'constant_gain': gain,
                 'peak': float(np.max(np.abs(audio))) * gain,
                 'listening_sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    for name, report in reports.items():
        (args.output / f'{name}.json').write_text(json.dumps(report, indent=2) + '\n')
    page = ['<!doctype html><meta charset="utf-8"><title>Swedish voice comparison</title>',
            '<style>body{font:17px system-ui;max-width:1100px;margin:2rem auto;padding:1rem}',
            'table{width:100%}td{padding:1rem}audio{width:100%;min-width:200px}',
            'section{margin:3rem 0}p{line-height:1.5}</style>',
            '<h1>Compact Swedish voices</h1><p>Same pronunciation data; matched loudness per case.',
            ' Natural durations are retained. Swedish quality awaits native listening review.</p>']
    names = ['cesar-1', 'piper-1', 'bjorn-1']
    for case in suite['cases']:
        page.extend([f'<section><h2>{html.escape(case["id"])}</h2>',
                     f'<p>{html.escape(case["text"])}</p><p>{html.escape(case["review_focus"])}</p>',
                     '<details><summary>Our IPA</summary>', f'<p>{html.escape(case["ipa"])}</p></details>',
                     '<div style="overflow:auto"><table><tr>',
                     *[f'<th>{name}</th>' for name in names], '</tr><tr>'])
        for name in names:
            row = reports[name]['cases'][case['id']]
            page.append(f'<td><audio controls preload="none" src="{name}/{case["id"]}.wav"></audio>'
                        f'<p>{row["audio_seconds"]:.2f} s · {row["lufs"]:.1f} LUFS</p></td>')
        page.append('</tr></table></div><details><summary>Other CPU/memory settings</summary>')
        for name in ['piper-4', 'bjorn-4', 'piper-clauses-1']:
            page.append(f'<p>{name}</p><audio controls preload="none" src="{name}/{case["id"]}.wav"></audio>')
        page.append('</details></section>')
    page.append("<script>document.addEventListener('play',e=>{if(e.target.tagName==='AUDIO')"
                "document.querySelectorAll('audio').forEach(a=>{if(a!==e.target)a.pause()})},true)</script>")
    (args.output / 'index.html').write_text(''.join(page))


if __name__ == '__main__':
    main()
