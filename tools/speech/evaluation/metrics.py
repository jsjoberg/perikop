"""Independent transcript scoring, acoustic candidates and paired uncertainty (MIT)."""
from collections import Counter
import re
import unicodedata
import wave

import numpy as np

VERSION = 'perikop-eval-1'


def normalize(text):
    # Keep Swedish letters and digits. Hyphens/quotes/punctuation become boundaries.
    return ' '.join(re.findall(r'[^\W_]+', unicodedata.normalize('NFC', text).casefold()))


def alignment(reference, hypothesis):
    """Levenshtein alignment, deterministic substitution/deletion/insertion tie order."""
    n,m = len(reference),len(hypothesis)
    d = np.zeros((n+1,m+1),dtype=int)
    d[:,0],d[0,:] = np.arange(n+1),np.arange(m+1)
    for i in range(1,n+1):
        for j in range(1,m+1):
            d[i,j] = min(d[i-1,j-1]+(reference[i-1]!=hypothesis[j-1]),d[i-1,j]+1,d[i,j-1]+1)
    i,j,ops = n,m,[]
    while i or j:
        if i and j and d[i,j] == d[i-1,j-1]+(reference[i-1]!=hypothesis[j-1]):
            ops.append(('equal' if reference[i-1]==hypothesis[j-1] else 'substitute',reference[i-1],hypothesis[j-1]))
            i,j = i-1,j-1
        elif i and d[i,j] == d[i-1,j]+1:
            ops.append(('delete',reference[i-1],None)); i-=1
        else:
            ops.append(('insert',None,hypothesis[j-1])); j-=1
    return list(reversed(ops))


def score(reference, hypothesis, categories=None):
    # Whisper control markers are decoder metadata, not spoken words.
    spoken = re.sub(r'<\|[^|>]+\|>', ' ', hypothesis)
    ref,hyp = normalize(reference),normalize(spoken)
    if not ref:
        raise ValueError('Empty normalized reference')
    ops = alignment(ref.split(),hyp.split())
    counts = Counter(o[0] for o in ops)
    chars = alignment(list(ref.replace(' ','')),list(hyp.replace(' ','')))
    errors = sum(counts[k] for k in ('substitute','delete','insert'))
    category_counts = {}
    for name,words in (categories or {}).items():
        relevant = [o for o in ops if o[1] in words]
        category_counts[name] = {'reference_words':len(relevant),
                                'substitutions_or_deletions':sum(o[0]!='equal' for o in relevant)}
    return {'wer':errors/len(ref.split()), 'cer':sum(o[0]!='equal' for o in chars)/len(ref.replace(' ','')),
            'word_errors':errors,'reference_words':len(ref.split()),
            'char_errors':sum(o[0]!='equal' for o in chars),'reference_chars':len(ref.replace(' ','')),
            'substitutions':counts['substitute'],'deletions':counts['delete'],'insertions':counts['insert'],
            'empty_transcript':not bool(hyp),'normalized_hypothesis':hyp,
            'categories':category_counts,'errors':[{'op':o,'expected':a,'recognized':b} for o,a,b in ops if o!='equal']}


def read_audio(path):
    with wave.open(str(path),'rb') as f:
        if f.getnchannels()!=1 or f.getsampwidth()!=2 or f.getcomptype()!='NONE':
            raise ValueError(f'Expected mono signed 16-bit PCM: {path}')
        rate = f.getframerate()
        pcm = np.frombuffer(f.readframes(f.getnframes()),dtype='<i2').astype(float)/32768
    if rate < 16000 or rate > 96000 or not 0 < len(pcm) <= rate*600:
        raise ValueError('Audio must be nonempty, 16-96 kHz, at most 600 seconds')
    return pcm,rate


def write_audio(path, pcm, rate):
    pcm = np.asarray(pcm)
    if not len(pcm) or not np.isfinite(pcm).all() or np.max(np.abs(pcm))>1:
        raise ValueError('Invalid audio')
    with wave.open(str(path),'wb') as f:
        f.setparams((1,2,rate,0,'NONE','not compressed'))
        f.writeframes(np.rint(pcm*32767).astype('<i2').tobytes())


