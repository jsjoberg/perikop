#!/usr/bin/env python3
"""Install or verify a pinned, offline voice pack. Python is an installation tool only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import time
import urllib.request

MANIFEST = Path(__file__).with_name('voice-pack.json')

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

def download(url, destination, spec):
    for attempt in range(5):
        try:
            with urllib.request.urlopen(url, timeout=60) as response, destination.open('wb') as stream:
                shutil.copyfileobj(response, stream, length=1024 * 1024)
            if verify_file(destination, spec):
                return
        except (OSError, ValueError) as error:
            print(f'Download retry: {error}', file=sys.stderr)
        time.sleep(min(2 ** attempt, 8))
    raise RuntimeError(f'Download verification failed: {destination.name}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data-dir', type=Path, default=default_data_directory())
    parser.add_argument('--source', type=Path, help='Use an unpacked offline voice pack instead of the network.')
    parser.add_argument('--verify', action='store_true', help='Verify the installed pack without downloading anything.')
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    voices = args.data_dir / 'voices'
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
            target.parent.mkdir(parents=True, exist_ok=True)
            if args.source:
                shutil.copyfile(args.source / name, target)
                if not verify_file(target, spec):
                    raise RuntimeError(f'Offline voice file failed verification: {name}')
            else:
                url = f'https://huggingface.co/{manifest["repository"]}/resolve/{manifest["revision"]}/{name}?download=true'
                print(f'Downloading {name} ({spec["size"]:,} bytes)', flush=True)
                download(url, target, spec)
        shutil.copyfile(MANIFEST, staged / 'voice-pack.json')
        shutil.copyfile(Path(__file__).with_name('LICENSE-Chatterbox.txt'), staged / 'LICENSE-Chatterbox.txt')
        # Every file passes before the application can see the new directory.
        staged.rename(destination)
    print(f'Installed voice pack: {destination}')

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
