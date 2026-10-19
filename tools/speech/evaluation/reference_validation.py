#!/usr/bin/env python3
"""MIT. Controlled full-reference sensitivity checks, not perceptual-score training."""
import argparse
import json
from pathlib import Path

import numpy as np

from metrics import read_audio,write_audio
from phrase_experiment import phrase_marks,pitch_curve
from prosody_experiment import analyze,make_grains,render
from run import digest,dump


def prepare(master,output,reaper):
    output.mkdir(parents=True,exist_ok=True)
    sources=json.loads((master/'master.json').read_text())['passages'][:5];pairs=[]
    for index,row in enumerate(sources):
        wav=master/row['audio'];pcm,rate=read_audio(wav);folder=output/row['id'];folder.mkdir(exist_ok=True)
        pairs.append({'id':row['id'],'condition':'identity','audio':str(wav.resolve()),'audio_sha256':digest(wav)})
        rng=np.random.default_rng(1917);noise=rng.normal(0,1,len(pcm));noise/=np.sqrt(np.mean(noise**2))
        level=np.sqrt(np.mean(pcm**2))
        controls={'polarity':-pcm,'gain-minus-6db':pcm*.5,'noise-30db':pcm+noise*level*10**(-30/20),
                  'noise-15db':pcm+noise*level*10**(-15/20),'silence':np.zeros_like(pcm)}
        for cutoff in (1500,3500):
            hz=np.fft.rfftfreq(len(pcm),1/rate)
            response=np.clip((cutoff+700-hz)/700,0,1)
            controls['lowpass-'+str(cutoff)]=np.fft.irfft(np.fft.rfft(pcm)*response,n=len(pcm))
        center=len(pcm)//2
        controls['inserted-300ms-gap']=np.r_[pcm[:center],np.zeros(7200),pcm[center:]]
        chunks=[];cursor=0
        for center in range(rate,len(pcm)-rate//2,rate):
            if np.sqrt(np.mean(pcm[center-240:center+240]**2))<.005:continue
            chunks.extend([pcm[cursor:center],np.zeros(1920)]);cursor=center
        controls['staccato-gaps']=np.concatenate([*chunks,pcm[cursor:]])
        energy=np.convolve(pcm**2,np.ones(960)/960,mode='same');center=int(np.argmax(energy))
        dropped=pcm.copy();dropped[max(0,center-1920):min(len(pcm),center+1920)]=0
        controls['muted-active-160ms']=dropped
        pm,f0=analyze(reaper,wav,folder/'source');curve=pitch_curve(f0)
        points=np.array([[0.,0.],[len(pcm),len(pcm)]])
        for name,change in [('pitch-plus-3semitones',.25),('flat-pitch',None)]:
            target=curve.copy()
            target[:,1]=target[:,1]+change if change is not None else np.median(curve[:,1])
            marks,positions,_=phrase_marks(pm,points,points,len(pcm),target,maximum_semitones=12.)
            changed,norm=render(pcm,len(pcm),make_grains(pcm,pm,marks,positions))
            if np.any(norm<=1e-8):raise ValueError('Control render uncovered samples')
            controls[name]=changed
        for name,changed in controls.items():
            path=folder/(name+'.wav');write_audio(path,np.clip(changed,-1,1),rate)
            pairs.append({'id':row['id'],'condition':name,'audio':str(path.resolve()),'audio_sha256':digest(path)})
        wrong=master/sources[(index+1)%len(sources)]['audio']
        pairs.append({'id':row['id'],'condition':'wrong-text','audio':str(wrong.resolve()),'audio_sha256':digest(wrong)})
    dump(output/'manifest.json',{'metadata':{'scope':'Five fixed development masters. Controlled signal changes; no human-quality labels inferred.',
            'seed':1917,'clipping':'Controls encoded with fixed clamp [-1,1]; no peak normalization',
            'code_sha256':digest(Path(__file__))},'pairs':pairs})
    print(json.dumps({'validation_pairs':len(pairs)}))


def check(comparison,output):
    report=json.loads(comparison.read_text());summary=report['summary'];rows=report['rows'];checks=[]
    def mean(condition,signal):return summary[condition]['signals'][signal]['mean']
    def add(name,passed,observed):checks.append({'name':name,'passed':bool(passed),'observed':observed})
    for metric in ('logmel_mae_db','f0_rmse_semitones','energy_envelope_rmse_db','active_span_abs_log_error'):
        value=mean('identity',metric);add('Identity: '+metric,abs(value)<1e-8,value)
    value=mean('polarity','logmel_mae_db');add('Polarity inversion acoustically invariant',value<.05,value)
    value=mean('gain-minus-6db','logmel_mae_db');add('Global gain does not masquerade as timbre',value<.1,value)
    a,b=mean('noise-15db','logmel_mae_db'),mean('noise-30db','logmel_mae_db')
    add('Increasing noise increases spectral error',a>b+.1,{'snr15':a,'snr30':b})
    a,b=mean('lowpass-1500','logmel_mae_db'),mean('lowpass-3500','logmel_mae_db')
    add('Stronger lowpass increases spectral error',a>b+.1,{'lowpass1500':a,'lowpass3500':b})
    value=mean('pitch-plus-3semitones','f0_rmse_semitones')
    add('Actual three-semitone signal shift is recovered',2.4<value<3.8,value)
    value=mean('inserted-300ms-gap','active_span_abs_log_error')
    add('Time alignment does not erase inserted gap duration',value>.01,value)
    value=summary['staccato-gaps']['quiet_extra_total']
    add('Extra staccato gaps remain visible after alignment',value>0,value)
    value=mean('muted-active-160ms','voicing_disagreement_fraction')
    add('Muted active span changes voicing signal',value>.005,value)
    values=[r['comparison']['status'] for r in rows if r['condition']=='silence']
    add('Silence fails explicitly instead of receiving a good distance',all(v=='candidate_no_active_audio' for v in values),values)
    good=[r['comparison']['metrics']['logmel_mae_db'] for r in rows if r['condition']=='wrong-text' and r['comparison']['status']=='ok']
    failures=summary['wrong-text']['alignment_or_audio_failures']
    add('Different text cannot match by speaker timbre alone',failures+sum(v>1 for v in good)==5,{'alignment_failures':failures,'spectral_db':good})
    result={'scope':'Functional and controlled-signal sensitivity only. Does not establish human pleasantness ranking.',
            'comparison_sha256':digest(comparison),'passed':all(c['passed'] for c in checks),'checks':checks}
    dump(output,result);print(json.dumps(result,indent=2))
    if not result['passed']:raise SystemExit(1)


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('command',choices=['prepare','check'])
    p.add_argument('--master',type=Path);p.add_argument('--reaper',type=Path)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--comparison',type=Path)
    a=p.parse_args()
    if a.command=='prepare':prepare(a.master,a.output,a.reaper)
    else:check(a.comparison,a.output)


if __name__=='__main__':main()
