#!/usr/bin/env python3
"""MIT. Measure prototype assets and extrapolate voice assets to the fixed corpus.

Estimates exclude the application, corpus database, evaluation models and masters.
Alice is an explicit equal-density projection; no Alice compact bank exists yet.
"""
import argparse
import json
import sqlite3
import zlib
from pathlib import Path

from run import digest, dump


def compressed_bytes(path):
    compressor = zlib.compressobj(9)
    total = 0
    with path.open('rb') as stream:
        while block := stream.read(1024 * 1024):
            total += len(compressor.compress(block))
    return total + len(compressor.flush())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('corpus', 'master', 'candidate', 'prosody', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    con = sqlite3.connect(a.corpus.resolve().as_uri() + '?mode=ro', uri=True)
    texts = [r[0] for r in con.execute("SELECT v.text FROM verse v JOIN source s ON s.id=v.source_id WHERE s.code='sv1917'")]
    words = sum(len(t.split()) for t in texts)
    master = json.loads((a.master / 'master.json').read_text())['passages']
    selection = json.loads((a.candidate / 'selection.json').read_text())
    trial = json.loads((a.prosody / 'selection.json').read_text())['passages']
    source_words = sum(len(r['text'].split()) for r in selection['passages'])
    trial_words = sum(len(r['text'].split()) for r in trial)
    span_paths = [a.candidate / r['plan'] for r in selection['passages']]
    grain_paths = sorted(a.prosody.glob('*/*.grains'))
    if len(grain_paths) != len(trial):
        raise ValueError('Missing grain plans')
    bank = a.candidate / 'wave-bank.u8'
    measurements = {'bank_bytes': bank.stat().st_size, 'bank_zlib9_bytes': compressed_bytes(bank),
        'span_sample_passages': len(span_paths), 'span_sample_words': source_words,
        'span_sample_bytes': sum(p.stat().st_size for p in span_paths),
        'span_sample_zlib9_bytes': sum(compressed_bytes(p) for p in span_paths),
        'grain_sample_passages': len(grain_paths), 'grain_sample_words': trial_words,
        'grain_sample_bytes': sum(p.stat().st_size for p in grain_paths),
        'grain_sample_zlib9_bytes': sum(compressed_bytes(p) for p in grain_paths)}
    # Remove sample headers, scale records by corpus words, then restore one
    # header per verse. This assumes one prepared plan per corpus verse.
    spans = (measurements['span_sample_bytes'] - 8 * len(span_paths)) * words / source_words + 8 * len(texts)
    grains = (measurements['grain_sample_bytes'] - 16 * len(grain_paths)) * words / trial_words + 16 * len(texts)
    # Compression measured on multi-verse clips is optimistic for shorter plans.
    # Include a nominal 100-byte per-file/container allowance, then show a broad
    # planning range; this range is a sensitivity allowance, not a confidence CI.
    span_zip = measurements['span_sample_zlib9_bytes'] * words / source_words + 100 * len(texts)
    grain_zip = measurements['grain_sample_zlib9_bytes'] * words / trial_words + 100 * len(texts)
    installed = measurements['bank_bytes'] + spans + grains
    download = measurements['bank_zlib9_bytes'] + span_zip + grain_zip
    seconds = sum(r['seconds'] for r in master) * words / sum(len(r['text'].split()) for r in master)
    result = {'scope': __doc__, 'code_sha256': digest(Path(__file__)), 'corpus_sha256': digest(a.corpus),
        'corpus_verses': len(texts), 'corpus_whitespace_words': words, 'measurements': measurements,
        'estimated_audio_hours': seconds / 3600,
        'literal_prototype_projection': {'span_bytes_per_voice': round(spans), 'grain_bytes_per_voice': round(grains),
            'one_voice_installed_bytes': round(installed), 'one_voice_download_bytes': round(download),
            'two_voice_installed_bytes': round(2 * installed), 'two_voice_download_bytes': round(2 * download),
            'planning_range_multiplier': [.7, 1.3]},
        'unimplemented_parameter_controls_budget': {
            'scope': 'Engineering budget only. No format, renderer equivalence, audio quality or compression is validated.',
            'pitch_encoding_assumption': 'One log-F0/voicing byte per 20 ms; reconstruct grains at playback.',
            'pitch_bytes_per_voice': round(seconds * 50), 'word_timing_bytes_per_voice': 2 * words,
            'source_epoch_budget_bytes_per_voice': 2000000,
            'one_voice_installed_bytes': round(measurements['bank_bytes'] + spans + seconds * 50 + 2 * words + 2000000)},
        'limits': ['Three Björn passages supply grain density; all 40 development passages supply span density and duration.',
            'Extrapolation by whitespace word count; no full-corpus package was built.',
            'Alice density, pitch rate, bank size and quality are unmeasured; doubling is not an Alice measurement.',
            'Shorter per-verse streams may compress worse. Real archive overhead and pronunciation revisions vary.',
            'Better units may require a larger bank. These are asset estimates, not release-size or quality commitments.']}
    dump(a.output, result)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
