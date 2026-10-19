#!/usr/bin/env python3
"""Summarize native speech-latency JSONL without ML/Python dependencies."""
import argparse
import hashlib
import json
from pathlib import Path


def summarize(path):
    records=[json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    configuration=next(x for x in records if x['event']=='configuration')
    rows=[]
    for case in [1,2,3]:
        events=[x for x in records if x.get('scenario')==case]
        by={x['event']:x for x in events}
        if 'completed' not in by:
            raise ValueError(f'Incomplete scenario {case}: {path}')
        def elapsed(start,end):
            return by[end]['seconds']-by[start]['seconds'] if start in by and end in by else None
        rows.append({'scenario':case,'name':configuration[f'case_{case}'],
            'model_load_seconds':elapsed('model_load_begin','model_load_end'),
            'generate_seconds':elapsed('generate_begin','generate_end'),
            'cache_save_seconds':elapsed('cache_save_begin','cache_save_end'),
            'first_append_seconds':by.get('append',{}).get('seconds'),
            'first_gate_release_seconds':by.get('gate_released',{}).get('seconds'),
            'first_nonzero_seconds':by.get('first_nonzero_gt_1e-5',{}).get('seconds'),
            'first_active_rms_seconds':by.get('first_frame_rms_gt_minus50dbfs',{}).get('seconds'),
            'note':'Signal thresholds in a simulated 24 kHz clock; excludes physical device, driver and UI delays.'})
    return {'log_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'configuration':configuration,'rows':rows}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('logs',type=Path,nargs='+')
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    if args.output.exists():raise ValueError('Choose a new output file')
    args.output.write_text(json.dumps({'runs':[summarize(p) for p in args.logs]},indent=2)+'\n')


if __name__=='__main__':main()
