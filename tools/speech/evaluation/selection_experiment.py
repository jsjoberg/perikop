#!/usr/bin/env python3
"""Offline connected-unit experiment. Original MIT code; not the application voice.

Training excludes every benchmark chapter. Selection never reads target audio or
target durations. Neural inference is used only to prepare the reusable source bank.
"""
import argparse
from collections import Counter, defaultdict
import importlib.util
import heapq
import json
from pathlib import Path
import re
import shutil
import sqlite3
import struct
import subprocess
import time

import numpy as np

from metrics import write_audio, score, aggregate, read_audio, acoustic
from run import ROOT, digest, dump, execute, recognize, identity

spec = importlib.util.spec_from_file_location('prepare_voice', ROOT/'tools/speech/bjorn-poc/prepare_voice.py')
prep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prep)


def prepare(args):
    args.output.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(f'file:{ROOT}/resources/corpus/corpus.db?mode=ro', uri=True)
    benchmark = json.loads((ROOT/'tools/speech/evaluation/dataset.json').read_text())
    forbidden = {tuple(p['id'].split('-')[:2]) for p in benchmark['passages']}
    rows = con.execute("""select b.code,v.chapter,v.verse,v.text from verse v
        join book b on b.id=v.book_id join source s on s.id=v.source_id
        where s.code='sv1917' order by b.id,v.chapter,v.verse""").fetchall()
    entries = prep.dictionary(args.neural_pack, ROOT/'resources/corpus/pronunciation-sv.tsv')
    vocab = json.loads((args.neural_pack/'config.json').read_text())['vocab']
    candidates = []
    frequencies = Counter()
    for book, chapter, verse, text in rows:
        if (book.lower(), f'{chapter:03}') in forbidden:
            continue
        words = re.findall(r'[^\W\d_]+', text.lower())
        if not 6 <= len(words) <= 36 or any(w not in entries for w in words):
            continue
        if sum(len(entries[w])+1 for w in words) > 330:
            continue
        candidates.append((f'{book.lower()}-{chapter:03}-{verse:03}', text, words))
        frequencies.update(words)
    # Corpus-only coverage criterion, independent of benchmark text/recognition.
    counts = Counter()
    selected = []
    weights = {w:float(np.log1p(n)) for w,n in frequencies.items()}
    def utility(i):
        return sum(weights[w]/(1+counts[w])**1.5 for w in set(candidates[i][2])) / len(candidates[i][2])**.35
    heap = [(-utility(i),i) for i in range(len(candidates))]
    heapq.heapify(heap)
    while heap and len(selected)<args.count:
        previous,index = heapq.heappop(heap)
        value = utility(index)
        if value < -previous-1e-9:
            heapq.heappush(heap,(-value,index))
            continue
        item = candidates[index]
        selected.append(item)
        counts.update(item[2])
    source = {'method':'Corpus word coverage, no benchmark chapters', 'forbidden_chapters':sorted(forbidden),
              'corpus_sha256':digest(ROOT/'resources/corpus/corpus.db'), 'teacher_sha256':digest(args.neural_pack/'voice-pack.json'),
              'utterances':[]}
    for index, (name, text, words) in enumerate(selected):
        prefix = args.output/name
        receipt = prefix.with_suffix('.json')
        if receipt.exists():
            item = json.loads(receipt.read_text())
            assert item['text'] == text and item['teacher_sha256'] == source['teacher_sha256']
            for suffix in ('.f32', '.tokens', '.durations'):
                assert digest(prefix.with_suffix(suffix)) == item['hashes'][suffix]
        else:
            tokens = execute([args.probes/'ortho-kokoro-text-probe',args.neural_pack], input=text+'\n')
            prefix.with_suffix('.tokens').write_text(tokens)
            execute([args.probes/'ortho-kokoro-audio-probe',args.neural_pack,prefix.with_suffix('.tokens'),
                     'bjorn',prefix.with_suffix('.f32'),prefix.with_suffix('.durations')])
            item = {'id':name,'text':text,'teacher_sha256':source['teacher_sha256'],
                    'hashes':{s:digest(prefix.with_suffix(s)) for s in ('.f32','.tokens','.durations')}}
            dump(receipt,item)
        source['utterances'].append(item)
        dump(args.output/'sources.json', source)
        print(json.dumps({'prepared':index+1,'of':len(selected),'id':name}),flush=True)


