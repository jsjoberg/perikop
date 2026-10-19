#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy==2.3.5"]
# ///
"""Reproducible, development-only Björn evaluation. No application dependencies (MIT)."""
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

import numpy as np

from metrics import VERSION, acoustic, aggregate, normalize, paired_delta, read_audio, score, write_audio

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
CONTROL_NAMES = ['silence','clipping','misplaced_silence','hard_joins','wrong_timing','subword_noise','destructive_overlap']


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def identity(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,ensure_ascii=False).encode()).hexdigest()


def dump(path, value):
    path.parent.mkdir(parents=True,exist_ok=True)
    temporary = path.with_suffix(path.suffix+'.tmp')
    temporary.write_text(json.dumps(value,ensure_ascii=False,indent=2,allow_nan=False)+'\n')
    temporary.replace(path)


def execute(args, **kwargs):
    result = subprocess.run([str(a) for a in args],capture_output=True,text=True,timeout=600,**kwargs)
    if result.returncode:
        raise RuntimeError(f'Command failed ({result.returncode}): {args[0]}\n{result.stderr[-4000:]}')
    return result.stdout


def file_inventory(folder):
    return {p.name:digest(p) for p in sorted(folder.iterdir()) if p.is_file()}


def controls(pcm,rate):
    """Coarse deterministic corruptions. Noise windows are NOT verified phoneme edits."""
    mid = len(pcm)//2
    hard = pcm.copy()
    step = max(1,round(rate*.12))
    for i in range(step,len(hard),2*step):
        hard[i:i+step] = np.clip(-hard[i:i+step]+.18,-1,1)
    timing = []
    for i in range(0,len(pcm),round(rate*.24)):
        chunk = pcm[i:i+round(rate*.24)]
        factor = .55 if len(timing)%2 else 1.6
        timing.append(np.interp(np.linspace(0,len(chunk)-1,max(1,round(len(chunk)*factor))),np.arange(len(chunk)),chunk))
    noise = pcm.copy()
    rng = np.random.default_rng(1917)
    for i in range(round(rate*.2),len(pcm),round(rate*.3)):
        end = min(i+round(rate*.10),len(pcm))
        noise[i:end] = np.clip(rng.normal(0,.15,end-i),-1,1)
    # Repeated 80 ms overlaps of 120 ms chunks. Destructive half-amplitude test
    # separately tests total cancellation; this one also changes timing/content.
    width,overlap = round(rate*.12),round(rate*.08)
    output = pcm[:width].copy()
    for i in range(width,len(pcm),width):
        chunk = pcm[i:i+width]
        n = min(overlap,len(chunk),len(output))
        ramp = np.linspace(0,1,n)
        output[-n:] = output[-n:]*(1-ramp)+chunk[:n]*ramp
        output = np.r_[output,chunk[n:]]
    return {'silence':np.zeros_like(pcm),'clipping':np.clip(pcm*16,-1,1),
            'misplaced_silence':np.r_[pcm[:mid],np.zeros(rate*3),pcm[mid:]],
            'hard_joins':hard,'wrong_timing':np.concatenate(timing),
            'subword_noise':noise,'destructive_overlap':output}


def token_chunks(tokens):
    """Match PortableSpeech's existing 510-token word-gap split; no synthesis changes."""
    begin = 0
    while begin < len(tokens):
        end = next_begin = len(tokens)
        if end-begin > 510:
            end = next_begin = begin+510
            for at in range(begin+510,begin,-1):
                if tokens[at]==16:
                    end,next_begin = at,at+1
                    break
        yield tokens[begin:end]
        begin = next_begin