def describe(values):
    a = np.asarray(values,dtype=float)
    return {k:float(np.quantile(a,q)) if len(a) else None
            for k,q in [('p05',.05),('median',.5),('p95',.95)]}


def runs(mask):
    edges = np.diff(np.r_[False,mask,False].astype(int))
    return list(zip(np.flatnonzero(edges==1),np.flatnonzero(edges==-1)))


def acoustic(pcm, rate):
    """Descriptive features; detected transitions are candidates, never quality verdicts."""
    hop,window = round(rate*.01),round(rate*.04)
    padded = np.pad(pcm,(window//2,window))
    frames = np.lib.stride_tricks.sliding_window_view(padded,window)[::hop][:int(np.ceil(len(pcm)/hop))]
    centered = frames-frames.mean(axis=1,keepdims=True)
    rms = np.sqrt(np.mean(frames**2,axis=1))
    silence = rms < max(10**(-50/20),float(rms.max())*.01)
    magnitude = np.abs(np.fft.rfft(centered*np.hanning(window),axis=1))
    power = magnitude**2
    bins = np.fft.rfftfreq(window,1/rate)
    centroid = (power@bins)/np.maximum(power.sum(axis=1),1e-20)
    unit = magnitude/np.maximum(np.linalg.norm(magnitude,axis=1,keepdims=True),1e-20)
    flux = 1-np.sum(unit[1:]*unit[:-1],axis=1)
    active_pair = ~silence[1:] & ~silence[:-1]
    # Autocorrelation pitch is independent of the synthesis front end. Octave errors remain possible.
    fft = 1 << (2*window-1).bit_length()
    ac = np.fft.irfft(np.abs(np.fft.rfft(centered,n=fft,axis=1))**2,n=fft,axis=1)[:,:window]
    low,high = int(rate/450),min(window-2,int(rate/65))
    square = np.c_[np.zeros(len(frames)),np.cumsum(centered**2,axis=1)]
    lags = np.arange(low,high+1)
    denom = np.sqrt(np.maximum(square[:,window-lags]*(square[:,-1,None]-square[:,lags]),1e-20))
    correlation = ac[:,lags]/denom
    f0,periodicity = np.zeros(len(frames)),np.zeros(len(frames))
    for i,row in enumerate(correlation):
        peaks = np.flatnonzero((row[1:-1]>row[:-2]) & (row[1:-1]>=row[2:]))+1
        if len(peaks):
            best = peaks[np.argmax(row[peaks])]
            # Prefer the first sufficiently strong peak to avoid repeated-period subharmonics.
            good = peaks[row[peaks] >= max(.65,row[best]*.92)]
            lag = good[0] if len(good) else best
            periodicity[i] = np.clip(row[lag],0,1)
            if not silence[i] and periodicity[i] >= .65:
                f0[i] = rate/(low+lag)
    voiced = f0>0
    pitch_pair = voiced[1:] & voiced[:-1]
    jumps = np.zeros(len(f0)-1)
    jumps[pitch_pair] = 12*np.abs(np.log2(f0[1:][pitch_pair]/f0[:-1][pitch_pair]))
    # Absolute and local-energy tests reduce high-frequency fricative false positives.
    derivative = np.abs(np.diff(pcm))
    local = rms[np.minimum(np.arange(len(derivative))//hop,len(rms)-1)]
    clicks = np.flatnonzero((derivative>.2) & (derivative>6*np.maximum(local,.005)))
    # Collapse adjacent candidate samples within 10 ms.
    clicks = clicks[np.r_[True,np.diff(clicks)>hop]] if len(clicks) else clicks
    silent_runs = runs(silence)
    internal = [(a,b) for a,b in silent_runs if a>0 and b<len(silence)]
    duration = len(pcm)/rate
    return {'duration_seconds':duration,'peak':float(np.max(np.abs(pcm))),
            'rms_dbfs':float(20*np.log10(max(np.sqrt(np.mean(pcm**2)),1e-10))),
            'clipped_fraction':float(np.mean(np.abs(pcm)>=32760/32768)),
            'silence_fraction':float(silence.mean()),
            'max_internal_silence_seconds':max(((b-a)*.01 for a,b in internal),default=0),
            'internal_silences':[{'start':a*.01,'seconds':(b-a)*.01} for a,b in internal if b-a>=20],
            'voiced_fraction':float(voiced.mean()),'f0_hz':describe(f0[voiced]),
            'pitch_step_semitones':describe(jumps[pitch_pair]),
            'pitch_jump_candidates_per_second':float(np.sum(jumps>6)/duration),
            'pitch_jump_candidate_times':(np.flatnonzero(jumps>6)[:50]*.01).tolist(),
            'periodicity':describe(periodicity[~silence]),
            'voiced_run_seconds':describe([(b-a)*.01 for a,b in runs(voiced)]),
            'voicing_transitions_per_second':float(np.sum(np.diff(voiced.astype(int))!=0)/duration),
            'spectral_centroid_hz':describe(centroid[~silence]),
            'spectral_flux':describe(flux[active_pair]),
            'spectral_jump_candidates_per_second':float(np.sum((flux>.35)&active_pair)/duration),
            'wave_jump_candidates_per_second':len(clicks)/duration,
            'wave_jump_candidate_times':(clicks[:50]/rate).tolist()}


def aggregate(rows):
    if not rows:
        return {'n':0}
    scores = [r['intelligibility'] for r in rows]
    return {'n':len(rows),'wer':sum(s['word_errors'] for s in scores)/sum(s['reference_words'] for s in scores),
            'cer':sum(s['char_errors'] for s in scores)/sum(s['reference_chars'] for s in scores),
            'word_errors':sum(s['word_errors'] for s in scores),
            'reference_words':sum(s['reference_words'] for s in scores),
            'empty_transcripts':sum(s['empty_transcript'] for s in scores),
            'failed_passages':[r['id'] for r in rows if r['intelligibility']['wer']>=.5],
            'acoustic':{key:describe([r['acoustic'][key] for r in rows]) for key in
                ['duration_seconds','clipped_fraction','silence_fraction','max_internal_silence_seconds',
                 'voiced_fraction','voicing_transitions_per_second','pitch_jump_candidates_per_second',
                 'spectral_jump_candidates_per_second','wave_jump_candidates_per_second']}}


def paired_delta(before, after, metric='wer', seed=1917, repetitions=4000):
    """Paired passage bootstrap of corpus micro error-rate delta (after minus before)."""
    if set(before)!=set(after) or not before:
        raise ValueError('Comparison requires identical nonempty passage IDs')
    ids = sorted(before)
    numerator,denominator = ('word_errors','reference_words') if metric=='wer' else ('char_errors','reference_chars')
    a = np.array([before[k]['intelligibility'][numerator] for k in ids])
    b = np.array([after[k]['intelligibility'][numerator] for k in ids])
    na = np.array([before[k]['intelligibility'][denominator] for k in ids])
    nb = np.array([after[k]['intelligibility'][denominator] for k in ids])
    if not np.array_equal(na,nb):
        raise ValueError('Reference counts changed')
    # Resample chapters together when passage metadata is available.
    # The frozen corpus includes several adjacent passages from Genesis 1 and Psalm 23.
    clusters = {}
    for index,key in enumerate(ids):
        group = before[key].get('reference',key).split(':')[0]
        clusters.setdefault(group,[]).append(index)
    groups = list(clusters.values())
    da = np.array([na[g].sum() for g in groups])
    delta = np.array([(b[g]-a[g]).sum() for g in groups])
    rng = np.random.default_rng(seed)
    index = rng.integers(0,len(groups),size=(repetitions,len(groups)))
    boot = delta[index].sum(axis=1)/da[index].sum(axis=1)
    return {'n':len(ids),'clusters':len(groups),'delta':float((b-a).sum()/na.sum()),
            'ci95':[float(v) for v in np.quantile(boot,[.025,.975])],
            'seed':seed,'bootstrap_repetitions':repetitions,
            'unit':'chapter cluster when reference metadata exists; otherwise passage'}
