#!/usr/bin/env -S uv run --locked
"""Download pinned lexical inputs into build/inputs; they are not committed."""
import hashlib, json, sys, urllib.request
from pathlib import Path
root=Path(__file__).resolve().parents[2]
target=root/'build/inputs'
def fetch(name):
    entry=json.loads((Path(__file__).parent/'inputs.json').read_text())[name]
    path=target/entry.get('file',entry['url'].rsplit('/',1)[1])
    if not path.exists():
        target.mkdir(parents=True,exist_ok=True)
        urllib.request.urlretrieve(entry['url'],path)
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    if digest!=entry['sha256']:raise SystemExit(f'Hash mismatch for {path.name}: {digest}')
    return path
if __name__=='__main__':
    for name in sys.argv[1:] or json.loads((Path(__file__).parent/'inputs.json').read_text()):print(fetch(name))
