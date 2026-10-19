#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy==2.3.5"]
# ///
"""Build reusable Swedish waveform units from Bjorn, offline (original MIT code)."""
import argparse
from collections import Counter
import hashlib
import heapq
import json
from pathlib import Path
import shutil
import struct
import subprocess

import numpy as np

REMAP = {"ʏ": "y", "ʉ": "ɨ", "ɵ": "ɜ", "ɧ": "ʂ", "ɭ": "l", "-": "", "\u0303": ""}
STRESS = "ˈˌ"
PUNCTUATION = ".,;:!?"


def remap(ipa):
    return "".join(REMAP.get(c, c) for c in ipa if not c.isspace())


def phones(ipa):
    result, stress = [], ""
    for c in remap(ipa):
        if c in STRESS:
            stress = c
        elif c == "ː" and result:
            result[-1] += c
        elif c not in PUNCTUATION:
            result.append(stress + c)
            stress = ""
    return result


def dictionary(pack, corrections):
    entries = {}
    for filename in ("g2p-corpus.tsv", "lexicon.tsv", "custom_lexicon.tsv"):
        for row in (pack / filename).read_text().splitlines():
            if not row or row.startswith("#"):
                continue
            word, ipa = row.split("\t")[:2]
            entries[word] = "".join(ipa.split())
    for row in corrections.read_text().splitlines():
        if not row or row.startswith("#"):
            continue
        columns = row.split("\t")
        word, ipa = columns[:2]
        if not word.isalpha():
            continue  # Phrase overrides are applied by the app's pronunciation matcher.
        entries[word.lower()] = ipa
        if len(columns) < 3 or columns[2] != "word-only":
            entries.setdefault(word.lower() + "s", ipa + "s")
    # Same two documented word fixes as KokoroText::ipa, before symbol remapping.
    return {word: ipa.replace("ˈuːɕˌɛj", "ˈuːkɛj").replace("uːəsˈɛs", "ˈɔs")
            for word, ipa in entries.items()}


def select_words(entries, maximum):
    pairs = {}
    for word, ipa in entries.items():
        sequence = ["_"] + phones(ipa) + ["_"]
        if not word.isalpha() or not sequence[1:-1] or len(sequence) > 35:
            continue
        pairs[word] = set(zip(sequence, sequence[1:]))
    missing = set().union(*pairs.values())
    original = len(missing)
    heap = [(-len(value), len(word), word) for word, value in pairs.items()]
    heapq.heapify(heap)
    selected = []
    while missing and heap and len(selected) < maximum:
        previous, _, word = heapq.heappop(heap)
        score = len(pairs[word] & missing)
        if not score:
            continue
        if score != -previous:
            heapq.heappush(heap, (-score, len(word), word))
            continue
        selected.append(word)
        missing -= pairs[word]
    return selected, {"dictionary_pairs": original, "selected_words": len(selected), "uncovered_pairs": len(missing)}


def aligned_phones(tokens, durations, symbols, sample_count, offset=0):
    total = sum(durations)
    if len(tokens) != len(durations) or not total or any(v <= 0 for v in durations):
        raise ValueError("Invalid teacher alignment.")
    result, cursor, stress, pending = [], 0, "", None
    scale = sample_count / total
    for token, duration in zip(tokens, durations):
        begin, end = (max(0,min(sample_count,round(t*scale)+offset)) for t in (cursor,cursor+duration))
        cursor += duration
        symbol = symbols.get(token, "_") if token else "_"
        if symbol in STRESS:
            stress, pending = symbol, begin
        elif symbol == "ː" and result:
            result[-1][0] += symbol
            result[-1][2] = end
        elif symbol == " ":
            if result:
                result[-1][2] = end
        else:
            label = "_" if symbol == "_" or symbol in PUNCTUATION else stress + symbol
            start = pending if pending is not None else begin
            if label == "_" and result and result[-1][0] == "_":
                result[-1][2] = end
            else:
                result.append([label, start, end])
            stress, pending = "", None
    return result