def generate(args, data):
    output = args.output
    if (output/'generation.json').exists():
        saved = json.loads((output/'generation.json').read_text())
        if saved['generation_identity']!=args.generation_identity:
            raise ValueError('Output belongs to different generation inputs; choose a new --output')
        for row in saved['audio']:
            if digest(output/row['audio'])!=row['audio_sha256']:
                raise ValueError('Generated audio changed; choose a new --output')
        return saved['audio']
    rows = []
    for passage in data['passages']:
        for voice in ['neural','compact']:
            folder = output/'audio'/voice
            folder.mkdir(parents=True,exist_ok=True)
            wav = folder/(passage['id']+'.wav')
            # Interrupted generations can resume from per-file receipts, never mere existence.
            receipt = wav.with_suffix('.generation.json')
            key = identity([args.generation_identity,passage,voice])
            if receipt.exists() and wav.exists():
                cached = json.loads(receipt.read_text())
                if cached.get('key')==key and cached.get('sha256')==digest(wav):
                    metadata = cached['metadata']
                else:
                    raise ValueError(f'Stale partial generation: {wav}')
            elif voice=='compact':
                metadata = json.loads(execute([args.compact_probe,args.compact_pack,wav,passage['text']]))
            else:
                tokens = execute([args.text_probe,args.neural_pack],input=passage['text']+'\n')
                sequences = list(token_chunks([int(v) for v in tokens.split()]))
                chunks,metadata = [],{'chunks':[], 'alignment_warning':'Teacher predictions, not independent phoneme boundaries',
                                     'chunking':'PortableSpeech 510-token word-gap split; concatenate PCM'}
                for chunk_index,sequence in enumerate(sequences):
                    token_file = wav.with_suffix(f'.{chunk_index}.tokens')
                    token_file.write_text(' '.join(map(str,sequence)))
                    raw = token_file.with_suffix('.f32')
                    durations = token_file.with_suffix('.durations')
                    timing = execute([args.audio_probe,args.neural_pack,token_file,'bjorn',raw,durations])
                    pcm = np.fromfile(raw,dtype='<f4').astype(float)
                    chunks.append(pcm)
                    raw.unlink()
                    metadata['chunks'].append({'probe_timing':timing.strip(),'tokens':sequence,
                        'predicted_token_durations':[int(v) for v in durations.read_text().split()],
                        'samples':len(pcm)})
                pcm = np.concatenate(chunks)
                metadata['float_peak'] = float(np.max(np.abs(pcm)))
                metadata['float_overload_samples'] = int(np.sum(np.abs(pcm)>1))
                metadata['pcm_encoding'] = 'Clamp to [-1,1] as PortableSpeech WAV writer; no normalization'
                write_audio(wav,np.clip(pcm,-1,1),24000)
            dump(receipt,{'key':key,'sha256':digest(wav),'metadata':metadata})
            rows.append({**passage,'voice':voice,'audio':str(wav.relative_to(output)),
                         'audio_sha256':digest(wav),'generation':metadata})
        print(json.dumps({'generated':passage['id']}),flush=True)
    if not args.no_legacy:
        samples = ROOT/'docs/reports/bjorn-poc-samples'
        report = json.loads((samples/'report.json').read_text())
        for clip in report['audio']:
            source = samples/clip['file']
            if digest(source)!=clip['sha256']:
                raise ValueError('Legacy comparison checksum mismatch')
            passage = next(p for p in data['legacy'] if p['id']==clip['sample'])
            for index,segment in enumerate(clip['segments']):
                voice = ['legacy-neural','legacy-harmonic64','legacy-compact'][index]
                wav = output/'audio'/voice/(passage['id']+'.wav')
                wav.parent.mkdir(parents=True,exist_ok=True)
                command = [args.ffmpeg,'-v','error','-y','-i',source,'-ss',segment['start_seconds']]
                if index<2:
                    length = clip['segments'][index+1]['start_seconds']-segment['start_seconds']-.75
                    command += ['-t',f'{length:.6f}']
                execute(command+['-ar','24000','-ac','1','-c:a','pcm_s16le',wav])
                rows.append({**passage,'voice':voice,'audio':str(wav.relative_to(output)),
                             'audio_sha256':digest(wav),'generation':{'source_sha256':clip['sha256'],
                             'segment_start':segment['start_seconds'],'label':segment['label']}})
    if not args.no_controls:
        # First five development passages are fixed controls. Heldout is never used for tuning diagnostics.
        references = [r for r in rows if r['voice']=='neural' and r['split']=='dev'][:5]
        for row in references:
            pcm,rate = read_audio(output/row['audio'])
            for name,changed in controls(pcm,rate).items():
                wav = output/'audio'/('control-'+name)/(row['id']+'.wav')
                wav.parent.mkdir(parents=True,exist_ok=True)
                write_audio(wav,changed,rate)
                rows.append({**row,'voice':'control-'+name,'split':'validation',
                             'audio':str(wav.relative_to(output)),'audio_sha256':digest(wav),
                             'generation':{'parent_sha256':row['audio_sha256'],'corruption':name,'seed':1917}})
    dump(output/'generation.json',{'generation_identity':args.generation_identity,'audio':rows})
    return rows


