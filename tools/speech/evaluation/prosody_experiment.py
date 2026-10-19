#!/usr/bin/env python3
"""Controlled TD-PSOLA experiment on the rejected compact candidate (MIT).

Sound comes exclusively from the compact waveform. Teacher audio supplies
offline pitch and predicted timing controls, never waveform samples.
REAPER is an offline analysis dependency only. This is not an accepted voice.
"""
import argparse
import json
from pathlib import Path
import struct

import numpy as np

from run import ROOT,digest,dump,execute
from metrics import read_audio,write_audio
from selection_experiment import prep


def track(path):
    text=Path(path).read_text().split('EST_Header_End\n',1)[1]
    rows=np.array([[float(v) for v in line.split()] for line in text.splitlines() if line.strip()])
    if len(rows)<2 or rows.shape[1]!=3 or not np.all(np.diff(rows[:,0])>0):
        raise ValueError('Invalid REAPER track')
    return rows


def analyze(reaper,wav,prefix):
    receipt=prefix.with_suffix('.receipt.json')
    identity={'audio_sha256':digest(wav),'reaper_sha256':digest(reaper)}
    if receipt.exists():
        saved=json.loads(receipt.read_text())
        if saved['identity']!=identity:raise ValueError('Stale epoch analysis')
        for suffix in ('.pm','.f0'):
            if digest(prefix.with_suffix(suffix))!=saved['hashes'][suffix]:raise ValueError('Changed epoch track')
    else:
        execute([reaper,'-i',wav,'-p',prefix.with_suffix('.pm'),'-f',prefix.with_suffix('.f0'),'-a'])
        dump(receipt,{'identity':identity,'hashes':{s:digest(prefix.with_suffix(s)) for s in ('.pm','.f0')}})
    return track(prefix.with_suffix('.pm')),track(prefix.with_suffix('.f0'))


def anchors_for(row,selection,teacher,neural_pack,output):
    if len(teacher['metadata']['chunks'])!=1:raise ValueError('Experiment currently accepts one teacher chunk')
    chunk=teacher['metadata']['chunks'][0]
    symbols={v:k for k,v in json.loads((neural_pack/'config.json').read_text())['vocab'].items()}
    target=[p for p in prep.aligned_phones([0]+chunk['tokens']+[0],chunk['predicted_token_durations'],
                                           symbols,chunk['samples'],-1500) if p[0]!='_']
    source_data={}
    offsets={};cursor=0
    for item in selection['sources']:
        path=Path(item['path']);count=path.with_suffix('.f32').stat().st_size//4
        offsets[path.stem]=cursor;cursor+=count
        source_data[path.stem]=(path,count)
    raw=(output/row['plan']).read_bytes()
    magic,n=struct.unpack_from('<4sI',raw)
    assert magic==b'BUS1' and len(raw)==8+n*16
    spans=[struct.unpack_from('<QII',raw,8+i*16) for i in range(n)]
    points=[];target_index=0;wave_cursor=0;trace_index=0
    for offset,count,overlap in spans:
        if offset==2**64-1:
            # A zero-length marker splits a missing pair, rather than punctuation.
            previous=target[target_index]
            target_index+=1
            next_begin=target[target_index][1] if target_index<len(target) else previous[2]+2400
            points.extend([(previous[2],wave_cursor),(next_begin,wave_cursor+count)])
            wave_cursor+=count
            continue
        trace=row['spans'][trace_index];trace_index+=1
        path,total=source_data[trace['source']]
        tokens=[int(v) for v in path.read_text().split()]
        durations=[int(v) for v in path.with_suffix('.durations').read_text().split()]
        aligned=prep.aligned_phones([0]+tokens+[0],durations,symbols,total,-1500)
        begin=offset-offsets[path.stem]
        first,last=trace['first_phone'],trace['last_phone']
        output_begin=wave_cursor-overlap
        if not points:points.append((target[target_index][1],0))
        for j in range(first,last+1):
            wanted=target[target_index+j-first]
            actual=aligned[j]
            assert wanted[0].lstrip(prep.STRESS)==actual[0].lstrip(prep.STRESS),(wanted,actual)
            t=(wanted[1]+wanted[2])/2
            s=(actual[1]+actual[2])/2-begin+output_begin
            points.append((t,s))
        target_index+=last-first
        wave_cursor=output_begin+count
    assert target_index==len(target) and trace_index==len(row['spans'])
    # Shared centers may have two slightly different source positions after phase search.
    unique={}
    for t,s in points:unique.setdefault(t,[]).append(s)
    points=np.array([(t,float(np.mean(s))) for t,s in sorted(unique.items())])
    if not np.all(np.diff(points[:,1])>=0):raise ValueError('Non-monotonic timing map')
    return points,target[0][1],min(chunk['samples'],target[-1][2]+2400)


