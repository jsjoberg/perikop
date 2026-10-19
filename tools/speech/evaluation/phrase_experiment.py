#!/usr/bin/env python3
"""MIT. Phrase flow/intonation experiment retaining the v4 word durations.

All waveform material is compact v2. The neural reference supplies only offline
pitch and pause controls. This is a listening candidate, not an accepted voice.
"""
import argparse
import json
from pathlib import Path
import struct

import numpy as np

from metrics import read_audio,write_audio
from prosody_experiment import duration_marks,make_grains,render,save_plan,track,verify_native
from run import ROOT,digest,dump


def verified_tracks(folder,label,audio_hash):
    receipt=json.loads((folder/(label+'.receipt.json')).read_text())
    if receipt['identity']['audio_sha256']!=audio_hash:raise ValueError('Stale analysis input')
    for suffix in ('.pm','.f0'):
        if digest(folder/(label+suffix))!=receipt['hashes'][suffix]:raise ValueError('Changed analysis track')
    return track(folder/(label+'.pm')),track(folder/(label+'.f0'))


def plan_pauses(path):
    raw=path.read_bytes();magic,count=struct.unpack_from('<4sI',raw)
    if magic!=b'BUS1' or len(raw)!=8+16*count:raise ValueError('Invalid compact span plan')
    pauses=[];cursor=0
    for i in range(count):
        offset,n,overlap=struct.unpack_from('<QII',raw,8+16*i)
        if offset==2**64-1 and n:pauses.append((cursor,cursor+n))
        cursor+=n-overlap
    return pauses,cursor


def quiet_intervals(pcm,rate=24000):
    frame=rate//100
    wave=np.pad(pcm,(0,(-len(pcm))%frame))
    # Quiet energy, rather than absence of F0: unvoiced consonants are speech.
    low=np.sqrt(np.mean(wave.reshape(-1,frame)**2,axis=1))<.002
    edges=np.diff(np.r_[False,low,False].astype(int))
    return [(int(a*frame),min(len(pcm),int(b*frame)))
            for a,b in zip(np.flatnonzero(edges==1),np.flatnonzero(edges==-1)) if b-a>=3]


def pause_targets(pauses,dense,teacher_pcm,input_length):
    quiet=quiet_intervals(teacher_pcm);result=[]
    for begin,end in pauses:
        count=end-begin
        times=np.interp([begin,end],dense[:,1],dense[:,0]);middle=float(np.mean(times))
        # The token map is approximate: accept nearby acoustic quiet intervals,
        # but do not treat every predicted punctuation duration as an actual gap.
        nearby=[(a,b) for a,b in quiet if a-2880<=middle<=b+2880]
        best=min(nearby,key=lambda ab:abs((ab[0]+ab[1])/2-middle)) if nearby else None
        observed=best[1]-best[0] if best else 0
        # Retain a short separation where units were independently faded. Final
        # trailing padding is held fixed; it is not an inter-phrase control.
        lower,upper=(3840,11040) if count>=6000 else (960,4320)
        desired=count if end==input_length else int(np.clip(observed,lower,upper))
        result.append({'begin':begin,'end':end,'source_samples':count,'output_samples':desired,
                       'reference_quiet_samples':observed,'reference_search_center':middle,
                       'reference_quiet_interval':list(best) if best else None})
    return result


def flow_map(word_points,pauses):
    boundaries=[p[k] for p in pauses for k in ('begin','end')]
    source=np.unique(np.r_[word_points[:,1],boundaries])
    target=np.interp(source,word_points[:,1],word_points[:,0])
    for p in pauses:
        a,b=p['begin'],p['end']
        old=float(np.interp(b,word_points[:,1],word_points[:,0])-np.interp(a,word_points[:,1],word_points[:,0]))
        if abs(old-(b-a))>1e-6:raise ValueError('V4 pause timing was not identity')
        target+=(p['output_samples']-old)*np.clip((source-a)/(b-a),0,1)
    points=np.c_[target,source]
    if not np.all(np.diff(target)>0):raise ValueError('Invalid phrase timing map')
    return points


def pitch_curve(f0):
    valid=(f0[:,2]>=65)&(f0[:,2]<=350)
    if valid.sum()<2:raise ValueError('No reliable reference pitch')
    logs=np.interp(f0[:,0],f0[valid,0],np.log2(f0[valid,2]))
    # 45 ms Hann smoothing rejects frame jitter while retaining word-scale shape.
    kernel=np.hanning(9);kernel/=kernel.sum()
    return np.c_[f0[:,0],np.convolve(np.pad(logs,(4,4),mode='edge'),kernel,mode='valid')]