def recognize(args,row):
    # This function receives a blinded audio record. Text/id/categories never cross the ASR boundary.
    key = identity([row['audio_sha256'],args.asr_identity])
    cache = args.cache/(key+'.json')
    if cache.exists():
        value = json.loads(cache.read_text())
        if value['key']!=key:
            raise ValueError('ASR cache identity mismatch')
        return value
    with tempfile.TemporaryDirectory(prefix='blind-asr-') as tmp:
        wav = Path(tmp)/'input.wav'
        execute([args.ffmpeg,'-v','error','-y','-i',args.output/row['audio'],'-ar','16000','-ac','1',wav])
        prefix = Path(tmp)/'result'
        command = [args.whisper,'-m',args.asr_model,'-f',wav,'-l','sv','-mc','0','-bs','5','-bo','5',
                   '-tp','0','-tpi','0','-nf','-otxt','-oj','-of',prefix,'-t','4']
        if args.cpu:
            command.append('-ng')
        execute(command)
        value = {'key':key,'status':'ok','text':prefix.with_suffix('.txt').read_text().strip(),
                 'raw':json.loads(prefix.with_suffix('.json').read_text())}
        dump(cache,value)
        return value


def flatten(profile,prefix=''):
    result = {}
    for k,v in profile.items():
        key = prefix+k
        if isinstance(v,dict):
            result.update(flatten(v,key+'.'))
        elif isinstance(v,(int,float)) and not isinstance(v,bool):
            result[key] = v
    return result


def summarize(rows,data,errors):
    voices = sorted(set(r['voice'] for r in rows))
    groups = {v:{split:aggregate([r for r in rows if r['voice']==v and r['split']==split])
                 for split in sorted(set(r['split'] for r in rows if r['voice']==v))} for v in voices}
    tags = {v:{tag:aggregate([r for r in rows if r['voice']==v and tag in r['tags']])
               for tag in sorted(set(t for r in rows if r['voice']==v for t in r['tags']))} for v in voices}
    neural = {r['id']:r for r in rows if r['voice']=='neural'}
    gate = {}
    for split in ['dev','heldout']:
        expected = [p for p in data['passages'] if p['split']==split]
        actual = [r for r in rows if r['voice']=='neural' and r['split']==split]
        summary = aggregate(actual)
        gate[split] = {'status':'pass' if len(actual)==len(expected) and summary.get('wer',1)<=.25
                      and summary.get('empty_transcripts',1)==0 else 'unreliable',
                      'rule':'Complete reference coverage; corpus WER <= 0.25; no empty transcripts. Provisional calibration gate.',
                      'measurements':summary}
    comparisons = {}
    for voice in voices:
        if voice in ('neural','legacy-neural'):
            continue
        baseline_voice = 'legacy-neural' if voice.startswith('legacy-') else 'neural'
        candidate = {r['id']:r for r in rows if r['voice']==voice}
        reference = {r['id']:r for r in rows if r['voice']==baseline_voice and r['id'] in candidate}
        if not candidate or set(reference)!=set(candidate):
            continue
        profile_effects = {}
        ref_features = {i:flatten(r['acoustic']) for i,r in reference.items()}
        cand_features = {i:flatten(r['acoustic']) for i,r in candidate.items()}
        for metric in sorted(set.intersection(*(set(v) for v in ref_features.values()),*(set(v) for v in cand_features.values()))):
            differences = [cand_features[i][metric]-ref_features[i][metric] for i in reference]
            profile_effects[metric] = {'median_paired_delta':float(np.median(differences)),
                                      'fraction_increasing':float(np.mean(np.array(differences)>0))}
        comparisons[voice] = {'reference':baseline_voice,'wer':paired_delta(reference,candidate),
                              'cer':paired_delta(reference,candidate,'cer'),'acoustic_deltas':profile_effects}
    categories = {}
    for voice in voices:
        categories[voice] = {}
        for name in data['word_categories']:
            counts = [r['intelligibility']['categories'][name] for r in rows if r['voice']==voice]
            n = sum(c['reference_words'] for c in counts)
            e = sum(c['substitutions_or_deletions'] for c in counts)
            categories[voice][name] = {'reference_words':n,'substitutions_or_deletions':e,'rate':e/n if n else None,
                                     'note':'Reference-word error rate; insertions unassigned; overlapping categories'}
    return {'groups':groups,'passage_tags':tags,'word_categories':categories,'reference_gate':gate,
            'comparisons_to_reference':comparisons,'technical_failures':errors,
            'status':'complete' if not errors else 'incomplete',
            'interpretation':'Acoustic candidates are descriptive. No combined score. ASR cannot establish listener quality.',
            'limitations':['Autocorrelation pitch may have octave errors.',
                          'Abrupt stops and fricatives can trigger spectral/wave candidates.',
                          'No independently aligned phoneme durations or voiced/unvoiced ground truth available.',
                          'Subword noise is a coarse proxy, not a verified phoneme substitution.',
                          'ASR can memorize Biblical language or hallucinate through corruption.',
                          'Chapter-cluster bootstrap does not capture ASR/model or human-rater uncertainty.',
                          'Heldout has only ten passages; do not repeatedly tune against it.']}


