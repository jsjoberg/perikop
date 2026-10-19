#!/usr/bin/env python3
"""Locate teacher timing bias using source acoustics, without benchmark audio (MIT).

This is a broad-class diagnostic, not an independent forced phoneme alignment.
Fricatives should have much more high-frequency energy than vowels. A consistent
offset at which their separation improves identifies a timing problem to test.
"""
import argparse
import json
from pathlib import Path
from types import SimpleNamespace

import numpy as np

from selection_experiment import load_sources
from run import dump,digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-manifest',type=Path,required=True)
    p.add_argument('--neural-pack',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    args.legacy_sources=None
    args.alignment_offset=0
    sources=load_sources(args)
    # Three consecutive groups fixed by the saved source manifest, never ASR.
    groups=np.array_split(np.arange(len(sources)),3)
    results=[]
    for group in groups:
        records=[]
        for index in group:
            source=sources[index]
            for label,a,b in source['phones']:
                label=label.strip('ˈˌː')
                if label in 'sʂɕaɑeɛəiɪoɔuʊyøœɨɜ':
                    records.append((source['pcm'],(a+b)//2,label in 'sʂɕ'))
        curve=[]
        for shift in range(-2400,2401,120):
            values=[[],[]]
            for pcm,center,fricative in records:
                center+=shift
                if center<256 or center+256>=len(pcm):continue
                power=np.abs(np.fft.rfft(pcm[center-256:center+256]*np.hanning(512)))**2
                feature=np.log((power[70:215].sum()+1e-9)/(power[2:65].sum()+1e-9))
                values[int(fricative)].append(float(feature))
            curve.append({'offset_samples':shift,'offset_ms':shift/24,
                          'fricative_minus_vowel_log_energy_ratio':float(np.mean(values[1])-np.mean(values[0])),
                          'fricatives':len(values[1]),'vowels':len(values[0])})
        results.append({'source_indices':[int(i) for i in group], 'curve':curve,
                        'maximum':max(curve,key=lambda r:r['fricative_minus_vowel_log_energy_ratio'])})
    dump(args.output,{'scope':'Source-only acoustic timing diagnostic; no benchmark recognition used',
                      'source_manifest_sha256':digest(args.source_manifest),'code_sha256':digest(Path(__file__)),
                      'groups':results})
    print(json.dumps([g['maximum'] for g in results]))


if __name__=='__main__':main()
