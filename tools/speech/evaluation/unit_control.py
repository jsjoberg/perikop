#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy==2.3.5"]
# ///
"""Diagnostic: identical native renderer with shipped vs sentence-specific units (MIT).

Sentence-specific output is an oracle reconstruction control, not a reusable voice
or evidence of generalization. Production source and voice resources are untouched.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import sys

import numpy as np

from metrics import acoustic, read_audio, score, write_audio
from run import digest, dump, execute, identity, recognize

ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('prepare_voice',ROOT/'tools/speech/bjorn-poc/prepare_voice.py')
prep=importlib.util.module_from_spec(spec)
spec.loader.exec_module(prep)
SAMPLES=[('genesis','I begynnelsen skapade Gud himmel och jord.'),
         ('angar','Han låter mig vila på gröna ängar.'),
         ('sjal','Han vederkvicker min själ.')]


def bank_units(path):
    bank=path.read_bytes()
    magic,rate,count,n=struct.unpack_from('<4s3I',bank)
    assert magic==b'BJU1' and rate==24000
    units={};at=16
    for _ in range(n):
        left,right,length,lead,tail=struct.unpack_from('<HHIHH',bank,at)
        at+=12
        units[(left,right)]=(bank[at:at+length],lead,tail)
        at+=length
    assert at==len(bank)
    return count,units


def replace_units(source, destination, pcm, aligned):
    count,units=bank_units(source/'units.bin')
    labels={line.split('\t')[1]:int(line.split('\t')[0]) for line in (source/'phones.tsv').read_text().splitlines()}
    selected={};scores={};traces={}
    def add(key,first,last,target):
        begin,end=max(0,first-240),min(len(pcm),last+240)
        quality=-abs((last-first)/24000-target)
        if quality>scores.get(key,-1e9):
            scores[key]=quality
            selected[key]=(prep.encode_wave(pcm[begin:end]),first-begin,end-last)
            traces[str(key)]={'first_sample':first,'last_sample':last,'key':key}
    for label,first,last in aligned:
        if label!='_':
            if label not in labels:raise ValueError('Unsupported source phone '+label)
            add((65535,labels[label]),first,last,.12)
    for left,right in zip(aligned,aligned[1:]):
        if left[0]!='_' and right[0]!='_':
            first,last=(left[1]+left[2])//2,(right[1]+right[2])//2
            if not 120<last-first<16000:raise ValueError('Unusable source diphone duration')
            add((labels[left[0]],labels[right[0]]),first,last,.14)
    units.update(selected)
    destination.mkdir()
    for name in ['phones.tsv','lexicon.tsv','LICENSE-Kokoro-Swedish.txt']:
        shutil.copy2(source/name,destination/name)
    with (destination/'units.bin').open('wb') as f:
        f.write(struct.pack('<4s3I',b'BJU1',24000,count,len(units)))
        for (left,right),(wave,lead,tail) in sorted(units.items()):
            f.write(struct.pack('<HHIHH',left,right,len(wave),lead,tail));f.write(wave)
    revision=identity({p.name:digest(p) for p in destination.iterdir()})
    (destination/'revision.txt').write_text(revision+'\n')
    return {'replaced_units':len(selected),'selected_source_spans':traces,
            'duplicate_phone_pairs':len([1 for a,b in zip(aligned,aligned[1:]) if a[0]!='_' and b[0]!='_'])-
                                  sum(k[0]!=65535 for k in selected)}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--neural-pack',type=Path,required=True)
    p.add_argument('--probes',type=Path,required=True)
    p.add_argument('--compact-probe',type=Path,default=ROOT/'build/bjorn-standalone/bjorn-say')
    p.add_argument('--compact-pack',type=Path,default=ROOT/'resources/voices/bjorn-lite-v1')
    p.add_argument('--asr-model',type=Path,required=True)
    p.add_argument('--cpu',action='store_true')
    args=p.parse_args()
    for k,v in vars(args).copy().items():
        if isinstance(v,Path):setattr(args,k,v.resolve())
    args.output.mkdir(parents=True,exist_ok=False)
    args.whisper=Path(shutil.which('whisper-cli'))
    args.ffmpeg=Path(shutil.which('ffmpeg'))
    args.cache=ROOT/'build/unit-control-asr-cache'
    args.asr_identity=identity({'model':digest(args.asr_model),'whisper':digest(args.whisper),
        'ffmpeg':digest(args.ffmpeg),'language':'sv','mc':0,'beam':5,'best_of':5,'temperature':0,'fallback':False,'cpu':args.cpu})
    config=json.loads((args.neural_pack/'config.json').read_text())
    symbols={v:k for k,v in config['vocab'].items()}
    report={'scope':'Oracle same-sentence extraction control; no production changes; not a generalization result',
            'renderer_sha256':digest(args.compact_probe),'asr_identity':args.asr_identity,
            'source_pack_sha256':digest(args.neural_pack/'voice-pack.json'),
            'compact_pack_sha256':digest(args.compact_pack/'units.bin'),'samples':[]}
    for name,text in SAMPLES:
        folder=args.output/name;folder.mkdir()
        tokens=[int(v) for v in execute([args.probes/'ortho-kokoro-text-probe',args.neural_pack],input=text+'\n').split()]
        token_file=folder/'source.tokens';token_file.write_text(' '.join(map(str,tokens)))
        raw=folder/'source.f32';dur=folder/'source.durations'
        execute([args.probes/'ortho-kokoro-audio-probe',args.neural_pack,token_file,'bjorn',raw,dur])
        pcm=np.fromfile(raw,dtype='<f4').astype(float)
        gain=min(1,.85/max(np.max(np.abs(pcm)),1e-9));pcm*=gain
        aligned=prep.aligned_phones([0]+tokens+[0],[int(v) for v in dur.read_text().split()],symbols,len(pcm))
        # Use the exact teacher phone sequence for BOTH native renders, removing lexical differences.
        ipa=''.join(symbols[t] for t in tokens if symbols[t] not in prep.PUNCTUATION)
        spoken='⟦'+ipa+'⟧'
        oracle=folder/'sentence-specific-pack'
        traces=replace_units(args.compact_pack,oracle,pcm,aligned)
        write_audio(folder/'neural-source.wav',pcm,24000)
        decoded=np.frombuffer(prep.encode_wave(pcm),dtype='u1').astype(float)/127.5-1
        decoded=np.sign(decoded)*np.expm1(np.abs(decoded)*np.log(256))/255
        write_audio(folder/'companding-only.wav',decoded,24000)
        variants={}
        for voice,pack in [('shipped-units',args.compact_pack),('same-sentence-units',oracle)]:
            variants[voice]=json.loads(execute([args.compact_probe,pack,folder/(voice+'.wav'),spoken]))
        results=[]
        for voice in ['neural-source','companding-only','shipped-units','same-sentence-units']:
            wav=folder/(voice+'.wav')
            rec=recognize(args,{'audio':str(wav.relative_to(args.output)),'audio_sha256':digest(wav)})
            wave,rate=read_audio(wav)
            results.append({'voice':voice,'wav':str(wav.relative_to(args.output)),'sha256':digest(wav),
                'recognition':rec,'score':score(text,rec['text']),'acoustic':acoustic(wave,rate),
                'renderer_stats':variants.get(voice)})
        report['samples'].append({'name':name,'text':text,'ipa':ipa,'aligned':aligned,'gain':float(gain),
                                  'unit_provenance':traces,'results':results})
        dump(args.output/'report.json',report)
        print(json.dumps({'name':name,'scores':{r['voice']:r['score']['wer'] for r in results}}),flush=True)


if __name__=='__main__':main()