def compare_reports(before, after):
    # A changed recognizer, scoring implementation or corpus cannot support a quality verdict.
    fields = ['dataset_sha256','asr_identity','metrics_identity']
    incompatible = [f for f in fields if before['provenance'][f]!=after['provenance'][f]]
    if incompatible:
        return {'status':'incompatible','fields':incompatible}
    results = {}
    for split in ['dev','heldout']:
        expected = set(before['provenance']['passage_ids'][split])
        a = {r['id']:r for r in before['rows'] if r['voice']=='compact' and r['split']==split}
        b = {r['id']:r for r in after['rows'] if r['voice']=='compact' and r['split']==split}
        gate_ok = all(report['summary']['reference_gate'][split]['status']=='pass' for report in [before,after])
        complete = set(a)==set(b)==expected and all(report['summary']['status']=='complete' for report in [before,after])
        if not gate_ok or not complete:
            results[split] = {'verdict':'uncertain','reason':'Unreliable reference gate or incomplete coverage'}
            continue
        wer,cer = paired_delta(a,b),paired_delta(a,b,'cer')
        verdict = 'uncertain'
        # Require at least two percentage points of supported WER change; CER must agree.
        if wer['ci95'][1]<-.02 and cer['ci95'][1]<=0:
            verdict = 'probably_improved'
        elif wer['ci95'][0]>.02 and cer['ci95'][0]>=0:
            verdict = 'probably_deteriorated'
        results[split] = {'verdict':verdict,'wer':wer,'cer':cer,
            'per_passage':[{'id':i,'wer_delta':b[i]['intelligibility']['wer']-a[i]['intelligibility']['wer'],
                'cer_delta':b[i]['intelligibility']['cer']-a[i]['intelligibility']['cer']} for i in sorted(a)]}
    return {'status':'compatible','splits':results,'primary':'dev; heldout is confirmation, never a tuning target',
            'rule':'95% chapter-cluster-bootstrap WER interval beyond +/-0.02 and CER interval agrees; otherwise uncertain'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dataset',type=Path,default=HERE/'dataset.json')
    parser.add_argument('--output',type=Path,required=True,help='New immutable run directory, or resume interrupted run')
    parser.add_argument('--cache',type=Path,default=ROOT/'build/speech-eval-asr-cache')
    parser.add_argument('--neural-pack',type=Path,default=ROOT/'build/kokoro-pack')
    parser.add_argument('--text-probe',type=Path,default=ROOT/'build/cmake/bin/ortho-kokoro-text-probe')
    parser.add_argument('--audio-probe',type=Path,default=ROOT/'build/cmake/bin/ortho-kokoro-audio-probe')
    parser.add_argument('--compact-pack',type=Path,default=ROOT/'resources/voices/bjorn-lite-v1')
    parser.add_argument('--compact-probe',type=Path,default=ROOT/'build/bjorn-standalone/bjorn-say')
    parser.add_argument('--asr-model',type=Path,required=True,help='Local KB-Whisper GGML file; never downloads implicitly')
    parser.add_argument('--whisper',type=Path,default=Path(shutil.which('whisper-cli') or 'whisper-cli'))
    parser.add_argument('--ffmpeg',type=Path,default=Path(shutil.which('ffmpeg') or 'ffmpeg'))
    parser.add_argument('--cpu',action='store_true')
    parser.add_argument('--no-controls',action='store_true')
    parser.add_argument('--no-legacy',action='store_true')
    parser.add_argument('--generate-only',action='store_true')
    parser.add_argument('--baseline',type=Path)
    args = parser.parse_args()
    for name,value in vars(args).copy().items():
        if isinstance(value,Path):
            setattr(args,name,value.resolve())
    args.output.mkdir(parents=True,exist_ok=True)
    data = json.loads(args.dataset.read_text())
    ids = [p['id'] for p in data['passages']]
    if len(set(ids))!=len(ids) or not data['passages']:
        raise ValueError('Dataset IDs must be unique and nonempty')
    executables = {k:digest(getattr(args,k)) for k in ['text_probe','audio_probe','compact_probe','whisper','ffmpeg']}
    packs = {'neural':file_inventory(args.neural_pack),'compact':file_inventory(args.compact_pack)}
    args.asr_identity = identity({'model':digest(args.asr_model),'binary':executables['whisper'],
        'version':execute([args.whisper,'--version']).strip(), 'ffmpeg':executables['ffmpeg'],
        'language':'sv','max_context':0,'beam_size':5,'best_of':5,'temperature':0,'fallback':False,
        'cpu':args.cpu,'threads':4,'prompt':None,'grammar':None})
    args.generation_identity = identity({'dataset':digest(args.dataset),'packs':packs,
        'probes':{k:v for k,v in executables.items() if k!='whisper'},'generator_version':2,
        'controls':not args.no_controls,'legacy':not args.no_legacy})
    provenance = {'schema':1,'version':VERSION,'dataset_sha256':digest(args.dataset),
        'asr_identity':args.asr_identity,'asr_model_sha256':digest(args.asr_model),
        'asr_model_path':str(args.asr_model),'asr_source':'https://huggingface.co/KBLab/kb-whisper-small',
        'metrics_identity':identity([digest(HERE/'metrics.py'),VERSION]),
        'runner_sha256':digest(__file__),'generation_identity':args.generation_identity,
        'executables':executables,'packs':packs,'python':sys.version,'numpy':np.__version__,
        'platform':platform.platform(),'source_commit':execute(['git','-C',ROOT,'rev-parse','HEAD']).strip(),
        'source_diff_sha256':hashlib.sha256(execute(['git','-C',ROOT,'diff','HEAD']).encode()).hexdigest(),
        'passage_ids':{s:[p['id'] for p in data['passages'] if p['split']==s] for s in ['dev','heldout']},
        'input_options':{k:str(v) if isinstance(v,Path) else v for k,v in vars(args).items()}}
    if (args.output/'report.json').exists():
        raise ValueError('Completed reports are immutable; choose a new --output directory')
    dump(args.output/'provenance.json',provenance)
    rows = generate(args,data)
    if args.generate_only:
        return
    results,errors = [],[]
    # Reference is fully transcribed first. Failures never disappear from coverage.
    rows.sort(key=lambda r:(r['voice']!='neural',r['voice'],r['id']))
    for row in rows:
        try:
            recognition = recognize(args,{'audio':row['audio'],'audio_sha256':row['audio_sha256']})
            pcm,rate = read_audio(args.output/row['audio'])
            result = {**row,'asr':recognition,
                      'intelligibility':score(row['text'],recognition['text'],data['word_categories']),
                      'acoustic':acoustic(pcm,rate)}
            results.append(result)
            print(json.dumps({'evaluated':row['id'],'voice':row['voice'],
                              'wer':round(result['intelligibility']['wer'],3)}),flush=True)
        except (RuntimeError,ValueError,OSError,subprocess.TimeoutExpired) as exc:
            errors.append({'id':row['id'],'voice':row['voice'],'error':str(exc)})
            print(json.dumps({'failed':row['id'],'voice':row['voice'],'error':str(exc)}),flush=True)
        dump(args.output/'partial-results.json',{'rows':results,'technical_failures':errors})
    report = {'created_utc':datetime.now(timezone.utc).isoformat(),'provenance':provenance,
              'summary':summarize(results,data,errors),'rows':results}
    if args.baseline:
        report['baseline_comparison'] = compare_reports(json.loads(args.baseline.read_text()),report)
    dump(args.output/'report.json',report)
    print(json.dumps({'report':str(args.output/'report.json'),'status':report['summary']['status']}),flush=True)
    if errors:
        sys.exit(2)


if __name__ == '__main__':
    main()
