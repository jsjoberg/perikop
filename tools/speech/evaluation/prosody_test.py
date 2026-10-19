import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

import numpy as np

from metrics import read_audio,write_audio
from prosody_experiment import duration_marks,make_grains,render,save_plan,word_map_from_intervals
from run import ROOT


class ProsodyTests(unittest.TestCase):
    def test_actual_duration_scheduler_preserves_irregular_source_epochs_under_identity_map(self):
        pcm=np.random.default_rng(1917).normal(0,.1,24000)
        centers=np.cumsum(np.resize([137,251,189,214,171,233],130))
        centers=centers[centers<len(pcm)-1]
        voiced=(np.arange(len(centers))%7!=0).astype(float)
        pm=np.c_[centers/24000,voiced,np.zeros(len(centers))]
        marks,positions=duration_marks(pm,np.array([[0.,0.],[len(pcm),len(pcm)]]),len(pcm))
        np.testing.assert_array_equal(marks[1:-1,0],centers)
        np.testing.assert_array_equal(marks[:,0],positions)
        output,normalizer=render(pcm,len(pcm),make_grains(pcm,pm,marks,positions))
        self.assertTrue(np.all(normalizer>0))
        np.testing.assert_allclose(output,pcm,atol=1e-14)

    def test_word_durations_are_bounded_with_unchanged_pauses_and_uniform_internal_timing(self):
        intervals=[(100,4100,16000,'longer'),(4600,7600,900,'shorter'),(8000,9000,10000,'particle')]
        points,begin,end,audit=word_map_from_intervals(intervals,10000,.1)
        self.assertEqual(begin,0)
        self.assertEqual(end,10100)
        self.assertEqual([w['output_samples'] for w in audit['words']],[4400,2700,1000])
        self.assertTrue(all(p['source_samples']==p['output_samples'] for p in audit['pauses']))
        # All interior samples follow one affine map per word, including its midpoint.
        for a,b in zip(points[:-1],points[1:]):
            self.assertAlmostEqual(np.interp((a[0]+b[0])/2,points[:,0],points[:,1]),(a[1]+b[1])/2)
        for w in audit['words']:self.assertLessEqual(abs(w['ratio']-1),.100001)
        identity,_,identity_end,_=word_map_from_intervals(intervals,10000,0)
        self.assertEqual(identity_end,10000)
        np.testing.assert_array_equal(identity[:,0],identity[:,1])
        with self.assertRaises(ValueError):word_map_from_intervals(intervals,10000,.5)

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
            self.assertEqual(subprocess.run(command,capture_output=True).returncode,1)


if __name__=='__main__':unittest.main()
