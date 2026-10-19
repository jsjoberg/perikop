#!/usr/bin/env python3
"""Freeze unquantized FP32 Swedish Kokoro master audio (offline evaluation, MIT)."""
import argparse
from collections import Counter
import importlib.metadata
import json
from pathlib import Path
import time

import numpy as np

from metrics import write_audio
from run import digest,dump,identity


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--checkpoint',type=Path,required=True)
    p.add_argument('--config',type=Path,required=True)
    p.add_argument('--voice',type=Path,required=True)
    p.add_argument('--token-references',type=Path,required=True)
    p.add_argument('--dataset',type=Path,default=Path(__file__).with_name('dataset.json'))
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--split',choices=['dev','heldout','all'],default='dev')
    p.add_argument('--ids',nargs='+')
    p.add_argument('--seed',type=int,default=42)
    args=p.parse_args()
    import torch
    from scipy.signal import iirnotch,filtfilt
    from kokoro import KModel
    torch.set_num_threads(4);torch.set_num_interop_threads(1)
    torch.use_deterministic_algorithms(True)
    pinned='c3447596e1b333eac063b8f15ab242c541392fac1f8974f0429ba473222db660'
    if digest(args.checkpoint)!=pinned:raise ValueError('Wrong Swedish master checkpoint')
    state=torch.load(args.checkpoint,map_location='cpu',weights_only=True)
    dtypes=Counter(str(t.dtype) for group in state.values() for t in group.values() if torch.is_tensor(t))
    if any(t.is_floating_point() and t.dtype!=torch.float32 for g in state.values() for t in g.values() if torch.is_tensor(t)):
        raise ValueError('Master weights must be FP32')
    del state
    model=KModel(repo_id='hexgrad/Kokoro-82M',config=str(args.config),model=str(args.checkpoint),disable_complex=False).eval()
    if any(t.dtype!=torch.float32 for t in model.parameters()):raise ValueError('Non-FP32 inference parameters')
    voice=torch.load(args.voice,map_location='cpu',weights_only=True)
    if tuple(voice.shape)!=(510,1,256) or voice.dtype!=torch.float32:raise ValueError('Invalid original voice tensor')
    args.output.mkdir(parents=True,exist_ok=True)
    provenance={'kind':'Original unquantized FP32 PyTorch master; complex ISTFT enabled; CPU; no autocast',
        'checkpoint_sha256':pinned,'checkpoint_bytes':args.checkpoint.stat().st_size,'checkpoint_dtypes':dict(dtypes),
        'source_revision':'2c7968d59c2fda1667e9e3ff0dd9967150a53f74',
        'config_sha256':digest(args.config),'voice_sha256':digest(args.voice),'voice_name':args.voice.stem,
        'code_sha256':digest(Path(__file__)),'torch':torch.__version__,'kokoro':importlib.metadata.version('kokoro'),
        'numpy':np.__version__,'scipy':importlib.metadata.version('scipy'),'seed':args.seed,
        'postprocess':'Upstream production iirnotch Q35 + filtfilt at 2400/4800/7200/9600 Hz. Raw float32 also retained.',
        'tokens':'Exact existing token sequences, not re-predicted pronunciation or compressed acoustic output'}
    passages=json.loads(args.dataset.read_text())['passages'];rows=[]
    if args.ids and not set(args.ids)<={r['id'] for r in passages}:raise ValueError('Unknown passage')
    for row in passages:
        if args.ids and row['id'] not in args.ids:continue
        if args.split!='all' and row['split']!=args.split:continue
        receipt=args.token_references/(row['id']+'.generation.json')
        metadata=json.loads(receipt.read_text())['metadata']
        key=identity([provenance,digest(receipt),row])
        wav=args.output/(row['id']+'.wav');saved=wav.with_suffix('.generation.json')
        if saved.exists():
            prior=json.loads(saved.read_text())
            if prior['key']!=key or prior['sha256']!=digest(wav):raise ValueError('Stale master generation')
            rows.append(prior['row']);continue
        start=time.perf_counter();audio=[];chunks=[]
        torch.manual_seed(args.seed)
        with torch.inference_mode():
            for chunk in metadata['chunks']:
                tokens=chunk['tokens'];ids=torch.tensor([[0,*tokens,0]],dtype=torch.long)
                raw,durations=model.forward_with_tokens(ids,voice[len(tokens)-1],1.)
                raw=raw.cpu().numpy().astype('<f4');audio.append(raw)
                chunks.append({'tokens':tokens,'predicted_token_durations':durations.tolist(),'samples':len(raw)})
        raw=np.concatenate(audio);raw.tofile(wav.with_suffix('.raw.f32'))
        filtered=raw.astype(float)
        for frequency in (2400,4800,7200,9600):
            b,a=iirnotch(frequency,Q=35,fs=24000);filtered=filtfilt(b,a,filtered)
        write_audio(wav,np.clip(filtered,-1,1),24000)
        result={**row,'audio':wav.name,'audio_sha256':digest(wav),'chunks':chunks,'seconds':len(raw)/24000,
                'raw_sha256':digest(wav.with_suffix('.raw.f32')),'float_overload_samples':int(np.sum(np.abs(filtered)>1)),
                'generation_seconds':time.perf_counter()-start,'token_receipt_sha256':digest(receipt)}
        dump(saved,{'key':key,'sha256':digest(wav),'row':result});rows.append(result)
        dump(args.output/'master.json',{'provenance':provenance,'passages':rows})
        print(json.dumps({'master':row['id'],'seconds':result['seconds'],'generation_seconds':result['generation_seconds']}),flush=True)
    dump(args.output/'master.json',{'provenance':provenance,'passages':rows})


if __name__=='__main__':main()
