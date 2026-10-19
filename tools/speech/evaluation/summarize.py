#!/usr/bin/env python3
"""Create concise, linked Markdown tables from an evaluation JSON report (MIT)."""
import argparse
import json
from pathlib import Path
import statistics


def pct(value):
    return f'{100*value:.1f}%'


def summary(report):
    s = report['summary']
    lines = ['# Björn evaluation measurements','',f"Run status: **{s['status']}**. No combined quality score.",'',
             '| Voice | Split | Passages | WER | CER | Empty ASR |',
             '|---|---|---:|---:|---:|---:|']
    for voice,groups in s['groups'].items():
        for split,group in groups.items():
            if group['n']:
                lines.append(f"| {voice} | {split} | {group['n']} | {pct(group['wer'])} | {pct(group['cer'])} | {group['empty_transcripts']} |")
    lines += ['', 'Reference gates: '+', '.join(f"{k}: **{v['status']}**" for k,v in s['reference_gate'].items())+'.','',
              '## Differences from neural reference','',
              'Delta is candidate minus reference; negative WER/CER differences favor candidate.', '',
              '| Candidate | Matched passages | WER delta | WER 95% interval | CER delta |',
              '|---|---:|---:|---:|---:|']
    for voice,c in s['comparisons_to_reference'].items():
        w=c['wer'];lo,hi=w['ci95']
        lines.append(f"| {voice} | {w['n']} | {100*w['delta']:+.1f} pp | [{100*lo:+.1f}, {100*hi:+.1f}] pp | {100*c['cer']['delta']:+.1f} pp |")
    lines += ['', '## Acoustic profile','',
              'Medians across passages. Jump counts are candidates, including legitimate speech transitions.','',
              '| Voice | Duration s | Silence | Voiced frames | Pitch jumps/s | Spectral jumps/s | Wave jumps/s |',
              '|---|---:|---:|---:|---:|---:|---:|']
    for voice in s['groups']:
        rows = [r for r in report['rows'] if r['voice']==voice]
        a={key:statistics.median(r['acoustic'][key] for r in rows) for key in
           ['duration_seconds','silence_fraction','voiced_fraction','pitch_jump_candidates_per_second',
            'spectral_jump_candidates_per_second','wave_jump_candidates_per_second']}
        lines.append(f"| {voice} | {a['duration_seconds']:.2f} | {pct(a['silence_fraction'])} | {pct(a['voiced_fraction'])} | {a['pitch_jump_candidates_per_second']:.2f} | {a['spectral_jump_candidates_per_second']:.2f} | {a['wave_jump_candidates_per_second']:.2f} |")
    lines += ['', '## Compact passages to inspect','',
              '| Passage | Split | WER | CER | Audio | Recognized |','|---|---|---:|---:|---|---|']
    rows=sorted([r for r in report['rows'] if r['voice']=='compact'],key=lambda r:r['intelligibility']['wer'],reverse=True)
    for row in rows:
        i=row['intelligibility']
        recognized=row['asr']['text'].replace('|','\\|').replace('\n',' ')
        lines.append(f"| {row['reference']} | {row['split']} | {pct(i['wer'])} | {pct(i['cer'])} | [WAV]({row['audio']}) | {recognized} |")
    lines += ['', '## Limitations','']+[f'- {x}' for x in s['limitations']]
    if s['technical_failures']:
        lines += ['', 'Technical failures:', '', '```json',json.dumps(s['technical_failures'],indent=2),'```']
    return '\n'.join(lines)+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():
        raise ValueError('Summary exists; choose a new output')
    args.output.write_text(summary(json.loads(args.report.read_text())))


if __name__=='__main__':
    main()