def make_grains(pcm,source_pm,target_marks,source_positions):
    source_centers=np.rint(source_pm[:,0]*24000).astype(int)
    voiced=np.flatnonzero(source_pm[:,1]>0)
    voiced_centers=source_centers[voiced]
    grains=[]
    for index,((destination,target_voiced),source_position) in enumerate(zip(target_marks,source_positions)):
        source=int(round(source_position));left=right=240
        if target_voiced and len(voiced_centers):
            k=int(np.argmin(np.abs(voiced_centers-source)))
            epoch=voiced[k]
            center=source_centers[epoch]
            if abs(center-source)<=360:
                source=int(center)
                before=source_centers[epoch]-source_centers[max(0,epoch-1)]
                after=source_centers[min(len(source_centers)-1,epoch+1)]-source_centers[epoch]
                left=int(np.clip(before,60,400));right=int(np.clip(after,60,400))
        # A lower target pitch needs wider windows to maintain overlap coverage.
        if index:left=max(left,int(np.ceil(destination-target_marks[index-1,0])))
        if index+1<len(target_marks):right=max(right,int(np.ceil(target_marks[index+1,0]-destination)))
        left=min(left,1000);right=min(right,1000)
        if not 0<=source<len(pcm):continue
        grains.append((source,int(round(destination)),left,right,1.0))
    return grains


def render(pcm,length,grains):
    out=np.zeros(length);normalizer=np.zeros(length)
    for source,destination,left,right,gain in grains:
        delta=np.arange(-left,right+1)
        window=np.where(delta<=0,.5+.5*np.cos(np.pi*delta/left),.5+.5*np.cos(np.pi*delta/right))
        si=source+delta;di=destination+delta
        valid=(si>=0)&(si<len(pcm))&(di>=0)&(di<length)
        out[di[valid]]+=pcm[si[valid]]*window[valid]*gain
        normalizer[di[valid]]+=window[valid]
    active=normalizer>1e-8
    out[active]/=normalizer[active]
    return out,normalizer


def save_plan(path,length,grains):
    with path.open('wb') as f:
        f.write(struct.pack('<4s3I',b'BGP1',24000,length,len(grains)))
        for grain in grains:f.write(struct.pack('<IIHHf',*grain))


