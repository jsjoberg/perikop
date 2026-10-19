import unittest
import json

import numpy as np

from reference_metrics import compare,features


def signal():
    t=np.arange(24000*3)/24000
    phase=2*np.pi*(100*t+10*t*t)
    return (.1*np.sin(phase)+.05*np.sin(phase*3)+.02*np.sin(phase*13))*(.5+.4*np.cos(2*np.pi*1.1*t))


def prof(pcm,factor=1):
    result=features(pcm)
    if result is not None:result['f0']=(100+20*result['times'])*factor
    return result


class ReferenceTests(unittest.TestCase):
    def test_identity_and_polarity_have_zero_acoustic_error(self):
        pcm=signal();r=prof(pcm)
        for c in [r,prof(-pcm)]:
            result=compare(r,c);self.assertEqual(result['status'],'ok');m=result['metrics']
            for name in ['logmel_mae_db','mel_spectral_convergence','stft_spectral_convergence','cepstral_envelope_distance_db','log_spectrum_mae_db','f0_rmse_semitones','energy_envelope_rmse_db']:
                self.assertAlmostEqual(m[name],0,places=8)
            self.assertEqual(m['duration_ratio'],1.)
            self.assertAlmostEqual(m['f0_contour_correlation'],1.)

    def test_gain_does_not_masquerade_as_timbre_error_but_is_reported(self):
        pcm=signal();result=compare(prof(pcm),prof(pcm*.5))['metrics']
        self.assertAlmostEqual(result['candidate_gain_correction_db'],20*np.log10(2),places=8)
        self.assertAlmostEqual(result['logmel_mae_db'],0,places=8)

    def test_pitch_and_spectral_damage_have_separate_signals(self):
        pcm=signal();r=prof(pcm);pitched=prof(pcm,2**(3/12))
        m=compare(r,pitched)['metrics']
        self.assertAlmostEqual(m['f0_rmse_semitones'],3.,places=8)
        self.assertAlmostEqual(m['logmel_mae_db'],0.,places=8)
        noisy=prof(pcm+np.random.default_rng(1917).normal(0,.01,len(pcm)))
        self.assertGreater(compare(r,noisy)['metrics']['logmel_mae_db'],1.)

    def test_inserted_gap_survives_time_alignment_in_timing_signals(self):
        pcm=signal();changed=np.r_[pcm[:36000],np.zeros(7200),pcm[36000:]]
        result=compare(prof(pcm),prof(changed));self.assertEqual(result['status'],'ok')
        m=result['metrics'];self.assertGreater(m['duration_ratio'],1.09)
        self.assertGreater(m['active_span_ratio'],1.09)
        json.dumps(result,allow_nan=False)
        self.assertGreater(m['quiet_intervals']['extra'],0)
        self.assertGreater(m['local_timing_deviation_log_p90'],0)

    def test_empty_candidate_is_explicit_failure(self):
        self.assertEqual(compare(prof(signal()),prof(np.zeros(24000)))['status'],'candidate_no_active_audio')


if __name__=='__main__':unittest.main()
