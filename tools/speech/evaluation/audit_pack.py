#!/usr/bin/env python3
"""Read-only unit-bank audit. Stored spans are not verified phoneme durations (MIT)."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np

from metrics import describe


def audit(folder):
    manifest = json.loads((folder/'voice-pack.json').read_text())
    checks = {name:hashlib.sha256((folder/name).read_bytes()).hexdigest()==info['sha256']
              for name,info in manifest['files'].items()}
    if not all(checks.values()):
        raise ValueError('Pack hash mismatch')
    bank = (folder/'units.bin').read_bytes()
    magic,rate,phone_count,count = struct.unpack_from('<4s3I',bank)
    if magic!=b'BJU1' or rate!=24000:
        raise ValueError('Unsupported bank')
    labels = {}
    for line in (folder/'phones.tsv').read_text().splitlines():
        index,label,duration = line.split('\t')
        labels[int(index)] = (label,int(duration)/rate)
    offset,units = 16,[]
    for _ in range(count):
        left,right,size,lead,tail = struct.unpack_from('<HHIHH',bank,offset)
        offset += 12
        if size<=lead+tail or offset+size>len(bank) or right not in labels or (left!=65535 and left not in labels):
            raise ValueError('Invalid unit')
        units.append({'left':'*' if left==65535 else labels[left][0], 'right':labels[right][0],
                      'core_seconds':(size-lead-tail)/rate,'guard_seconds':(lead+tail)/rate,
                      'rail_code_fraction':float(np.mean(np.isin(np.frombuffer(bank[offset:offset+size],dtype='u1'),[0,255])))})
        offset += size
    if offset!=len(bank) or len(labels)!=phone_count:
        raise ValueError('Invalid bank length/count')
    keys = [(u['left'],u['right']) for u in units]
    return {'revision':manifest['revision'],'file_checks':checks,'phone_count':phone_count,'unit_count':count,
            'monophones':sum(u['left']=='*' for u in units),'duplicate_keys':len(keys)-len(set(keys)),
            'stored_core_seconds':describe([u['core_seconds'] for u in units]),
            'stored_phone_median_seconds':describe([v[1] for v in labels.values()]),
            'teacher_batches':len(manifest.get('training',[])),
            'batch_gain':describe([r['normalization_gain'] for r in manifest.get('training',[])]),
            'alignment_scale_samples':describe([r['duration_scale_samples'] for r in manifest.get('training',[])]),
            'units':units,'interpretation':'Span lengths and mu-law rail codes; neither proves acoustic phone identity or clipping.',
            'provenance_gap':'Bank does not record the winning source batch/span per unit; training batches alone cannot trace each cut.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(audit(args.pack),ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':
    main()
