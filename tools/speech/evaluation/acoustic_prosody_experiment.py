#!/usr/bin/env python3
"""MIT. Test acoustically aligned FP32 melody; retain every v5 timing anchor.

Only offline pitch controls use the target master. Every output waveform sample
comes from the reusable compact bank. This fixed-corpus experiment requires a
prepared master for each text; it is not arbitrary-text synthesis or a release.
"""
import argparse
import json
from pathlib import Path

import numpy as np

from metrics import read_audio, write_audio
from phrase_experiment import phrase_marks, pitch_curve, verified_tracks
from prosody_experiment import analyze, make_grains, render, save_plan, verify_native
from run import digest, dump


def acoustic_map(alignment):
    """Coalesce repeated source positions into a monotone source-to-master map."""
    points = np.asarray(alignment, dtype=float)
    if points.ndim != 2 or points.shape[1] != 2 or len(points) < 2 or not np.isfinite(points).all():
        raise ValueError('Invalid acoustic alignment')
    if np.any(np.diff(points, axis=0) < 0):
        raise ValueError('Non-monotone acoustic alignment')
    source = np.unique(points[:, 1])
    reference = np.array([points[points[:, 1] == value, 0].mean() for value in source])
    if len(source) < 2 or np.any(np.diff(reference) <= 0):
        raise ValueError('Degenerate acoustic alignment')
    return np.c_[reference, source] * 24000


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('candidate', 'word-controls', 'phrase-controls', 'comparison', 'master', 'output', 'renderer', 'reaper'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--ids', nargs='+', required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    sources = {r['id']: r for r in json.loads((a.candidate / 'selection.json').read_text())['passages']}
    timings = {r['id']: r for r in json.loads((a.phrase_controls / 'report.json').read_text())['passages']}
    master = json.loads((a.master / 'master.json').read_text())
    if 'unquantized FP32' not in master['provenance']['kind']:
        raise ValueError('Requires original FP32 master')
    refs = {r['id']: r for r in master['passages']}
    comparisons = {r['id']: r for r in json.loads(a.comparison.read_text())['rows'] if r['condition'] == 'compact-v2'}
    report = {'scope': __doc__, 'code_sha256': digest(Path(__file__)),
              'comparison_sha256': digest(a.comparison), 'master_manifest_sha256': digest(a.master / 'master.json'),
              'timing_report_sha256': digest(a.phrase_controls / 'report.json'), 'passages': []}
    selection = []
    for id in a.ids:
        row, old, ref, pair = sources[id], timings[id], refs[id], comparisons[id]
        source = a.candidate / row['audio']
        reference = a.master / ref['audio']
        if digest(source) != row['audio_sha256'] or row['audio_sha256'] != old['source_sha256']:
            raise ValueError('Changed compact input')
        if digest(reference) != ref['audio_sha256'] or pair['reference_sha256'] != ref['audio_sha256']:
            raise ValueError('Changed FP32 master')
        if pair['audio_sha256'] != row['audio_sha256'] or pair['comparison']['status'] != 'ok':
            raise ValueError('Stale or failed source alignment')
        folder = a.output / id
        folder.mkdir(exist_ok=True)
        source_pm, _ = verified_tracks(a.word_controls / id, 'source', row['audio_sha256'])
        _, master_f0 = analyze(a.reaper, reference, folder / 'master')
        points = np.array(old['timing_anchors'])
        mapping = acoustic_map(pair['comparison']['alignment_seconds'])
        length = int(round(points[-1, 0]))
        pcm, rate = read_audio(source)
        if rate != 24000:
            raise ValueError('Expected 24 kHz compact audio')
        marks, positions, audit = phrase_marks(source_pm, points, mapping, length, pitch_curve(master_f0))
        grains = make_grains(pcm, source_pm, marks, positions)
        wave, norm = render(pcm, length, grains)
        if np.any(norm <= 1e-8):
            raise ValueError('Uncovered output samples')
        name = 'acoustic-melody'
        write_audio(folder / (name + '.wav'), wave, rate)
        save_plan(folder / (name + '.grains'), length, grains)
        native, stats, difference = verify_native(a.renderer, source, folder, name)
        selection.append({'id': id + '-' + name, 'original_id': id, 'split': 'dev', 'text': row['text'],
                          'audio': str(native.relative_to(a.output)), 'audio_sha256': digest(native)})
        report['passages'].append({'id': id, 'source_sha256': row['audio_sha256'],
            'master_audio_sha256': ref['audio_sha256'], 'timing_anchors': old['timing_anchors'],
            'timing_unchanged_from': 'compact-v5', 'acoustic_mapping': mapping.tolist(), 'pitch_control': audit,
            'seconds': length / rate, 'uncovered_samples': 0, 'native_stats': stats,
            'native_maximum_pcm_unit_difference': difference, 'audio_sha256': digest(native)})
        dump(a.output / 'report.json', report)
        dump(a.output / 'selection.json', {'scope': __doc__, 'passages': selection})
        print(json.dumps({'id': id, 'pitch_control': audit, 'native_stats': stats}), flush=True)


if __name__ == '__main__':
    main()
