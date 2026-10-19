"""Regression checks for timing correction and the native selected-unit format."""
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import wave

import numpy as np

from selection_experiment import ROOT,phone_flags,prep


class TimingTests(unittest.TestCase):
    def test_offset_preserves_phone_labels_and_lengths_away_from_edges(self):
        symbols={1:'s',2:'ˈ',3:'a',4:'ː',5:' ',6:'t'}
        tokens=[0,1,2,3,4,5,6,0]
        durations=[10,2,1,3,2,1,2,10]
        original=prep.aligned_phones(tokens,durations,symbols,18600)
        shifted=prep.aligned_phones(tokens,durations,symbols,18600,-1500)
        self.assertEqual([p[0] for p in original],[p[0] for p in shifted])
        for a,b in zip(original[1:-1],shifted[1:-1]):
            self.assertEqual(a[1]-b[1],1500)
            self.assertEqual(a[2]-b[2],1500)
        self.assertEqual(shifted[0][1],0)

    def test_punctuation_resets_dangling_stress(self):
        flags=phone_flags('hɛːrɔmˈ:dˈeːn')
        self.assertIn(('d',True,False),flags)
        self.assertNotIn(('ˈd',True,False),flags)


class NativeRendererTests(unittest.TestCase):
    renderer=ROOT/'build/unit-selection/bjorn-unit-render'

    def setUp(self):
        if not self.renderer.exists():self.skipTest('Compile unit_render.cpp first')
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder=Path(self.temp.name)
        self.bank=self.folder/'bank.u8'
        self.bank.write_bytes(bytes([180])*1000+bytes([70])*1000)

    def render(self,spans,tail=b''):
        plan=self.folder/'plan'
        plan.write_bytes(struct.pack('<4sI',b'BUS1',len(spans))+
                         b''.join(struct.pack('<QII',*s) for s in spans)+tail)
        return subprocess.run([self.renderer,self.bank,plan,self.folder/'out.wav'],capture_output=True,text=True)

    def test_overlap_has_expected_duration_and_silence(self):
        result=self.render([(0,1000,0),(1000,1000,240),(2**64-1,100,0)])
        self.assertEqual(result.returncode,0,result.stderr)
        with wave.open(str(self.folder/'out.wav')) as f:
            self.assertEqual(f.getnframes(),1860)
            pcm=np.frombuffer(f.readframes(1860),dtype='<i2')
        self.assertTrue(np.all(pcm[-100:]==0))
        self.assertTrue(np.all(np.diff(pcm[760:1000].astype(int))<=0))
        self.assertEqual(json.loads(result.stdout)['joins'],1)

    def test_rejects_span_past_bank_end_and_trailing_bytes(self):
        self.assertEqual(self.render([(1999,2,0)]).returncode,1)
        self.assertEqual(self.render([(0,100,0)],b'x').returncode,1)

    def test_rejects_overlap_across_pause_boundary(self):
        self.assertEqual(self.render([(0,100,0),(2**64-1,100,0),(1000,100,50)]).returncode,1)


if __name__=='__main__':unittest.main()
