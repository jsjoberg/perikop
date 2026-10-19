#!/usr/bin/env python3
"""Explicit one-time download of pinned KB-Whisper Small GGML weights (Apache-2.0)."""
import argparse
import hashlib
from pathlib import Path
import urllib.request

REVISION = '3564d61a42fc210ceaa55a22a96dd64478959c78'
SHA256 = 'de6911330cbdc131362f7a955682b65c8a5a2394caba73e7ea821a9822efb8c6'
URL = f'https://huggingface.co/KBLab/kb-whisper-small/resolve/{REVISION}/ggml-model.bin'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    args = parser.parse_args()
    if args.output.exists():
        if hashlib.sha256(args.output.read_bytes()).hexdigest()!=SHA256:
            raise ValueError('Existing file has different contents; choose another destination')
        print(args.output)
        return
    args.output.parent.mkdir(parents=True,exist_ok=True)
    temporary = args.output.with_suffix('.download')
    urllib.request.urlretrieve(URL,temporary)
    if hashlib.sha256(temporary.read_bytes()).hexdigest()!=SHA256:
        temporary.unlink()
        raise ValueError('Model checksum mismatch')
    temporary.replace(args.output)
    print(args.output)


if __name__ == '__main__':
    main()
