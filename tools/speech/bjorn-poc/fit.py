#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = ["numpy==2.3.5"]
# ///
"""Fit Bjorn utterances to a small harmonic + noise synthesizer (MIT).

The only analysis dependency is NumPy (BSD-3-Clause). The C renderer has none.
No eSpeak, GPL DSP, speech aligner, or neural vocoder is used.
"""
from __future__ import annotations

import argparse
import hashlib
import html
import json
from pathlib import Path
import struct
import subprocess
import time
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
RATE, HOP, WINDOW, FFT = 24000, 240, 960, 2048
EDGES = np.array([0, 100, 200, 300, 450, 650, 900, 1250, 1700,
                  2300, 3100, 4100, 5400, 7000, 9000, 10500, 12000])
SAMPLES = [
    ("psalm", "Herren är min herde, mig skall intet fattas. Han låter mig vila på gröna ängar."),
    ("sjal", "Han vederkvicker min själ; han leder mig på rätta vägar, för sitt namns skull."),
    ("namn", "Abraham, Isak och Jakob. Jerusalem och Betlehem. Mose och Aron."),
]


def run(*args, **kwargs):
    return subprocess.run([str(arg) for arg in args], check=True, capture_output=True,
                          text=True, **kwargs).stdout


def read_wav(path):
    with wave.open(str(path), "rb") as source:
        if (source.getnchannels(), source.getsampwidth(), source.getframerate()) != (1, 2, RATE):
            raise ValueError("Reference must be mono, 16-bit PCM, 24000 Hz.")
        if not 0 < source.getnframes() <= RATE * 600:
            raise ValueError("Reference must contain between 0 and 600 seconds of audio.")
        return np.frombuffer(source.readframes(source.getnframes()), dtype="<i2").astype(float) / 32768


def write_wav(path, samples):
    if not len(samples) or not np.isfinite(samples).all() or np.max(np.abs(samples)) > 1:
        raise ValueError("Invalid or clipping reference samples.")
    with wave.open(str(path), "wb") as output:
        output.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
        output.writeframes(np.rint(samples * 32767).astype("<i2").tobytes())


def centered(samples, center, length):
    begin, end = center - length // 2, center + length // 2
    result = np.zeros(length)
    first, last = max(begin, 0), min(end, len(samples))
    if last > first:
        result[first - begin:last - begin] = samples[first:last]
    return result


def fit_window(sample_count, frame):
    positions = frame * HOP + np.arange(WINDOW) - WINDOW // 2
    return np.hanning(WINDOW) * ((positions >= 0) & (positions < sample_count))


