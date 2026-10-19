#!/usr/bin/env python3
"""Compare two immutable evaluation reports without re-running ASR (MIT)."""
import argparse
import json
from pathlib import Path
from run import compare_reports

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('baseline',type=Path)
parser.add_argument('candidate',type=Path)
parser.add_argument('--output',type=Path)
args = parser.parse_args()
result = compare_reports(json.loads(args.baseline.read_text()),json.loads(args.candidate.read_text()))
text = json.dumps(result,ensure_ascii=False,indent=2)+'\n'
if args.output:
    if args.output.exists():
        raise ValueError('Comparison output already exists; choose a new path')
    args.output.write_text(text)
print(text)
