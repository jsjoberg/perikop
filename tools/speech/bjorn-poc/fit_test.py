"""Signal-level checks for the independent fitter and native renderer."""
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

import numpy as np

import fit


class SynthesisTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.folder = Path(cls.temporary.name)
        cls.renderer = cls.folder / "render"
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                        str(Path(__file__).with_name("render.c")), "-lm", "-o", str(cls.renderer)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def reconstruct(self, signal, controls=None, harmonics=8):
        track, wav = self.folder / "test.bjp", self.folder / "test.wav"
        fit.fit(signal, harmonics, track, controls)
        measurement = json.loads(fit.run(self.renderer, track, wav))
        result = fit.read_wav(wav)
        self.assertEqual(len(result), len(signal))
        self.assertEqual(measurement["clipped_samples"], 0)
        self.assertTrue(np.isfinite(result).all())
        return result, track, wav

    def test_estimated_pitch_and_fitted_waves_recover_a_harmonic_tone(self):
        time = np.arange(fit.RATE) / fit.RATE
        target = 0.2 * np.cos(2 * np.pi * 123 * time + 0.7) + 0.1 * np.cos(2 * np.pi * 246 * time - 1)
        pitch = fit.pitches(target, 101)
        self.assertLess(np.max(np.abs(pitch[5:-5] - 123)), 0.3)
        actual, _, _ = self.reconstruct(target)
        self.assertLess(np.sqrt(np.mean((target - actual) ** 2) / np.mean(target ** 2)), 0.035)

    def test_chirped_phase_recurrence_matches_the_offline_fit(self):
        time = np.arange(fit.RATE + 73) / fit.RATE
        phase = 2 * np.pi * (110 * time + 15 * time ** 2)
        target = 0.16 * np.cos(phase + 0.8) + 0.1 * np.cos(3 * phase - 0.4)
        frames = (len(target) + fit.HOP - 1) // fit.HOP + 1
        controls = np.column_stack((np.rint((110 + 30 * np.arange(frames) * fit.HOP / fit.RATE) * 64) / 64,
                                    np.full(frames, 30)))
        actual, _, _ = self.reconstruct(target, controls)
        self.assertLess(np.sqrt(np.mean((target - actual) ** 2) / np.mean(target ** 2)), 0.04)

    def test_noise_reconstruction_retains_energy_and_frequency_band(self):
        rng = np.random.default_rng(31)
        spectrum = np.fft.rfft(rng.normal(size=fit.RATE))
        frequencies = np.fft.rfftfreq(fit.RATE, 1 / fit.RATE)
        spectrum[(frequencies < 3100) | (frequencies >= 5400)] = 0
        target = np.fft.irfft(spectrum) * 0.08
        actual, _, _ = self.reconstruct(target, np.zeros((101, 2)))
        ratio = np.sqrt(np.mean(actual ** 2) / np.mean(target ** 2))
        self.assertGreater(ratio, 0.88)
        self.assertLess(ratio, 1.12)
        power = np.abs(np.fft.rfft(actual)) ** 2
        self.assertGreater(np.sum(power[(frequencies > 3000) & (frequencies < 5500)]) / np.sum(power), 0.98)

    def test_silence_and_single_sample_keep_exact_length(self):
        for length in (1, 241, fit.RATE):
            actual, _, _ = self.reconstruct(np.zeros(length))
            self.assertEqual(float(np.max(np.abs(actual))), 0)

    def test_render_is_deterministic(self):
        target = np.random.default_rng(5).normal(size=2400) * 0.04
        _, track, wav = self.reconstruct(target, np.zeros((11, 2)))
        first = wav.read_bytes()
        fit.run(self.renderer, track, wav)
        self.assertEqual(first, wav.read_bytes())

    def test_invalid_tracks_are_rejected(self):
        valid = struct.pack("<4s6I", b"BJP2", 24000, 1, 240, 960, 2, 8) + bytes(2 * (4 + 16 + 16))
        invalid_pitch = bytearray(valid)
        invalid_pitch[28:30] = struct.pack("<H", 59 * 64)
        cases = [b"", b"BJP2", valid[:-1], valid + b"x", b"FAIL" + valid[4:],
                 struct.pack("<4s6I", b"BJP2", 24000, 0xffffffff, 240, 960, 2, 8), invalid_pitch]
        for payload in cases:
            with self.subTest(size=len(payload)):
                path = self.folder / "invalid.bjp"
                path.write_bytes(payload)
                result = subprocess.run([str(self.renderer), str(path), str(self.folder / "invalid.wav")],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(result.stderr)


if __name__ == "__main__":
    unittest.main()
