#!/usr/bin/env python3
"""Package lossless, downloadable comparison audio using macOS afconvert (MIT)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import wave


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path("build/bjorn-poc"))
    parser.add_argument("--output", type=Path, default=Path("docs/reports/bjorn-poc-samples"))
    parser.add_argument("--compact-probe", type=Path, help="Add real text-to-speech instead of the 32-harmonic variant.")
    parser.add_argument("--compact-pack", type=Path, default=Path("resources/voices/bjorn-lite-v1"))
    args = parser.parse_args()
    report = json.loads((args.input / "report.json").read_text())
    args.output.mkdir(parents=True, exist_ok=True)
    clips = []
    for sample in report["samples"]:
        compact = None
        if args.compact_probe:
            wav = args.input / f'{sample["name"]}-compact.wav'
            result = subprocess.run([str(args.compact_probe), str(args.compact_pack), str(wav), sample["text"]],
                                    check=True, capture_output=True, text=True)
            compact = json.loads(result.stdout)
            sequence = [("Björn reference", sample["reference"])] + [
                ("First harmonic prototype", variant["wav"]) for variant in sample["variants"] if variant["harmonics"] == 64]
            sequence.append(("Björn Bas: text-to-speech", wav.name))
        else:
            sequence = [("Björn reference", sample["reference"])] + [
                (f'{variant["harmonics"]} harmonics', variant["wav"]) for variant in sample["variants"]]
        with tempfile.TemporaryDirectory() as temporary:
            wav = Path(temporary) / "comparison.wav"
            starts = []
            with wave.open(str(wav), "wb") as output:
                output.setparams((1, 2, report["sample_rate"], 0, "NONE", "not compressed"))
                for index, (label, name) in enumerate(sequence):
                    if index:
                        output.writeframes(bytes(round(report["sample_rate"] * 0.75) * 2))
                    starts.append({"label": label, "start_seconds": output.getnframes() / report["sample_rate"]})
                    with wave.open(str(args.input / name), "rb") as source:
                        if (source.getnchannels(), source.getsampwidth(), source.getframerate()) != (1, 2, report["sample_rate"]):
                            raise ValueError(f"Unexpected WAV format: {name}")
                        output.writeframes(source.readframes(source.getnframes()))
            destination = args.output / f'{sample["name"]}-comparison.m4a'
            subprocess.run(["/usr/bin/afconvert", "-f", "m4af", "-d", "alac", str(wav), str(destination)], check=True)
            decoded = Path(temporary) / "decoded.wav"
            subprocess.run(["/usr/bin/afconvert", "-f", "WAVE", "-d", "LEI16", str(destination), str(decoded)], check=True)
            with wave.open(str(wav), "rb") as a, wave.open(str(decoded), "rb") as b:
                if a.getnframes() != b.getnframes() or a.readframes(a.getnframes()) != b.readframes(b.getnframes()):
                    raise ValueError("Lossless comparison round trip changed the samples.")
            clips.append({"sample": sample["name"], "file": destination.name, "segments": starts,
                          "bytes": destination.stat().st_size,
                          "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(), "compact_render": compact})
    details = {"audio": clips, "experiment": report}
    if args.compact_probe:
        manifest = json.loads((args.compact_pack / "voice-pack.json").read_text())
        details["compact_voice"] = {key: manifest[key] for key in ("revision", "phonemes", "units", "files", "selection")}
    (args.output / "report.json").write_text(json.dumps(details, ensure_ascii=False, indent=2) + "\n")
    (args.output / "LICENSE-Kokoro-Swedish.txt").write_bytes((args.input / "LICENSE-Kokoro-Swedish.txt").read_bytes())
    rows = []
    for clip in clips:
        times = ", ".join(f'{segment["label"]}: {segment["start_seconds"]:.2f} s' for segment in clip["segments"])
        rows.append(f'- [{clip["sample"]} comparison]({clip["file"]}): {times}.')
    order = "Björn reference → first harmonic prototype → Björn Bas text-to-speech" if args.compact_probe else "Björn reference → 32 harmonics → 64 harmonics"
    scope = ("The last segment is synthesized from text using reusable units, not fitted to the complete recording. "
             "Its duration can differ from the reference. Voice quality still needs listening review.") if args.compact_probe else (
             "These are complete-utterance reconstructions, not a reusable text-to-speech voice. The samples have not been approved by listening.")
    (args.output / "README.md").write_text("""# Björn listening samples

Each lossless Apple Lossless / M4A file contains **""" + order + """**, separated by 0.75 seconds of silence. The PCM is verified identical
after encoding and decoding. Download the files to listen in a local audio player;
these links do not require the development machine or tailnet server.

""" + "\n".join(rows) + """

""" + scope + """ No eSpeak or GPL speech code was
used. The original renderer and fitting tools are MIT; the Björn teacher pack is
marked Apache-2.0 and its notices are included here. Source revisions, hashes and
measurements are in [report.json](report.json).

To regenerate on macOS after running the fitter:

```sh
python3 tools/speech/bjorn-poc/package_samples.py --compact-probe build/cmake/bin/ortho-compact-voice-probe
```

The encoder is Apple's built-in `afconvert`. It is only used to package this
audition, and is not a dependency of the native synthesizer.
""", encoding="utf-8")
    print(json.dumps(clips, indent=2))


if __name__ == "__main__":
    main()