def encode_wave(samples):
    compressed = np.sign(samples) * np.log1p(255 * np.minimum(1, np.abs(samples))) / np.log(256)
    return np.rint((compressed + 1) * 127.5).astype("u1").tobytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", type=Path, default=Path("build/kokoro-pack"))
    parser.add_argument("--probe", type=Path, default=Path("build/cmake/bin/ortho-kokoro-audio-probe"))
    parser.add_argument("--output", type=Path, default=Path("resources/voices/bjorn-lite-v1"))
    parser.add_argument("--work", type=Path, default=Path("build/bjorn-units"))
    parser.add_argument("--max-words", type=int, default=700)
    parser.add_argument("--alignment-offset", type=int, default=0,
                        help="Experimental acoustic correction in samples; default preserves existing pack")
    args = parser.parse_args()
    entries = dictionary(args.pack, Path("resources/corpus/pronunciation-sv.tsv"))
    vocab = json.loads((args.pack / "config.json").read_text())["vocab"]
    symbols = {token: symbol for symbol, token in vocab.items()}
    words, selection = select_words(entries, args.max_words)
    print(json.dumps(selection), flush=True)
    batches, batch, count = [], [], 0
    for word in words:
        size = len(remap(entries[word])) + 1
        if batch and count + size > 340:
            batches.append(batch)
            batch, count = [], 0
        batch.append(word)
        count += size
    if batch:
        batches.append(batch)
    args.output.mkdir(parents=True, exist_ok=True)
    args.work.mkdir(parents=True, exist_ok=True)
    units, unit_scores, lengths, training = {}, {}, {}, []
    guard = 240
    for number, batch in enumerate(batches):
        prefix = args.work / f"batch-{number:03}"
        tokens = [vocab[c] for word in batch for c in remap(entries[word]) + " " if c in vocab]
        tokens.append(vocab["."])
        token_file, raw, timing = prefix.with_suffix(".tokens"), prefix.with_suffix(".f32"), prefix.with_suffix(".durations")
        token_file.write_text(" ".join(map(str, tokens)))
        # Reuse exact cached references when preparing again. Their inputs must match.
        identity = hashlib.sha256(token_file.read_bytes() + (args.pack / "voice-pack.json").read_bytes()).hexdigest()
        key = prefix.with_suffix(".sha256")
        if not (raw.exists() and timing.exists() and key.exists() and key.read_text() == identity):
            subprocess.run([str(args.probe), str(args.pack), str(token_file), "bjorn", str(raw), str(timing)],
                           check=True, capture_output=True, text=True)
            key.write_text(identity)
        pcm = np.fromfile(raw, dtype="<f4").astype(float)
        if not len(pcm) or not np.isfinite(pcm).all() or np.max(np.abs(pcm)) > 5:
            raise ValueError("Invalid teacher waveform.")
        peak = float(np.max(np.abs(pcm)))
        gain = min(1.0, 0.85 / max(peak, 1e-9))
        pcm *= gain
        durations = [int(value) for value in timing.read_text().split()]
        aligned = aligned_phones([0] + tokens + [0], durations, symbols, len(pcm), args.alignment_offset)
        for label, first, last in aligned:
            if label != "_":
                lengths.setdefault(label, []).append(last - first)
            begin, end = max(0, first - guard), min(len(pcm), last + guard)
            if label != "_" and end - begin > 48:
                score = -abs((last - first) / 24000 - 0.12)
                unit_key = ("*", label)
                if score > unit_scores.get(unit_key, -1e9):
                    units[unit_key] = (pcm[begin:end], first - begin, end - last)
                    unit_scores[unit_key] = score
        for left, right in zip(aligned, aligned[1:]):
            if left[0] == "_" or right[0] == "_":
                continue
            first, last = (left[1] + left[2]) // 2, (right[1] + right[2]) // 2
            begin, end = max(0, first - guard), min(len(pcm), last + guard)
            if not 120 < last - first < 16000:
                continue
            unit_key = (left[0], right[0])
            # Prefer ordinary durations and avoid low-energy/misaligned fragments.
            score = -abs((last - first) / 24000 - 0.14)
            if score > unit_scores.get(unit_key, -1e9):
                units[unit_key] = (pcm[begin:end], first - begin, end - last)
                unit_scores[unit_key] = score
        training.append({"words": batch, "tokens_sha256": identity,
                         "pcm_sha256": hashlib.sha256(raw.read_bytes()).hexdigest(), "samples": len(pcm),
                         "duration_scale_samples": len(pcm) / sum(durations), "teacher_peak": peak,
                         "normalization_gain": gain})
        print(f"{number + 1}/{len(batches)}: {len(units)} reusable units", flush=True)
    labels = sorted(lengths)
    required = set(phone for ipa in entries.values() for phone in phones(ipa))
    missing = required - set(labels)
    if missing:
        raise ValueError(f"Training is missing phonemes: {sorted(missing)}")
    ids = {label: index for index, label in enumerate(labels)}
    with (args.output / "units.bin").open("wb") as output:
        output.write(struct.pack("<4s3I", b"BJU1", 24000, len(labels), len(units)))
        for (left, right), (samples, lead, tail) in sorted(units.items()):
            output.write(struct.pack("<HHIHH", 65535 if left == "*" else ids[left], ids[right], len(samples), lead, tail))
            output.write(encode_wave(samples))
    (args.output / "phones.tsv").write_text("".join(f"{ids[label]}\t{label}\t{round(float(np.median(lengths[label])))}\n" for label in labels), encoding="utf-8")
    (args.output / "lexicon.tsv").write_text("".join(f"{word}\t{ipa}\n" for word, ipa in sorted(entries.items())), encoding="utf-8")
    shutil.copyfile(args.pack / "LICENSE-Kokoro-Swedish.txt", args.output / "LICENSE-Kokoro-Swedish.txt")
    digest = hashlib.sha256(b"".join((args.output / name).read_bytes() for name in ("units.bin", "phones.tsv", "lexicon.tsv"))).hexdigest()
    (args.output / "revision.txt").write_text(digest + "\n")
    manifest = {"id": "bjorn-lite-v1", "revision": digest, "method": "reusable diphone/monophone waveform units; mu=255 companding",
                "alignment_offset_samples":args.alignment_offset,
                "teacher": json.loads((args.pack / "voice-pack.json").read_text()), "selection": selection,
                "phonemes": len(labels), "units": len(units), "training": training,
                "files": {name: {"bytes": (args.output / name).stat().st_size,
                                  "sha256": hashlib.sha256((args.output / name).read_bytes()).hexdigest()}
                          for name in ("units.bin", "phones.tsv", "lexicon.tsv")}}
    (args.output / "voice-pack.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    root = Path(__file__).resolve().parents[3]
    if args.output.resolve() == root / "resources/voices/bjorn-lite-v1":
        path = root / "resources/manifest.json"
        resources = json.loads(path.read_text())
        prefix = "resources/voices/bjorn-lite-v1/"
        resources["assets"] = [asset for asset in resources["assets"] if not asset.get("path", "").startswith(prefix)]
        for file in sorted(args.output.iterdir()):
            if file.is_file():
                resources["assets"].append({"path": prefix + file.name,
                    "url": "https://huggingface.co/Joakim/kokoro-sv-voices/tree/2c7968d59c2fda1667e9e3ff0dd9967150a53f74",
                    "sha256": hashlib.sha256(file.read_bytes()).hexdigest(), "license": "Apache-2.0",
                    "role": "Generated Björn Bas waveform voice; preparation and provenance in voice-pack.json"})
        path.write_text(json.dumps(resources, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Voice ready: {args.output}, {len(units)} units, {sum(p.stat().st_size for p in args.output.iterdir())} bytes", flush=True)


if __name__ == "__main__":
    main()
