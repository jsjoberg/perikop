#!/usr/bin/env python3
"""Inspect sensitivity to actual audio controls; no synthetic transcript scoring (MIT)."""
import argparse
import json
from pathlib import Path
import statistics

from metrics import paired_delta


def validate(report):
    rows=report['rows']
    neural={r['id']:r for r in rows if r['voice']=='neural'}
    result={'reference_gate':report['summary']['reference_gate'],'controls':{},
            'human_evidence':'PR #32 listening review: Bas largely incomprehensible; names relatively best. No numeric human transcriptions available.',
            'interpretation':'Sensitivity tests, not an overall score. An ASR miss on an altered signal is explicitly reported.'}
    probes={'silence':'silence_fraction','clipping':'clipped_fraction',
            'misplaced_silence':'max_internal_silence_seconds','hard_joins':'wave_jump_candidates_per_second',
            'wrong_timing':'pitch_jump_candidates_per_second','subword_noise':'spectral_jump_candidates_per_second',
            'destructive_overlap':'duration_seconds'}
    for control,metric in probes.items():
        candidate={r['id']:r for r in rows if r['voice']=='control-'+control}
        reference={k:neural[k] for k in candidate if k in neural}
        if not candidate or set(reference)!=set(candidate):
            result['controls'][control]={'status':'missing_or_incomplete'}
            continue
        wer,cer=paired_delta(reference,candidate),paired_delta(reference,candidate,'cer')
        differences=[candidate[k]['acoustic'][metric]-reference[k]['acoustic'][metric] for k in candidate]
        result['controls'][control]={'passages':len(candidate),'wer':wer,'cer':cer,
            'asr_response':'increase_detected' if wer['ci95'][0]>.02 and cer['ci95'][0]>=0 else 'uncertain_or_insensitive',
            'acoustic_metric':metric,'median_paired_acoustic_delta':statistics.median(differences),
            'note':'Misplaced pauses need not change words. Timing/noise/overlap controls have multiple acoustic effects.'}
    candidate={r['id']:r for r in rows if r['voice']=='compact'}
    reference={k:neural[k] for k in candidate if k in neural}
    if candidate and set(candidate)==set(reference):
        w=paired_delta(reference,candidate)
        result['neural_vs_bas']={'wer':w,'separates_reported_failed_voice':w['ci95'][0]>.02,
            'caution':'This agrees/disagrees with one aggregate listener report, not a calibrated relation to human intelligibility.'}
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():
        raise ValueError('Validation output exists; choose a new path')
    args.output.write_text(json.dumps(validate(json.loads(args.report.read_text())),ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':
    main()
