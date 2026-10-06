#!/usr/bin/env -S uv run --locked
"""Install or verify the offline Swedish voice pack. Python is an installation tool only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

KOKORO_ID = 'kokoro-sv-alice-bjorn-2c7968d-v1'
KOKORO_FILES = {'kokoro.onnx', 'g2p-encoder.onnx', 'g2p-decoder.onnx',
                'g2p-config.json', 'config.json', 'alice.bin', 'bjorn.bin',
                'lexicon.tsv', 'custom_lexicon.tsv', 'LICENSE-Kokoro-Swedish.txt'}

def default_data_directory():
    if sys.platform == 'darwin':
        return Path.home() / 'Library/Application Support/orthodox-reader'
    if os.name == 'nt':
        return Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData/Local')) / 'orthodox-reader'
    return Path.home() / '.orthodox-reader'

def verify_file(path, spec):
    if not path.is_file() or path.stat().st_size != spec['size']:
        return False
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest() == spec['sha256']

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path, default=default_data_directory())
    parser.add_argument('--pack', choices=['kokoro'], default='kokoro', help='The Alice and Björn pack, the only voice pack.')
    parser.add_argument('--source', type=Path, help='The unpacked pack from tools/speech/prepare_kokoro.py.')
    parser.add_argument('--verify', action='store_true', help='Verify the installed pack.')
    args = parser.parse_args()
    voices = args.data_dir / 'voices'
    manifest_path = (args.source if args.source else voices / KOKORO_ID) / 'voice-pack.json'
    if not manifest_path.is_file():
        raise RuntimeError('Prepare the pack with tools/speech/prepare_kokoro.py, then install it with --source PATH.')
    manifest = json.loads(manifest_path.read_text())
    if manifest['id'] != KOKORO_ID or set(manifest['files']) != KOKORO_FILES:
        raise RuntimeError('Unexpected voice pack identity or files.')
    destination = voices / manifest['id']
    if args.verify or destination.exists():
        for name, spec in manifest['files'].items():
            if not verify_file(destination / name, spec):
                raise RuntimeError(f'Installed voice file failed verification: {name}')
        print(f'All {len(manifest["files"])} voice files verified: {destination}')
        return
    voices.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='voice-install-', dir=voices) as temporary:
        staged = Path(temporary) / manifest['id']
        staged.mkdir()
        for name, spec in manifest['files'].items():
            target = staged / name
            shutil.copyfile(args.source / name, target)
            if not verify_file(target, spec):
                raise RuntimeError(f'Offline voice file failed verification: {name}')
        shutil.copyfile(manifest_path, staged / 'voice-pack.json')
        # Every file passes before the application can see the new directory.
        staged.rename(destination)
    print(f'Installed voice pack: {destination}')

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