def phrase_marks(source_pm,points,dense,length,curve,strength=1.,maximum_semitones=6.):
    if not 0<=strength<=1 or not 0<=maximum_semitones<=12:raise ValueError('Invalid pitch control bounds')
    if strength==0:return (*duration_marks(source_pm,points,length),{'strength':0.,'clamped_epochs':0})
    centers=np.rint(source_pm[:,0]*24000).astype(int)
    marks=[(0.,0.)];positions=[float(points[0,1])];shifts=[];clamped=0
    t=float(np.interp(centers[0],points[:,1],points[:,0]))
    while t<length-1:
        where=float(np.interp(t,points[:,0],points[:,1]));index=int(np.argmin(np.abs(centers-where)))
        voiced=source_pm[index,1]>0
        if t>marks[-1][0]:
            marks.append((t,float(voiced)));positions.append(float(centers[index]) if voiced else where)
        step=int(centers[index+1]-centers[index]) if index+1<len(centers) else int(centers[-1]-centers[-2])
        step=max(1,step)
        if voiced:
            reference_time=float(np.interp(where,dense[:,1],dense[:,0]))/24000
            desired=float(np.interp(reference_time,curve[:,0],curve[:,1]))
            shift=strength*12*(desired-np.log2(24000/step))
            bounded=float(np.clip(shift,-maximum_semitones,maximum_semitones))
            clamped+=abs(shift-bounded)>1e-9;shifts.append(bounded)
            step*=2**(-bounded/12)
        t+=step
    marks.append((float(length-1),0.));positions.append(float(points[-1,1]-1))
    return np.array(marks),np.array(positions),{'strength':strength,'maximum_semitones':maximum_semitones,
          'clamped_epochs':int(clamped),'voiced_epochs':len(shifts),
          'applied_shift_semitones_p10_p50_p90':np.percentile(shifts,[10,50,90]).tolist()}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--word-controls',type=Path,required=True)
    p.add_argument('--phone-controls',type=Path,required=True)
    p.add_argument('--teacher',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--renderer',type=Path,default=ROOT/'build/unit-selection/bjorn-grain-render')
    p.add_argument('--ids',nargs='+',default=['1cor-013-004-007'])
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    word_report=args.word_controls/'report.json';phone_report=args.phone_controls/'report.json'
    words={r['id']:r for r in json.loads(word_report.read_text())['passages']}
    phones={r['id']:r for r in json.loads(phone_report.read_text())['passages']}
    data=json.loads((args.candidate/'selection.json').read_text())
    rows={r['id']:r for r in data['passages']}
    report={'scope':__doc__,'code_sha256':digest(Path(__file__)),
            'word_report_sha256':digest(word_report),'phone_report_sha256':digest(phone_report),
            'renderer_sha256':digest(args.renderer),'passages':[]}
    selection=[]
    for id in args.ids:
        row=rows[id];old=words[id];dense=phones[id]
        source=args.candidate/row['audio'];reference=args.teacher/(id+'.wav')
        source_hash=digest(source);reference_hash=digest(reference)
        for item in (old,dense):
            if item['source_sha256']!=source_hash or item['teacher_control_source_sha256']!=reference_hash:
                raise ValueError('Stale prosody controls')
        if old['timing_policy']!='bounded-word' or dense.get('timing_policy','phoneme')!='phoneme':
            raise ValueError('Incompatible timing controls')
        pcm,rate=read_audio(source);teacher_pcm,teacher_rate=read_audio(reference)
        if rate!=24000 or teacher_rate!=24000:raise ValueError('Expected mono 24 kHz audio')
        source_pm,_=verified_tracks(args.word_controls/id,'source',source_hash)
        _,teacher_f0=verified_tracks(args.word_controls/id,'teacher',reference_hash)
        phone_points=np.array(dense['timing_anchors']);word_points=np.array(old['timing_anchors'])
        pauses,input_length=plan_pauses(args.candidate/row['plan'])
        if input_length!=len(pcm):raise ValueError('Source plan/audio length mismatch')
        changes=pause_targets(pauses,phone_points,teacher_pcm,len(pcm))
        points=flow_map(word_points,changes);length=int(round(points[-1,0]))
        curve=pitch_curve(teacher_f0)
        marks,positions,pitch_audit=phrase_marks(source_pm,points,phone_points,length,curve)
        grains=make_grains(pcm,source_pm,marks,positions);wave,norm=render(pcm,length,grains)
        if np.any(norm<=1e-8):raise ValueError('Uncovered output samples')
        folder=args.output/id;folder.mkdir(exist_ok=True);name='phrase-flow'
        write_audio(folder/(name+'.wav'),wave,24000);save_plan(folder/(name+'.grains'),length,grains)
        native,stats,difference=verify_native(args.renderer,source,folder,name)
        selection.append({'id':id+'-'+name,'original_id':id,'split':'dev','text':row['text'],'variant':name,
                          'audio':str(native.relative_to(args.output)),'audio_sha256':digest(native),
                          'native_stats':stats})
        report['passages'].append({'id':id,'source_sha256':source_hash,'teacher_control_source_sha256':reference_hash,
            'timing_anchors':points.tolist(),'pauses':changes,'pitch_control':pitch_audit,'seconds':length/24000,
            'uncovered_samples':0,'native_maximum_pcm_unit_difference':difference,
            'native_stats':stats,'audio_sha256':digest(native)})
        dump(args.output/'report.json',report);dump(args.output/'selection.json',{'scope':__doc__,'passages':selection})
        print(json.dumps(report['passages'][-1]),flush=True)


if __name__=='__main__':main()
