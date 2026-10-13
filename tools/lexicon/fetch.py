#!/usr/bin/env -S uv run --locked
"""Provide pinned lexical inputs in build/inputs.

Most inputs are committed xz-compressed in resources/lexicon/input; the rest are downloaded.
"""
import hashlib, json, lzma, shutil, sys, urllib.request
from pathlib import Path
root=Path(__file__).resolve().parents[2]
target=root/'build/inputs'
committed=root/'resources/lexicon/input'
def verify(path,sha256):
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    if digest!=sha256:raise SystemExit(f'Hash mismatch for {path.name}: {digest}')
def fetch(name):
    entry=json.loads((Path(__file__).parent/'inputs.json').read_text(encoding='utf-8'))[name]
    path=target/entry.get('file',entry['url'].rsplit('/',1)[1])
    if path.exists():
        verify(path,entry['sha256'])
        return path
    target.mkdir(parents=True,exist_ok=True)
    # Only a verified file gets the final name, so an interrupted one is provided again.
    part=path.with_name(path.name+'.part')
    if (compressed:=committed/(path.name+'.xz')).exists():
        with lzma.open(compressed) as source,part.open('wb') as output:shutil.copyfileobj(source,output)
    else:
        urllib.request.urlretrieve(entry['url'],part)
    verify(part,entry['sha256'])
    part.replace(path)
    return path
if __name__=='__main__':
    for name in sys.argv[1:] or json.loads((Path(__file__).parent/'inputs.json').read_text(encoding='utf-8')):print(fetch(name))
