import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

import numpy as np

from metrics import read_audio,write_audio
from prosody_experiment import render,save_plan
from run import ROOT


class ProsodyTests(unittest.TestCase):
    def test_pitch_synchronous_repetition_changes_pitch_without_changing_duration(self):
        pcm=.5*np.cos(2*np.pi*100*np.arange(24000)/24000)
        grains=[(12000,t,240,240,1.) for t in range(200,23800,200)]
        output,_=render(pcm,24000,grains)
        middle=output[2400:-2400]
        frequencies=np.fft.rfftfreq(len(middle),1/24000)
        dominant=frequencies[np.argmax(np.abs(np.fft.rfft(middle)))]
        self.assertAlmostEqual(dominant,120.,delta=1.)
        self.assertEqual(len(output),len(pcm))

    def test_identity_overlap_add_preserves_source_samples(self):
        pcm=np.random.default_rng(1917).normal(0,.1,24000)
        grains=[(t,t,240,240,1.) for t in range(0,24000,120)]
        output,normalizer=render(pcm,len(pcm),grains)
        self.assertTrue(np.all(normalizer>0))
        np.testing.assert_allclose(output,pcm,atol=1e-14)

    def test_native_renderer_matches_independent_python_reference_and_rejects_bad_extent(self):
        renderer=ROOT/'build/unit-selection/bjorn-grain-render'
        if not renderer.exists():self.skipTest('Build grain renderer first')
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            write_audio(folder/'input.wav',.4*np.cos(2*np.pi*100*np.arange(24000)/24000),24000)
            pcm,_=read_audio(folder/'input.wav')
            grains=[(12000,t,240,240,1.) for t in range(200,23800,200)]
            expected,_=render(pcm,24000,grains)
            write_audio(folder/'expected.wav',expected,24000)
            save_plan(folder/'plan',24000,grains)
            command=[renderer,folder/'input.wav',folder/'plan',folder/'out.wav']
            result=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            actual,_=read_audio(folder/'out.wav');expected,_=read_audio(folder/'expected.wav')
            self.assertLessEqual(float(np.max(np.abs(actual-expected))*32768),1.)
            save_plan(folder/'plan',24000,[(24000,200,240,240,1.)])
            self.assertNotEqual(subprocess.run(command,capture_output=True).returncode,0)


if __name__=='__main__':unittest.main()