def pitch_frame(frame):
    """YIN-style normalized difference, evaluated with FFT autocorrelation.

    Implemented here from the difference-function definition. No borrowed DSP code.
    Search 60--400 Hz; return zero for silence/aperiodic frames.
    """
    frame = frame - np.mean(frame)
    if np.mean(frame * frame) < 1e-7:
        return 0.0
    corr = np.fft.irfft(np.abs(np.fft.rfft(frame, 4096)) ** 2, 4096)
    square = np.r_[0, np.cumsum(frame * frame)]
    lags = np.arange(1, RATE // 60 + 2)
    difference = np.maximum(0, square[len(frame) - lags] + square[-1] - square[lags] - 2 * corr[lags])
    normalized = np.r_[1.0, difference * lags / np.maximum(np.cumsum(difference), 1e-20)]
    low, high = RATE // 400, RATE // 60
    valleys = [i for i in range(low, high) if normalized[i] <= normalized[i - 1]
               and normalized[i] < normalized[i + 1]]
    if not valleys:
        return 0.0
    strong = [i for i in valleys if normalized[i] < 0.18]
    lag = strong[0] if strong else min(valleys, key=lambda i: normalized[i])
    if normalized[lag] > 0.38:
        return 0.0
    left, middle, right = normalized[lag - 1:lag + 2]
    curvature = left - 2 * middle + right
    offset = 0.5 * (left - right) / curvature if curvature > 1e-12 else 0
    return RATE / (lag + np.clip(offset, -0.5, 0.5))


def pitches(samples, frames):
    values = np.array([pitch_frame(centered(samples, i * HOP, 1440)) for i in range(frames)])
    # Suppress isolated octave jumps without spreading pitch into unvoiced frames.
    filtered = values.copy()
    for i, value in enumerate(values):
        neighbors = values[max(0, i - 2):i + 3]
        neighbors = neighbors[neighbors > 0]
        if value and len(neighbors) >= 3:
            median = np.median(neighbors)
            if abs(np.log2(value / median)) > 0.45:
                filtered[i] = median
    # Fit exactly the quantized pitch that the native renderer will consume.
    return np.rint(np.where(filtered > 0, np.clip(filtered, 60, 400), 0) * 64) / 64


def quantize(amplitudes):
    decibels = 20 * np.log10(np.maximum(amplitudes, 1e-30))
    return np.where(decibels < -96, 0, 1 + np.rint((np.clip(decibels, -96, 0) + 96) * 254 / 96)).astype("u1")


def harmonic_frame(target, frequency, slope, count, taper, seconds):
    numbers = np.arange(1, count + 1)[:, None]
    angles = 2 * np.pi * numbers * (frequency * seconds + 0.5 * slope * seconds ** 2)
    cosine, sine = np.cos(angles), np.sin(angles)
    basis = np.concatenate((cosine, sine))
    gram = (basis * taper) @ basis.T
    gram.flat[::len(gram) + 1] += np.trace(gram) / len(gram) * 1e-5
    coefficients = np.linalg.solve(gram, basis @ (target * taper))
    fitted = coefficients @ basis
    return fitted, coefficients, cosine, sine


def fit_controls(samples, pitch):
    """Refine the pitch estimate by fitting waves, including a local pitch bend.

    Gauss-Newton steps alternate with a linear least-squares amplitude/phase fit.
    This is offline optimization, not a learned model required at playback.
    """
    controls = np.zeros((len(pitch), 2))
    seconds = (np.arange(WINDOW) - WINDOW // 2) / RATE
    for i, initial in enumerate(pitch):
        if not initial:
            continue
        taper = fit_window(len(samples), i)
        target = centered(samples, i * HOP, WINDOW)
        frequency, slope = initial, 0.0
        count = min(24, int(RATE * 0.4 / initial))
        for _ in range(7):
            fitted, coefficients, cosine, sine = harmonic_frame(target, frequency, slope, count, taper, seconds)
            residual = target - fitted
            derivative = np.sum(np.arange(1, count + 1)[:, None] * (
                -coefficients[:count, None] * sine + coefficients[count:, None] * cosine), axis=0)
            jacobian = np.array([2 * np.pi * seconds * derivative, np.pi * seconds ** 2 * derivative])
            normal = (jacobian * taper) @ jacobian.T
            normal.flat[::3] += np.array([1e-8, 1e-12])
            delta = np.linalg.solve(normal, jacobian @ (residual * taper))
            delta = np.clip(delta, [-3, -300], [3, 300])
            old_error = np.sum(taper * residual ** 2)
            accepted = False
            for fraction in (1.0, 0.5, 0.25):
                new_frequency = np.clip(frequency + fraction * delta[0], max(60, initial * 0.92), min(400, initial * 1.08))
                new_slope = np.clip(slope + fraction * delta[1], -10000, 10000)
                new_fit, *_ = harmonic_frame(target, new_frequency, new_slope, count, taper, seconds)
                if np.sum(taper * (target - new_fit) ** 2) < old_error:
                    frequency, slope, accepted = new_frequency, new_slope, True
                    break
            if not accepted or (abs(delta[0]) < 0.001 and abs(delta[1]) < 0.1):
                break
        controls[i] = [np.rint(frequency * 64) / 64, np.rint(slope)]
    return controls


def fit(samples, harmonics, destination, controls=None):
    frames = (len(samples) + HOP - 1) // HOP + 1
    if controls is None:
        controls = fit_controls(samples, pitches(samples, frames))
    f0 = controls[:, 0]
    seconds = (np.arange(WINDOW) - WINDOW // 2) / RATE
    frequencies = np.fft.rfftfreq(FFT, 1 / RATE)
    noise_bands = [(frequencies >= first) & (frequencies < last) for first, last in zip(EDGES, EDGES[1:])]
    rows = []
    weighted_error, energy = 0.0, 0.0
    for i, frequency in enumerate(f0):
        taper = fit_window(len(samples), i)
        target = centered(samples, i * HOP, WINDOW)
        slope = controls[i, 1]
        count = min(harmonics, int(RATE * 0.48 / (frequency + abs(slope) * WINDOW / (2 * RATE)))) if frequency else 0
        amps, phases = np.zeros(harmonics), np.zeros(harmonics)
        fitted = np.zeros(WINDOW)
        if count:
            # Least-squares fitting with the same Hann weighting used for overlap-add.
            fitted, coefficients, *_ = harmonic_frame(target, frequency, slope, count, taper, seconds)
            amps[:count] = np.hypot(coefficients[:count], coefficients[count:])
            phases[:count] = np.arctan2(-coefficients[count:], coefficients[:count])
        residual = target - fitted
        spectrum = np.abs(np.fft.rfft(residual * taper, FFT)) ** 2
        band_rms = np.array([np.sqrt(2 * np.sum(spectrum[band]) / (FFT * np.sum(taper ** 2)))
                             for band in noise_bands])
        weighted_error += float(np.sum((residual * taper) ** 2))
        energy += float(np.sum((target * taper) ** 2))
        phase_codes = (np.rint((phases % (2 * np.pi)) * 256 / (2 * np.pi)).astype(int) % 256).astype("u1")
        rows.append(struct.pack("<Hh", round(frequency * 64), round(slope)) + quantize(amps).tobytes()
                    + phase_codes.tobytes() + quantize(band_rms).tobytes())
    destination.write_bytes(struct.pack("<4s6I", b"BJP2", RATE, len(samples), HOP, WINDOW, frames, harmonics)
                            + b"".join(rows))
    return {"voiced_frame_fraction": float(np.mean(f0 > 0)),
            "median_f0_hz": float(np.median(f0[f0 > 0])) if np.any(f0 > 0) else None,
            "harmonic_fit_energy_fraction": 1 - weighted_error / max(energy, 1e-20)}


def comparison_metrics(reference, reconstructed):
    result = {}
    for size in (256, 1024):
        def stft(values):
            frames = np.lib.stride_tricks.sliding_window_view(np.pad(values, (size // 2, size // 2)), size)[::size // 4]
            return np.abs(np.fft.rfft(frames * np.hanning(size)))
        a, b = stft(reference), stft(reconstructed)
        # Ignore silence and bins more than 60 dB below the reference's maximum.
        floor = max(float(np.max(a)) * 1e-3, 1e-9)
        result[f"spectral_convergence_{size}"] = float(np.linalg.norm(a - b) / max(np.linalg.norm(a), 1e-20))
        audible = a > floor
        result[f"mean_spectral_error_db_{size}"] = float(np.mean(np.abs(
            20 * np.log10(np.maximum(b[audible], floor) / a[audible])))) if audible.any() else 0
    result["rms_ratio"] = float(np.sqrt(np.mean(reconstructed ** 2) / max(np.mean(reference ** 2), 1e-20)))
    return result


def audition(output, report):
    sections = []
    for sample in report["samples"]:
        players = [f'<div><b>Björn reference</b><audio controls preload="metadata" aria-label="{html.escape(sample["name"])} reference" src="{html.escape(sample["reference"])}?v=2"></audio></div>']
        for variant in sample["variants"]:
            players.append(f'<div><b>{variant["harmonics"]} harmonics + noise</b>'
                           f'<audio controls preload="metadata" aria-label="{html.escape(sample["name"])} {variant["harmonics"]} harmonics" src="{html.escape(variant["wav"])}?v=2"></audio>'
                           f'<small>{variant["track_bytes"]:,} bytes of parameters; '
                           f'{variant["render_cpu_seconds"]:.3f} CPU s for {sample["seconds"]:.2f} s audio</small></div>')
        sections.append(f'<section><h2>{html.escape(sample["name"])}</h2><p lang="sv">{html.escape(sample["text"])}</p>'
                        + ''.join(players) + '</section>')
    (output / "index.html").write_text('''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Björn — small synthesizer experiment</title><style>
body{font:17px system-ui;max-width:940px;margin:40px auto;padding:0 20px;background:#f4f0e8;color:#29251f}
section{background:white;border-radius:12px;padding:22px;margin:22px 0}h1{font-size:28px}
audio{display:block;width:100%;margin:10px 0}small{color:#595149}section>div{margin:22px 0}
</style><h1>Björn → a small native synthesizer</h1>
<p>Original recording followed by fitted harmonic oscillators and shaped noise. No neural model runs during reconstruction.</p>
<p>Original MIT C11 renderer; no eSpeak or GPL speech code.</p>
<p><strong>This experiment stores parameters for each complete utterance.</strong> It does not yet speak new text.
Phase, pitch, amplitudes and noise energy are fitted offline. These are not recordings played by the renderer.
Listen for vowel identity, consonants, rhythm, buzzing and phrase endings. Quality has not been approved by listening.</p>
''' + ''.join(sections) + '''<p>Technical measurements and reference provenance: <a href="report.json">report.json</a>.</p>
<script>document.addEventListener('play', event => {
  if (event.target.tagName === 'AUDIO') document.querySelectorAll('audio').forEach(player => {
    if (player !== event.target) player.pause();
  });
}, true);</script></html>''', encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/bjorn-poc")
    parser.add_argument("--pack", type=Path, default=ROOT / "build/kokoro-pack")
    parser.add_argument("--probes", type=Path, default=ROOT / "build/cmake/bin")
    parser.add_argument("--reference", type=Path, action="append", help="Fit existing 24 kHz WAV instead of generating references.")
    parser.add_argument("--harmonics", type=int, nargs="+", default=[32, 64])
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()
    if any(not 1 <= count <= 128 for count in args.harmonics):
        parser.error("Harmonic counts must be between 1 and 128.")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "LICENSE-Perikop-MIT.txt").write_bytes((ROOT / "LICENSE").read_bytes())
    renderer = output / "bjorn-render"
    compiler = run(args.cc, "--version").splitlines()[0]
    run(args.cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", Path(__file__).with_name("render.c"), "-lm", "-o", renderer)
    report = {"method": "per-utterance optimized harmonic least squares + sixteen residual noise bands",
              "format": "BJP2", "sample_rate": RATE, "hop_samples": HOP, "window_samples": WINDOW,
              "renderer_compiler": compiler, "renderer_bytes": renderer.stat().st_size,
              "analysis_numpy": np.__version__, "samples": []}
    references = []
    if args.reference:
        for i, path in enumerate(args.reference):
            name = f"input-{i + 1}"
            samples = read_wav(path)
            references.append((name, path.name, samples, {"input": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}))
    else:
        pack = args.pack.resolve()
        manifest = json.loads((pack / "voice-pack.json").read_text())
        # Keep the actual teacher notices with the generated experiments.
        (output / "LICENSE-Kokoro-Swedish.txt").write_bytes((pack / "LICENSE-Kokoro-Swedish.txt").read_bytes())
        for name, text in SAMPLES:
            tokens = run(args.probes / "ortho-kokoro-text-probe", pack, input=text + "\n")
            token_file, raw_file = output / f"{name}.tokens", output / f"{name}.f32"
            token_file.write_text(tokens)
            timing = run(args.probes / "ortho-kokoro-audio-probe", pack, token_file, "bjorn", raw_file).strip()
            samples = np.fromfile(raw_file, dtype="<f4").astype(float)
            references.append((name, text, samples, {"voice": "bjorn", "pack": manifest, "probe_timing": timing,
                                                     "raw_sha256": hashlib.sha256(raw_file.read_bytes()).hexdigest()}))
    for name, text, samples, provenance in references:
        reference = output / f"{name}-reference.wav"
        write_wav(reference, samples)
        # Analyze the PCM reference used by the listener, including its quantization.
        samples = read_wav(reference)
        frames = (len(samples) + HOP - 1) // HOP + 1
        control_start = time.perf_counter()
        controls = fit_controls(samples, pitches(samples, frames))
        control_seconds = time.perf_counter() - control_start
        sample = {"name": name, "text": text, "reference": reference.name,
                  "seconds": len(samples) / RATE, "control_fit_wall_seconds": control_seconds,
                  "provenance": provenance, "variants": []}
        for count in args.harmonics:
            track, wav = output / f"{name}-{count}.bjp", output / f"{name}-{count}.wav"
            start = time.perf_counter()
            analysis = fit(samples, count, track, controls)
            fit_seconds = time.perf_counter() - start
            measurement = json.loads(run(renderer, track, wav))
            reconstructed = read_wav(wav)
            if len(reconstructed) != len(samples) or not np.isfinite(reconstructed).all():
                raise ValueError("Invalid reconstruction length or samples.")
            if measurement["clipped_samples"]:
                raise ValueError(f"Clipping in {wav}: {measurement}")
            variant = {"harmonics": count, "wav": wav.name, "track": track.name,
                       "track_bytes": track.stat().st_size, "pcm_to_track_ratio": (2 * len(samples)) / track.stat().st_size,
                       "fit_wall_seconds": fit_seconds, **analysis, **measurement,
                       **comparison_metrics(samples, reconstructed)}
            sample["variants"].append(variant)
            print(json.dumps({"sample": name, **variant}), flush=True)
        report["samples"].append(sample)
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    audition(output, report)
    print(f"Audition: {output / 'index.html'}", flush=True)


if __name__ == "__main__":
    main()