def load_sources(args):
    config = json.loads((args.neural_pack/'config.json').read_text())
    symbols = {v:k for k,v in config['vocab'].items()}
    sources = []
    if args.source_manifest:
        saved_sources = json.loads(args.source_manifest.read_text())['sources']
        paths = [Path(s['path']) for s in saved_sources]
        for source,path in zip(saved_sources,paths):
            if digest(path)!=source['tokens_sha256'] or digest(path.with_suffix('.f32'))!=source['pcm_sha256']:
                raise ValueError(f'Training source changed: {path}')
    else:
        paths = [p.with_suffix('.tokens') for p in sorted(args.sources.glob('*.json')) if p.name!='sources.json']
        if not paths:paths = sorted(args.sources.glob('*.tokens'))
    if args.legacy_sources and not args.source_manifest:
        paths += sorted(args.legacy_sources.glob('*.tokens'))
    for path in paths:
        pcm = np.fromfile(path.with_suffix('.f32'),dtype='<f4').astype(float)
        assert len(pcm) and np.isfinite(pcm).all()
        pcm *= min(1,.85/max(np.max(np.abs(pcm)),1e-9))
        # Use the actual lossy bank waveform for both feature extraction and rendering.
        codes = np.frombuffer(prep.encode_wave(pcm),dtype='u1')
        u = codes.astype(float)/127.5-1
        pcm = np.sign(u)*np.expm1(np.abs(u)*np.log(256))/255
        tokens = [int(v) for v in path.read_text().split()]
        durations = [int(v) for v in path.with_suffix('.durations').read_text().split()]
        alignment = prep.aligned_phones([0]+tokens+[0],durations,symbols,len(pcm))
        alignment = [[label,max(0,min(len(pcm),a+args.alignment_offset)),
                      max(0,min(len(pcm),b+args.alignment_offset))] for label,a,b in alignment]
        word_flags = phone_flags(''.join(symbols[t] for t in tokens))
        flags = []
        at = 0
        for label,_,_ in alignment:
            if label=='_':flags.append((False,False))
            else:
                assert label==word_flags[at][0], (path,label,word_flags[at])
                flags.append(word_flags[at][1:])
                at += 1
        assert at==len(word_flags)
        sources.append({'path':str(path), 'pcm':pcm, 'codes':codes,'phones':alignment,
                        'flags':flags,
                        'centers':[(a+b)//2 for _,a,b in alignment]})
    if not sources:
        raise ValueError('No training sources')
    return sources


def phone_flags(ipa):
    result = []
    for word in re.split(r'[\s.,;:!?]+',ipa):
        labels = prep.phones(word)
        result += [(label,i==0,i==len(labels)-1) for i,label in enumerate(labels)]
    return result


def boundary_feature(pcm, center):
    wave = np.zeros(1024)
    begin, end = max(0,center-512), min(len(pcm),center+512)
    wave[begin-center+512:end-center+512] = pcm[begin:end]
    power = np.abs(np.fft.rfft(wave*np.hanning(1024)))**2
    bands = np.array([np.log(1e-8+np.mean(b)) for b in np.array_split(power[1:350],24)])
    # Keep energy as well as spectral shape; no target waveform is involved.
    return bands


def select(args):
    args.output.mkdir(parents=True,exist_ok=True)
    sources = load_sources(args)
    candidates = defaultdict(list)
    base_candidates = defaultdict(list)
    lengths = defaultdict(list)
    for s,source in enumerate(sources):
        for i,(label,a,b) in enumerate(source['phones']):
            if label != '_':
                lengths[label].append(b-a)
        for i,(a,b) in enumerate(zip(source['phones'],source['phones'][1:])):
            if '_' not in (a[0],b[0]) and 120 < source['centers'][i+1]-source['centers'][i] < 16000:
                candidates[(a[0],b[0])].append((s,i))
                base_candidates[(a[0].lstrip(prep.STRESS),b[0].lstrip(prep.STRESS))].append((s,i))
        source['features'] = np.stack([boundary_feature(source['pcm'],c) for c in source['centers']])
    medians = {k:float(np.median(v)) for k,v in lengths.items()}
    data = json.loads(args.dataset.read_text())
    rows = [p for p in data['passages'] if p['split']=='dev'][:args.limit]
    bank_path = args.output/'wave-bank.u8'
    offsets = []
    with bank_path.open('wb') as f:
        for source in sources:
            offsets.append(f.tell())
            f.write(source['codes'].tobytes())
    provenance = {'method':'Viterbi diphone selection with context and spectral join cost; contiguous spans preserved',
                  'runtime':'Native waveform bank and precomputed spans; no neural inference',
                  'bank_bytes':bank_path.stat().st_size,'bank_sha256':digest(bank_path),
                  'alignment_offset_samples':args.alignment_offset,
                  'word_boundary_cost':args.word_boundary_cost,'beam':args.beam,
                  'phase_alignment':args.phase_align,
                  'code_sha256':digest(Path(__file__)),
                  'sources':[{'path':s['path'],'tokens_sha256':digest(Path(s['path'])),
                              'pcm_sha256':digest(Path(s['path']).with_suffix('.f32'))} for s in sources],
                  'passages':[]}
    # A native plan is deliberately simple: bank offset, sample count, overlap.
    for passage in rows:
        started = time.perf_counter()
        raw = execute([args.probes/'ortho-kokoro-text-probe',args.neural_pack],input=passage['text']+'\n')
        symbols = {v:k for k,v in json.loads((args.neural_pack/'config.json').read_text())['vocab'].items()}
        ipa = ''.join(symbols[int(t)] for t in raw.split())
        clauses = []
        parts = re.split('([.,;:!?])',ipa)
        backoffs = 0
        for n in range(0,len(parts),2):
            flagged = phone_flags(parts[n])
            target = [v[0] for v in flagged]
            if not target:continue
            punctuation = parts[n+1] if n+1<len(parts) else '.'
            pause = 7680 if punctuation in '.!?' else 3360
            begin = 0
            for i,key in enumerate(zip(target,target[1:])):
                if key not in candidates and tuple(k.lstrip(prep.STRESS) for k in key) not in base_candidates:
                    clauses.append((target[begin:i+1],0,[v[1:] for v in flagged[begin:i+1]]))
                    begin = i+1
                    backoffs += 1
            clauses.append((target[begin:],pause,[v[1:] for v in flagged[begin:]]))
        audio = []
        spans = []
        traces = []
        for target,pause,flags in clauses:
            layers = []
            states = []
            missing = []
            for at,key in enumerate(zip(target,target[1:])):
                options = candidates.get(key,[]) or base_candidates.get(tuple(k.lstrip(prep.STRESS) for k in key),[])
                if not options:
                    # Do not silently claim unsupported pairs are synthesized correctly.
                    missing.append([at,*key])
                    break
                target_cost = []
                for s,i in options:
                    phones = sources[s]['phones']
                    cost = 0.
                    for delta in (0,1):
                        cost += args.word_boundary_cost*sum(a!=b for a,b in zip(flags[at+delta],sources[s]['flags'][i+delta]))
                    for delta,weight in [(-2,.15),(-1,.45),(2,.45),(3,.15)]:
                        ti,si = at+delta,i+delta
                        if 0<=ti<len(target) and 0<=si<len(phones):
                            cost += weight*(target[ti]!=phones[si][0])
                    duration = sources[s]['centers'][i+1]-sources[s]['centers'][i]
                    expected = (medians[key[0]]+medians[key[1]])/2
                    cost += .25*abs(np.log(duration/expected))
                    target_cost.append(cost)
                # Retain contextual variety before the acoustic search.
                keep = np.argsort(target_cost,kind='stable')[:96]
                options = [options[k] for k in keep]
                costs = np.array([target_cost[k] for k in keep])
                if at:
                    joins = np.empty((len(options),len(states)))
                    for j,(s,i) in enumerate(options):
                        new = sources[s]['features'][i]
                        for k,(_,ps,pi,_) in enumerate(states):
                            contiguous = s==ps and i==pi+1
                            joins[j,k] = 0 if contiguous else 1.2 + .10*np.mean(np.abs(new-sources[ps]['features'][pi+1]))
                    paths = joins+np.array([v[0] for v in states])[None,:]
                    parents = np.argmin(paths,axis=1)
                    costs += paths[np.arange(len(options)),parents]
                else:
                    parents = np.full(len(options),-1)
                keep = np.argsort(costs,kind='stable')[:args.beam]
                states = [(float(costs[k]),*options[k],int(parents[k])) for k in keep]
                layers.append(states)
            if missing:
                raise ValueError(f'Uncovered pairs in {passage["id"]}: {missing}')
            chosen = []
            index = 0
            for layer in reversed(layers):
                _,s,i,index = layer[index]
                chosen.append((s,i))
            chosen.reverse()
            runs = []
            for s,i in chosen:
                if runs and runs[-1][0]==s and runs[-1][2]==i:
                    runs[-1][2] = i+1
                else:
                    runs.append([s,i,i+1])
            if len(target)==1:
                options = [(abs((b-a)-medians[target[0]]),s,i)
                           for s,source in enumerate(sources) for i,(label,a,b) in enumerate(source['phones'])
                           if label==target[0]]
                _,s,i = min(options)
                runs = [[s,i,i]]
            chunks = []
            for n,(s,first,last) in enumerate(runs):
                source = sources[s]
                # Joins lie at the center of the same phone on both sides.
                # Preserve the whole first/last phone at utterance boundaries.
                begin = source['phones'][first][1] if n==0 else source['centers'][first]-120
                end = source['phones'][last][2] if n==len(runs)-1 else source['centers'][last]+120
                begin,end = max(0,begin), min(len(source['pcm']),end)
                if args.phase_align and n and any(c in source['phones'][first][0] for c in 'aɑeɛəiɪoɔuʊyøœɨɜmnlrjvŋɳ'):
                    overlap = min(240,end-begin,len(chunks))
                    previous = chunks[-overlap:]
                    norm = float(previous@previous)
                    best = (-2,begin)
                    for shift in range(-180,181,2):
                        at = begin+shift
                        if at<0 or at+overlap>end:continue
                        wave = source['pcm'][at:at+overlap]
                        correlation = float(previous@wave)/np.sqrt(max(1e-18,norm*float(wave@wave)))
                        quality = correlation-.02*abs(shift)/180
                        if quality>best[0]:best = (quality,at)
                    begin = best[1]
                part = source['pcm'][begin:end].copy()
                overlap = 0 if n==0 else min(240,len(part),len(chunks))
                if n==0:
                    chunks = part
                else:
                    ramp = np.arange(1,overlap+1)/(overlap+1)
                    chunks[-overlap:] = chunks[-overlap:]*(1-ramp)+part[:overlap]*ramp
                    chunks = np.concatenate([chunks,part[overlap:]])
                spans.append([offsets[s]+begin,end-begin,overlap])
                traces.append({'source':Path(source['path']).stem,'first_phone':first,'last_phone':last,
                               'labels':[p[0] for p in source['phones'][first:last+1]],'samples':end-begin})
            fade = min(120,len(chunks)//2)
            chunks[:fade] *= np.arange(fade)/fade
            chunks[-fade:] *= np.arange(fade-1,-1,-1)/fade
            # Native renderer fades each clause, then adds a short fixed pause.
            spans.append([0,pause,0])
            spans[-1][0] = 0xffffffffffffffff
            audio.extend([chunks,np.zeros(pause)])
        wav = args.output/(passage['id']+'.wav')
        write_audio(wav,np.concatenate(audio),24000)
        plan = args.output/(passage['id']+'.plan')
        with plan.open('wb') as f:
            f.write(struct.pack('<4sI',b'BUS1',len(spans)))
            for offset,count,overlap in spans:
                f.write(struct.pack('<QII',offset,count,overlap))
        row = {**passage,'audio':wav.name,'audio_sha256':digest(wav),'plan':plan.name,
               'selection_seconds':time.perf_counter()-started,'spans':traces,'joins':len(traces)-len(clauses),
               'missing_pair_backoffs':backoffs}
        provenance['passages'].append(row)
        dump(args.output/'selection.json',provenance)
        print(json.dumps({'selected':passage['id'],'joins':row['joins'],'seconds':row['selection_seconds']}),flush=True)


def evaluate(args):
    args.whisper = Path(shutil.which('whisper-cli'))
    args.ffmpeg = Path(shutil.which('ffmpeg'))
    args.cache = ROOT/'build/selection-asr-cache'
    args.asr_identity = identity({'model':digest(args.asr_model),'whisper':digest(args.whisper),
        'ffmpeg':digest(args.ffmpeg),'language':'sv','mc':0,'beam':5,'best_of':5,'temperature':0,'fallback':False,'cpu':args.cpu})
    data = json.loads((args.output/'selection.json').read_text())
    baseline = json.loads((ROOT/'build/speech-eval/baseline-v4/report.json').read_text())['rows']
    report = {'scope':'Development only. ASR improvement does not establish pleasant voice quality.',
              'selection_sha256':digest(args.output/'selection.json'),'asr_identity':args.asr_identity,'rows':[]}
    for passage in data['passages']:
        rec = recognize(args,passage)
        wave,rate = read_audio(args.output/passage['audio'])
        result = {k:v for k,v in passage.items() if k!='spans'}
        result.update({'recognition':rec['text'],'intelligibility':score(passage['text'],rec['text']),
                       'audio_seconds':len(wave)/rate,
                       'acoustic':acoustic(wave,rate),
                       'baseline':[{k:r[k] for k in ('voice','intelligibility')} for r in baseline
                                   if r['id']==passage['id'] and r['voice'] in ('compact','neural')]})
        report['rows'].append(result)
        report['aggregate'] = aggregate(report['rows'])
        dump(args.output/'evaluation.json',report)
        print(json.dumps({'id':passage['id'],'wer':result['intelligibility']['wer'],'text':rec['text']},ensure_ascii=False),flush=True)


def native(args):
    data = json.loads((args.output/'selection.json').read_text())
    results = []
    for passage in data['passages']:
        target = args.output/(passage['id']+'.native.wav')
        stats = json.loads(execute([args.renderer,args.output/'wave-bank.u8',args.output/passage['plan'],target]))
        python_pcm,rate = read_audio(args.output/passage['audio'])
        native_pcm,native_rate = read_audio(target)
        assert rate==native_rate and len(python_pcm)==len(native_pcm)
        difference = float(np.max(np.abs(python_pcm-native_pcm))*32768)
        if difference>1:raise ValueError(f'Native renderer differs by {difference} PCM units')
        results.append({'id':passage['id'],'stats':stats,'maximum_pcm_unit_difference':difference,
                        'wav_sha256':digest(target)})
        passage['python_audio_sha256'] = passage['audio_sha256']
        passage['audio'] = target.name
        passage['audio_sha256'] = digest(target)
        passage['native_stats'] = stats
    data['native_verification'] = {'renderer_sha256':digest(args.renderer),'results':results}
    dump(args.output/'selection.json',data)
    print(json.dumps(data['native_verification']))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['prepare','select','evaluate','native'])
    parser.add_argument('--neural-pack',type=Path,required=True)
    parser.add_argument('--probes',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--sources',type=Path)
    parser.add_argument('--source-manifest',type=Path)
    parser.add_argument('--legacy-sources',type=Path)
    parser.add_argument('--count',type=int,default=180)
    parser.add_argument('--limit',type=int,default=5)
    parser.add_argument('--beam',type=int,default=16)
    parser.add_argument('--alignment-offset',type=int,default=0)
    parser.add_argument('--word-boundary-cost',type=float,default=0)
    parser.add_argument('--phase-align',action='store_true')
    parser.add_argument('--asr-model',type=Path)
    parser.add_argument('--renderer',type=Path,default=ROOT/'build/unit-selection/bjorn-unit-render')
    parser.add_argument('--cpu',action='store_true')
    parser.add_argument('--dataset',type=Path,default=ROOT/'tools/speech/evaluation/dataset.json')
    args = parser.parse_args()
    if args.command=='prepare':prepare(args)
    elif args.command=='select':select(args)
    elif args.command=='native':native(args)
    else:evaluate(args)


if __name__=='__main__':main()
