import unittest

import numpy as np

from phrase_experiment import flow_map,pause_targets,phrase_marks,pitch_curve,quiet_intervals
from prosody_experiment import duration_marks,make_grains,render


class PhraseTests(unittest.TestCase):
    def test_pause_detector_uses_energy_and_pause_targets_are_bounded(self):
        pcm=np.r_[np.full(2400,.1),np.zeros(2400),np.full(2400,.1)]
        self.assertEqual(quiet_intervals(pcm),[(2400,4800)])
        dense=np.array([[0.,0.],[30000.,30000.]])
        # A voiced/unvoiced decision is deliberately not an input to this detector.
        active=np.full(30000,.1)
        changes=pause_targets([(2400,5760),(10000,17680),(22320,30000)],dense,active,30000)
        self.assertEqual([p['output_samples'] for p in changes],[960,3840,7680])
        quiet=np.zeros(30000)
        changes=pause_targets([(2400,5760),(10000,17680),(22320,30000)],dense,quiet,30000)
        self.assertEqual([p['output_samples'] for p in changes],[4320,11040,7680])

    def test_flow_changes_only_explicit_inserted_pause_durations(self):
        old=np.array([[0.,0.],[4000.,4000.],[7360.,7360.],[14000.,14000.]])
        points=flow_map(old,[{'begin':4000,'end':7360,'output_samples':960}])
        np.testing.assert_array_equal(points[:,0],[0,4000,4960,11600])
        self.assertEqual(points[-1,0]-points[-2,0],14000-7360)
        self.assertTrue(np.all(np.diff(points[:,0])>0))

    def test_zero_pitch_strength_is_exactly_the_v4_scheduler(self):
        centers=np.cumsum(np.resize([137,251,189,214,171,233],130))
        centers=centers[centers<23999]
        pm=np.c_[centers/24000,np.ones(len(centers)),np.zeros(len(centers))]
        points=np.array([[0.,0.],[24000.,24000.]])
        expected,expected_positions=duration_marks(pm,points,24000)
        marks,positions,_=phrase_marks(pm,points,points,24000,np.array([[0.,7.],[1.,7.]]),strength=0)
        np.testing.assert_array_equal(marks,expected)
        np.testing.assert_array_equal(positions,expected_positions)

    def test_pitch_controller_reaches_target_without_altering_duration(self):
        pcm=.4*np.cos(2*np.pi*100*np.arange(24000)/24000)
        centers=np.arange(240,24000,240)
        pm=np.c_[centers/24000,np.ones(len(centers)),np.zeros(len(centers))]
        points=np.array([[0.,0.],[24000.,24000.]])
        f0=np.c_[np.arange(0,1,.005),np.ones(200),np.full(200,125.)]
        marks,positions,audit=phrase_marks(pm,points,points,24000,pitch_curve(f0))
        wave,norm=render(pcm,len(pcm),make_grains(pcm,pm,marks,positions))
        frequencies=np.fft.rfftfreq(19200,1/24000)
        dominant=frequencies[np.argmax(np.abs(np.fft.rfft(wave[2400:-2400])))]
        self.assertAlmostEqual(dominant,125.,delta=1.)
        self.assertTrue(np.all(norm>0))
        self.assertEqual(len(wave),24000)
        self.assertEqual(audit['clamped_epochs'],0)
        for shift in audit['applied_shift_semitones_p10_p50_p90']:
            self.assertAlmostEqual(shift,12*np.log2(1.25))


if __name__=='__main__':unittest.main()
