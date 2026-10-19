"""MIT. Separate full-reference acoustic errors; no learned/combined quality score.

Alignment uses low-order spectral shape, not teacher phone predictions or F0.
Timing and quiet-interval errors remain visible after spectral time alignment.
"""
from pathlib import Path
import inspect

import numpy as np

from metrics import read_audio,runs
from prosody_experiment import analyze
from run import digest,identity

VERSION='perikop-full-reference-1'
HOP=480
RATE=24000


def features(pcm):
    window=1024
    frames=np.lib.stride_tricks.sliding_window_view(np.pad(pcm,(window//2,window)),window)[::HOP][:int(np.ceil(len(pcm)/HOP))]
    rms=np.sqrt(np.mean(frames**2,axis=1));quiet=rms<max(1e-4,float(rms.max())*.01)
    active=np.flatnonzero(~quiet)
    if len(active)<5:return None
    begin,end=int(active[0]),int(active[-1]+1)
    frames=frames[begin:end];rms=rms[begin:end];quiet=quiet[begin:end]
    taper=np.hanning(window)
    power=(np.abs(np.fft.rfft(frames*taper,axis=1))/taper.sum())**2
    hz=np.fft.rfftfreq(window,1/RATE)
    mel=lambda f:2595*np.log10(1+f/700)
    edges=700*(10**(np.linspace(mel(50),mel(11000),66)/2595)-1)
    filters=np.maximum(0,np.minimum((hz[None,:]-edges[:-2,None])/(edges[1:-1,None]-edges[:-2,None]),
                                    (edges[2:,None]-hz[None,:])/(edges[2:,None]-edges[1:-1,None])))
    filters/=np.maximum(filters.sum(axis=1,keepdims=True),1e-20)
    floor=max(float(power.max())*1e-8,1e-30)
    logmel=10*np.log10(np.maximum(power@filters.T,floor))
    basis=np.cos(np.pi/64*(np.arange(64)+.5)[None,:]*np.arange(1,21)[:,None])*np.sqrt(2/64)
    cepstra=logmel@basis.T
    return {'mel':logmel,'cepstra':cepstra,'spectrum':10*np.log10(np.maximum(power,floor)),
            'rms_db':20*np.log10(np.maximum(rms,max(1e-15,float(rms.max())*1e-5))),'quiet':quiet,
            'times':(np.arange(begin,end)*HOP/RATE),'duration':len(pcm)/RATE,
            'speech_rms':np.sqrt(np.mean(rms[~quiet]**2))}


def cached_features(wav,cache,reaper):
    cache.mkdir(parents=True,exist_ok=True)
    audio_hash=digest(wav);extractor_hash=identity([inspect.getsource(features),inspect.getsource(cached_features)])
    key=identity([VERSION,extractor_hash,audio_hash,digest(reaper)])
    path=cache/(key+'.npz')
    if path.exists():
        with np.load(path,allow_pickle=False) as saved:return {k:saved[k] for k in saved.files}
    pcm,rate=read_audio(wav)
    if rate!=RATE:raise ValueError('Reference metrics require mono 24 kHz PCM')
    result=features(pcm)
    if result is None:return None
    analysis_key=identity(['reaper-analysis-1',audio_hash,digest(reaper)])
    _,pitch=analyze(reaper,wav,cache/analysis_key)
    indices=np.clip(np.searchsorted(pitch[:,0],result['times']),0,len(pitch)-1)
    left=np.maximum(0,indices-1)
    indices=np.where(abs(pitch[left,0]-result['times'])<abs(pitch[indices,0]-result['times']),left,indices)
    result['f0']=np.maximum(0,pitch[indices,2])
    np.savez_compressed(path,**result)
    return result


def spectral_path(reference,candidate):
    a,b=reference['cepstra'],candidate['cepstra'];n,m=len(a),len(b)
    if max(n,m)>6000:raise ValueError('At most 120 seconds of active span per comparison')
    if not .5<=m/n<=2:raise ValueError('Active-span duration ratio outside DTW slope limits')
    # Orthonormal low-order DCT distance divided by sqrt(64): envelope dB units.
    cost=np.sqrt(np.maximum(0,np.sum(a*a,axis=1)[:,None]+np.sum(b*b,axis=1)[None,:]-2*a@b.T)/64)
    gain=20*np.log10(reference['speech_rms']/candidate['speech_rms'])
    cost+=.05*np.minimum(40,abs(reference['rms_db'][:,None]-(candidate['rms_db'][None,:]+gain)))
    dp=np.full((n,m),np.inf);parent=np.full((n,m),-1,dtype=np.int8)
    dp[0,0]=cost[0,0]
    # Steps (1,1)/(1,2)/(2,1) prohibit unlimited repetitions. A normalized-time
    # band of +/-2s and a 2dB step penalty constrain opportunistic warping.
    for i in range(n):
        center=i*(m-1)/max(1,n-1)
        for j in range(max(0,int(center)-100),min(m,int(center)+102)):
            if i==0 and j==0:continue
            choices=[dp[i-1,j-1] if i and j else np.inf,
                     dp[i-1,j-2]+cost[i,j-1]+2 if i and j>1 else np.inf,
                     dp[i-2,j-1]+cost[i-1,j]+2 if i>1 and j else np.inf]
            which=int(np.argmin(choices));dp[i,j]=cost[i,j]+choices[which];parent[i,j]=which
    if not np.isfinite(dp[-1,-1]):raise ValueError('No constrained acoustic alignment')
    path=[];i,j=n-1,m-1
    while i or j:
        which=parent[i,j];path.append((i,j))
        if which==0:i-=1;j-=1
        elif which==1:path.append((i,j-1));i-=1;j-=2
        elif which==2:path.append((i-1,j));i-=2;j-=1
        else:raise ValueError('Broken DTW traceback')
    path.append((0,0));return np.array(path[::-1],dtype=int)


def quiet_comparison(reference,candidate,path):
    r,c=path.T
    expected=[(a,b) for a,b in runs(reference['quiet']) if b-a>=3 and a>0 and b<len(reference['quiet'])]
    actual=[(a,b) for a,b in runs(candidate['quiet']) if b-a>=3 and a>0 and b<len(candidate['quiet'])]
    used=set();errors=[];missing=0
    for a,b in expected:
        predicted=float(np.interp((a+b)/2,r,c))
        available=[(abs((x+y)/2-predicted),i) for i,(x,y) in enumerate(actual) if i not in used]
        if not available or min(available)[0]>10:missing+=1;continue
        _,index=min(available);used.add(index);x,y=actual[index]
        errors.append(abs((b-a)-(y-x))*20)
    return {'reference_count':len(expected),'candidate_count':len(actual),'missing':missing,'extra':len(actual)-len(used),
            'matched_duration_mae_ms':float(np.mean(errors)) if errors else None,
            'reference_total_ms':int(sum(b-a for a,b in expected)*20),'candidate_total_ms':int(sum(b-a for a,b in actual)*20)}


def compare(reference,candidate):
    if reference is None:return {'status':'invalid_reference_no_active_audio'}
    if candidate is None:return {'status':'candidate_no_active_audio'}
    try:path=spectral_path(reference,candidate)
    except ValueError as e:return {'status':'alignment_failed','reason':str(e),'duration_ratio':float(candidate['duration']/reference['duration'])}
    r,c=path.T;gain=float(20*np.log10(reference['speech_rms']/candidate['speech_rms']))
    active=~reference['quiet'][r] | ~candidate['quiet'][c]
    mel_error=np.mean(abs(reference['mel'][r]-(candidate['mel'][c]+gain)),axis=1)
    spectral_error=np.mean(abs(reference['spectrum'][r]-(candidate['spectrum'][c]+gain)),axis=1)
    envelope=np.linalg.norm(reference['cepstra'][r]-candidate['cepstra'][c],axis=1)/np.sqrt(64)
    energy=reference['rms_db'][r]-(candidate['rms_db'][c]+gain)
    rf,cf=reference['f0'][r],candidate['f0'][c];joint=(rf>0)&(cf>0)&active
    shift=12*np.log2(rf[joint]/cf[joint]);f0_error=np.full(len(r),np.nan);f0_error[joint]=abs(shift)
    rv,cv=(rf>0),(cf>0)
    corr=None
    if joint.sum()>2 and np.std(np.log2(rf[joint]))>1e-6 and np.std(np.log2(cf[joint]))>1e-6:
        corr=float(np.corrcoef(np.log2(rf[joint]),np.log2(cf[joint]))[0,1])
    # Timing is measured separately; DTW must not make a slow/staccato render
    # appear correct simply because its matched spectral frames look similar.
    unique=np.arange(len(reference['times']))
    mapped=np.array([np.mean(c[r==i]) for i in unique])*.02
    slope=(mapped[10:]-mapped[:-10])/.2 if len(mapped)>10 else np.array([1.])
    ratio=float((len(candidate['times'])-1)/max(1,len(reference['times'])-1))
    local=np.abs(np.log(np.maximum(slope,1e-8)/ratio))
    rd=np.diff(reference['mel'],axis=0,prepend=reference['mel'][:1])
    cd=np.diff(candidate['mel'],axis=0,prepend=candidate['mel'][:1])
    motion=np.mean(abs(rd[r]-cd[c]),axis=1)
    def convergence(key):
        a=10**(reference[key][r[active]]/20);b=10**((candidate[key][c[active]]+gain)/20)
        return float(np.linalg.norm(a-b)/max(1e-20,np.linalg.norm(a)))
    metrics={'logmel_mae_db':float(np.mean(mel_error[active])),
        'mel_spectral_convergence':convergence('mel'),'stft_spectral_convergence':convergence('spectrum'),
        'cepstral_envelope_distance_db':float(np.mean(envelope[active])),
        'log_spectrum_mae_db':float(np.mean(spectral_error[active])),
        'spectral_motion_mae_db':float(np.mean(motion[active])),
        'energy_envelope_rmse_db':float(np.sqrt(np.mean(energy[active]**2))),
        'f0_rmse_semitones':float(np.sqrt(np.mean(shift**2))) if len(shift) else None,
        'f0_mae_semitones':float(np.mean(abs(shift))) if len(shift) else None,
        'f0_contour_correlation':corr,'joint_voiced_coverage':float(np.mean(joint[active])),
        'voicing_disagreement_fraction':float(np.mean((rv!=cv)[active])),
        'duration_ratio':float(candidate['duration']/reference['duration']),
        'duration_abs_log_error':float(abs(np.log(candidate['duration']/reference['duration']))),
        'active_span_ratio':ratio,'local_timing_deviation_log_p90':float(np.percentile(local,90)),
        'candidate_gain_correction_db':gain,'quiet_intervals':quiet_comparison(reference,candidate,path)}
    windows=[]
    for start in range(int(reference['times'][-1])+1):
        mask=(reference['times'][r]>=start)&(reference['times'][r]<start+1)&active
        if not np.any(mask):continue
        voiced=mask&joint
        windows.append({'reference_start_seconds':start,'candidate_start_seconds':float(candidate['times'][c[mask][0]]),
            'candidate_end_seconds':float(candidate['times'][c[mask][-1]]),
            'logmel_mae_db':float(np.mean(mel_error[mask])),
            'f0_mae_semitones':float(np.mean(f0_error[voiced])) if np.any(voiced) else None,
            'voicing_disagreement_fraction':float(np.mean((rv!=cv)[mask]))})
    return {'status':'ok','metrics':metrics,'windows':windows,
            'alignment_seconds':np.c_[reference['times'][r[::5]],candidate['times'][c[::5]]].tolist(),
            'alignment_scope':'Acoustic DTW; approximate correspondences, not verified phone/word boundaries'}
