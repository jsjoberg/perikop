#!/usr/bin/env -S uv run --locked --group voice-prep
"""Prepare Alice/Björn as an offline ONNX pack; never used by the reader at runtime."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys
import time

VOICES = '2c7968d59c2fda1667e9e3ff0dd9967150a53f74'
G2P = 'a5aac876ccb2bf7480a129774c7e89cf3bbeac01'
PACK = 'kokoro-sv-alice-bjorn-2c7968d-v1'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reference-source', type=Path, help='Pinned kokoro-sv checkout for upstream pronunciation/audio comparison.')
    args = parser.parse_args()
    import numpy as np
    import torch
    from torch import nn
    import torch.nn.functional as F
    from huggingface_hub import hf_hub_download
    import onnxruntime as ort
    from kokoro import KModel
    torch.set_num_threads(4)
    torch.manual_seed(42)
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    def fetch(repo, revision, name, digest=None):
        path = Path(hf_hub_download(repo, name, revision=revision))
        if digest and hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError('Source checksum mismatch: '+name)
        return path
    model_path = fetch('Joakim/kokoro-sv-voices', VOICES, 'kokoro_sv.pth', 'c3447596e1b333eac063b8f15ab242c541392fac1f8974f0429ba473222db660')
    config = fetch('Joakim/kokoro-sv-voices', VOICES, 'config.json')
    shutil.copyfile(config, out/'config.json')
    for name, digest in [('Alice','ab0192c220ae6ebdb6a10476275c842817934d26e0ea24a088425f57d12cb259'), ('Björn','ce04503d369f4089831361812d61be3631a7b64fa6a2c22cc6c1cca92ba76210')]:
        voice = torch.load(fetch('Joakim/kokoro-sv-voices', VOICES, f'voices/{name}.pt', digest), map_location='cpu', weights_only=True)
        assert tuple(voice.shape) == (510,1,256)
        voice.numpy().astype('<f4').tofile(out/('alice.bin' if name=='Alice' else 'bjorn.bin'))
    km = KModel(repo_id='hexgrad/Kokoro-82M', config=str(config), model=str(model_path), disable_complex=True).eval()
    class Acoustic(nn.Module):
        def __init__(self):
            super().__init__(); self.model = km
        def forward(self, input_ids, ref_s):
            return self.model.forward_with_tokens(input_ids, ref_s, 1.0)
    ids = torch.tensor([[0,50,43,67,16,0]], dtype=torch.long)
    ref = torch.from_numpy(np.fromfile(out/'alice.bin', dtype='<f4').reshape(510,1,256)[3].copy())
    def export(model, inputs, filename, names, outputs, axes):
        torch.onnx.export(model, inputs, str(out/filename), input_names=names, output_names=outputs,
                          dynamic_axes=axes, opset_version=17, dynamo=False)
    print('Exporting Swedish acoustic model', flush=True)
    export(Acoustic().eval(), (ids,ref), 'kokoro.onnx', ['input_ids','ref_s'], ['audio','pred_dur'],
           {'input_ids':{1:'tokens'},'audio':{0:'samples'},'pred_dur':{0:'tokens'}})
    checkpoint = torch.load(fetch('Joakim/kokoro-sv-g2p', G2P, 'g2p/g2p_model.pt', 'a676975c4b8a618815b10db6557d13bbab692b5bb246c899d1cbe592e5737731'), map_location='cpu', weights_only=True)
    cfg = checkpoint['cfg']; size = cfg['d_model']; heads = cfg['nhead']
    # Explicit attention avoids the exporter's fixed-length MultiheadAttention
    # reshape. These operations reproduce the upstream Transformer in eval mode.
    class G2PModel(nn.Module):
        def __init__(self):
            super().__init__()
            self.src_emb = nn.Embedding(cfg['src_vocab'], size, padding_idx=0)
            self.tgt_emb = nn.Embedding(cfg['tgt_vocab'], size, padding_idx=0)
            self.pos = nn.Module(); self.pos.register_buffer('pe', checkpoint['state_dict']['pos.pe'])
            self.transformer = nn.Transformer(d_model=size,nhead=heads,num_encoder_layers=cfg['num_encoder_layers'],
                num_decoder_layers=cfg['num_decoder_layers'],dim_feedforward=cfg['dim_feedforward'],dropout=cfg['dropout'],batch_first=True)
            self.out = nn.Linear(size,cfg['tgt_vocab'])
        def attention(self, layer, q, kv, causal=False):
            wq,wk,wv = layer.in_proj_weight.chunk(3); bq,bk,bv = layer.in_proj_bias.chunk(3)
            q = F.linear(q,wq,bq).reshape(1,-1,heads,size//heads).transpose(1,2)
            k = F.linear(kv,wk,bk).reshape(1,-1,heads,size//heads).transpose(1,2)
            v = F.linear(kv,wv,bv).reshape(1,-1,heads,size//heads).transpose(1,2)
            score = (q @ k.transpose(-2,-1))/math.sqrt(size//heads)
            if causal:
                mask = torch.triu(torch.ones_like(score,dtype=torch.bool),diagonal=1)
                score = score.masked_fill(mask,float('-inf'))
            h = (score.softmax(-1) @ v).transpose(1,2).reshape(1,-1,size)
            return layer.out_proj(h)
        def encode(self, src):
            h = self.src_emb(src)*math.sqrt(size)+self.pos.pe[:,:src.shape[1]]
            for layer in self.transformer.encoder.layers:
                h = layer.norm1(h+self.attention(layer.self_attn,h,h))
                h = layer.norm2(h+layer.linear2(F.relu(layer.linear1(h))))
            return self.transformer.encoder.norm(h)
        def decode(self, tgt, memory):
            h = self.tgt_emb(tgt)*math.sqrt(size)+self.pos.pe[:,:tgt.shape[1]]
            for layer in self.transformer.decoder.layers:
                h = layer.norm1(h+self.attention(layer.self_attn,h,h,True))
                h = layer.norm2(h+self.attention(layer.multihead_attn,h,memory))
                h = layer.norm3(h+layer.linear2(F.relu(layer.linear1(h))))
            return self.out(self.transformer.decoder.norm(h)[:,-1])
    g = G2PModel().eval(); g.load_state_dict(checkpoint['state_dict'])
    class Encoder(nn.Module):
        def __init__(self): super().__init__(); self.g = g
        def forward(self, src): return self.g.encode(src)
    class Decoder(nn.Module):
        def __init__(self): super().__init__(); self.g = g
        def forward(self, tgt, memory): return self.g.decode(tgt,memory)
    src = torch.tensor([[1,4,5,6,2]],dtype=torch.long); tgt = torch.tensor([[1,4,5]],dtype=torch.long)
    memory = g.encode(src)
    print('Exporting Swedish neural pronunciation fallback', flush=True)
    export(Encoder().eval(), (src,), 'g2p-encoder.onnx', ['src'], ['memory'], {'src':{1:'letters'},'memory':{1:'letters'}})
    export(Decoder().eval(), (tgt,memory), 'g2p-decoder.onnx', ['tgt','memory'], ['logits'], {'tgt':{1:'phones'},'memory':{1:'letters'}})
    (out/'g2p-config.json').write_text(json.dumps({'cfg':cfg,'vocab':checkpoint['vocab']},ensure_ascii=False),encoding='utf-8')
    lex = fetch('Joakim/kokoro-sv-g2p', G2P, 'g2p/lexicon.tsv', '65eb3aae9c737f6d04c22a44b2ab836d1ec01f682b1cdee07bb2209852355296')
    shutil.copyfile(lex,out/'lexicon.tsv')
    # Only the pronunciation overrides ship from upstream code; the runtime
    # reproduces its documented remap and number normalization in C++.
    from urllib.request import urlopen
    root = 'https://raw.githubusercontent.com/joakimeriksson/kokoro-sv/42d1a3a5c083f405a6eb8e14c2a405ccb36cc90f/'
    for remote, local in [('g2p/custom_lexicon.tsv','custom_lexicon.tsv'),('LICENSE','LICENSE-Kokoro-Swedish.txt')]:
        (out/local).write_bytes(urlopen(root+remote).read())
    with (out/'LICENSE-Kokoro-Swedish.txt').open('ab') as license_file:
        license_file.write(b'\n'+Path(__file__).with_name('LICENSE-Apache-2.0.txt').read_bytes())
    # Check varying input lengths against the actual torch Transformer.
    options = ort.SessionOptions(); options.intra_op_num_threads=4
    enc = ort.InferenceSession(str(out/'g2p-encoder.onnx'),options,providers=['CPUExecutionProvider'])
    dec = ort.InferenceSession(str(out/'g2p-decoder.onnx'),options,providers=['CPUExecutionProvider'])
    for word in ['mose','melkisedek','sjörövaren','ängar']:
        s = torch.tensor([[1]+[checkpoint['vocab']['char2id'][c] for c in word]+[2]])
        with torch.no_grad():
            expected = g.transformer.encoder(g.src_emb(s)*math.sqrt(size)+g.pos.pe[:,:s.shape[1]])
        actual = enc.run(None,{'src':s.numpy()})[0]
        np.testing.assert_allclose(actual,expected.numpy(),atol=2e-5,rtol=2e-4)
        prefix = [1]
        for _ in range(63):
            t = torch.tensor([prefix])
            mask = nn.Transformer.generate_square_subsequent_mask(len(prefix))
            with torch.no_grad():
                h = g.transformer.decoder(g.tgt_emb(t)*math.sqrt(size)+g.pos.pe[:,:len(prefix)],expected,tgt_mask=mask)
                expected_logits = g.out(h[:,-1]).numpy()
            logits = dec.run(None,{'tgt':t.numpy(),'memory':actual})[0]
            np.testing.assert_allclose(logits,expected_logits,atol=1e-4,rtol=1e-3)
            nxt = int(logits.argmax()); assert nxt == int(expected_logits.argmax())
            if nxt==2: break
            prefix.append(nxt)
    loaded_at = time.perf_counter()
    acoustic = ort.InferenceSession(str(out/'kokoro.onnx'), options,providers=['CPUExecutionProvider'])
    acoustic_load = time.perf_counter()-loaded_at
    for n in [4,9,35]:
        inputs = np.array([[0]+[50,43,67,16,44,55,16,50,43]*(n//9)+[50]*(n%9)+[0]],dtype=np.int64)
        audio,durations = acoustic.run(None,{'input_ids':inputs,'ref_s':ref.numpy()})
        assert audio.size>1000 and np.isfinite(audio).all() and np.max(np.abs(audio))>0.0001
        assert durations.size == inputs.size
    if args.reference_source:
        import subprocess
        revision = subprocess.check_output(['git','-C',str(args.reference_source),'rev-parse','HEAD'],text=True).strip()
        if revision != '42d1a3a5c083f405a6eb8e14c2a405ccb36cc90f':
            raise RuntimeError('Reference checkout revision differs from the pinned source.')
        sys.path.insert(0,str(args.reference_source.resolve()/'g2p'))
        sys.path.insert(0,str(args.reference_source.resolve()))
        from g2p_infer import SwedishG2P
        from g2p_sv import normalize_numbers, _apply_fixes, _apply_remap, KOKORO_REMAP
        from scipy.signal import iirnotch, filtfilt
        import soundfile as sf
        frontend = SwedishG2P(model_path=fetch('Joakim/kokoro-sv-g2p',G2P,'g2p/g2p_model.pt'),
            lexicon_path=out/'lexicon.tsv',custom_path=out/'custom_lexicon.tsv')
        samples_dir = out.parent/'kokoro-audition'; samples_dir.mkdir(exist_ok=True)
        timings = {'acoustic_load_seconds':acoustic_load,'samples':[]}
        for text in ['Mose.', 'Herren är min herde, mig skall intet fattas. Han låter mig vila på gröna ängar.']:
            started = time.perf_counter()
            ipa = ' '.join(''.join(s) for s in frontend.phonemize(normalize_numbers(text)))
            ipa = _apply_remap(_apply_fixes(ipa),KOKORO_REMAP)
            tokens = [km.vocab[c] for c in ipa if c in km.vocab]
            frontend_seconds = time.perf_counter()-started
            for voice in ['alice','bjorn']:
                style = np.fromfile(out/(voice+'.bin'),dtype='<f4').reshape(510,1,256)[len(tokens)-1].copy()
                inputs = np.array([[0]+tokens+[0]],dtype=np.int64)
                started = time.perf_counter()
                audio,durations = acoustic.run(None,{'input_ids':inputs,'ref_s':style})
                for f in [2400,4800,7200,9600]:
                    b,a = iirnotch(f,Q=35,fs=24000); audio=filtfilt(b,a,audio)
                generated = time.perf_counter()-started
                filename = voice+('-word.wav' if text=='Mose.' else '-psalm.wav')
                sf.write(samples_dir/filename,audio,24000)
                timings['samples'].append({'voice':voice,'text':text,'ipa':ipa,'frontend_seconds':frontend_seconds,
                    'generation_seconds':generated,'audio_seconds':audio.size/24000})
                with torch.no_grad():
                    _, expected_durations = km.forward_with_tokens(torch.from_numpy(inputs),torch.from_numpy(style),1.0)
                np.testing.assert_array_equal(durations,expected_durations.numpy())
        (samples_dir/'timings.json').write_text(json.dumps(timings,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    files = {p.name:{'size':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in out.iterdir() if p.is_file() and p.name!='voice-pack.json'}
    (out/'voice-pack.json').write_text(json.dumps({'id':PACK,'licence':'Apache-2.0','sources':{'voices':VOICES,'g2p':G2P,'code':'42d1a3a5c083f405a6eb8e14c2a405ccb36cc90f'},'files':files},indent=2)+'\n')
    print('Prepared and validated:',out,flush=True)

if __name__=='__main__': main()