def verify_native(renderer,source,folder,variant):
    target=folder/(variant+'.native.wav')
    timing=json.loads(execute([renderer,source,folder/(variant+'.grains'),target]))
    expected,rate=read_audio(folder/(variant+'.wav'));actual,native_rate=read_audio(target)
    if rate!=native_rate or len(expected)!=len(actual):raise ValueError('Native grain output length/rate mismatch')
    difference=float(np.max(np.abs(expected-actual))*32768)
    if difference>1:raise ValueError(f'Native grain output differs by {difference} PCM units')
    return target,timing,difference


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--teacher',type=Path,required=True)
    p.add_argument('--neural-pack',type=Path,required=True)
    p.add_argument('--reaper',type=Path,required=True)
    p.add_argument('--renderer',type=Path,default=ROOT/'build/unit-selection/bjorn-grain-render')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--ids',nargs='+',default=['1cor-013-004-007'])
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    data=json.loads((args.candidate/'selection.json').read_text())
    if not set(args.ids)<={r['id'] for r in data['passages']}:raise ValueError('Unknown compact passage ID')
    report={'scope':__doc__,'code_sha256':digest(Path(__file__)),'reaper_sha256':digest(args.reaper),
            'renderer_sha256':digest(args.renderer),'passages':[]}
    native_rows=[]
    for row in [r for r in data['passages'] if r['id'] in args.ids]:
        folder=args.output/row['id'];folder.mkdir(exist_ok=True)
        source=args.candidate/row['audio'];reference=args.teacher/(row['id']+'.wav')
        pcm,rate=read_audio(source);assert rate==24000
        source_pm,source_f0=analyze(args.reaper,source,folder/'source')
        teacher_pm,teacher_f0=analyze(args.reaper,reference,folder/'teacher')
        metadata=json.loads(reference.with_suffix('.generation.json').read_text())
        points,begin,end=anchors_for(row,data,metadata,args.neural_pack,args.candidate)
        variants=[]
        # Identity controls detect grain/normalization defects before modifying prosody.
        marks=np.c_[source_pm[:,0]*24000,source_pm[:,1]]
        marks=np.vstack([[[0.,0.]],marks,[[len(pcm)-1.,0.]]])
        identity_grains=make_grains(pcm,source_pm,marks,marks[:,0])
        wave,norm=render(pcm,len(pcm),identity_grains)
        mask=norm>1e-6
        identity_error=float(np.max(np.abs(wave-pcm)))
        if identity_error>1e-10:raise ValueError(f'Identity grain reconstruction failed: {identity_error}')
        write_audio(folder/'identity.wav',wave,24000)
        save_plan(folder/'identity.grains',len(pcm),identity_grains)
        _,identity_timing,identity_difference=verify_native(args.renderer,source,folder,'identity')
        teacher_voiced=teacher_f0[:,2]>0
        for name in ['rhythm','rhythm-and-pitch']:
            times=[];t=float(begin)
            while t<end:
                where=float(np.interp(t,points[:,0],points[:,1]))
                frame=min(len(source_f0)-1,max(0,round(where/120)))
                source_pitch=source_f0[frame,2]
                f0=source_pitch
                if name=='rhythm-and-pitch' and source_pitch>0:
                    f0=float(np.interp(t,teacher_f0[teacher_voiced,0]*24000,teacher_f0[teacher_voiced,2]))
                # Keep source phonetic voicing; transfer pitch rather than making
                # an alignment error turn a vowel into an unvoiced interval.
                times.append((t-begin,float(source_pitch>0)))
                t+=24000/f0 if 65<f0<400 else 240
            target_marks=np.array(times)
            positions=np.interp(target_marks[:,0]+begin,points[:,0],points[:,1])
            grains=make_grains(pcm,source_pm,target_marks,positions)
            wave,norm=render(pcm,end-begin,grains)
            write_audio(folder/(name+'.wav'),wave,24000)
            save_plan(folder/(name+'.grains'),len(wave),grains)
            native_wav,timing,difference=verify_native(args.renderer,source,folder,name)
            native_rows.append({'id':row['id']+'-'+name,'original_id':row['id'],'split':'dev','text':row['text'],
                                'variant':name,'audio':str(native_wav.relative_to(args.output)),
                                'audio_sha256':digest(native_wav),'native_stats':timing,
                                'native_maximum_pcm_unit_difference':difference})
            variants.append({'variant':name,'wav':str((folder/(name+'.wav')).relative_to(args.output)),
                             'sha256':digest(folder/(name+'.wav')),'grains':len(grains),
                             'uncovered_samples':int(np.sum(norm<=1e-8)),
                             'native_audio_sha256':digest(native_wav),'native_stats':timing,
                             'seconds':len(wave)/24000})
        report['passages'].append({'id':row['id'],'text':row['text'],'source_sha256':digest(source),
             'teacher_control_source_sha256':digest(reference),'scope':'All waveform samples come from rejected compact v2. Target audio supplies only offline pitch and predicted timing controls; source voicing is retained.',
             'identity_maximum_float_error':identity_error,'identity_uncovered_samples':int(np.sum(~mask)),
             'identity_native_pcm_unit_difference':identity_difference,'identity_native_stats':identity_timing,
             'timing_anchors':points.tolist(),'variants':variants})
        dump(args.output/'report.json',report)
        dump(args.output/'selection.json',{'scope':__doc__,'passages':native_rows})
        print(json.dumps({'id':row['id'],'variants':variants}),flush=True)


if __name__=='__main__':main()
