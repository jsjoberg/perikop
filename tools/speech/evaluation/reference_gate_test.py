import unittest

from reference_compare import SIGNALS
from reference_gate import review


def row(id,condition,value,corr):
    metrics={k:value for k in SIGNALS};metrics['f0_contour_correlation']=corr
    metrics['active_span_ratio']=1+value
    return {'id':id,'condition':condition,'comparison':{'status':'ok','metrics':metrics,'windows':[]}}


class ReferenceGateTests(unittest.TestCase):
    def test_outer_padding_does_not_affect_timing_decision(self):
        from reference_compare import signal_value
        metrics={'active_span_ratio':1.,'duration_ratio':1.2,'duration_abs_log_error':.182}
        self.assertEqual(signal_value(metrics,'active_span_abs_log_error'),0.)
    def test_better_pitch_cannot_cancel_spectral_regression(self):
        rows=[]
        for i in range(5):
            rows.append(row(str(i),'before',1,.8));after=row(str(i),'after',1,.8)
            after['comparison']['metrics']['f0_rmse_semitones']=.1
            after['comparison']['metrics']['mel_spectral_convergence']=2
            rows.extend([after,row(str(i),'master-repeat-seed43',.01,.99)])
        result=review({'rows':rows},'before','after')
        self.assertEqual(result['signals']['f0_rmse_semitones']['signal_decision'],'improved')
        self.assertEqual(result['signals']['mel_spectral_convergence']['signal_decision'],'regressed')
        self.assertEqual(result['automatic_promotion'],'hold')

    def test_small_sample_and_audio_failure_cannot_promote(self):
        rows=[]
        for i in range(3):
            rows.extend([row(str(i),'before',1,.5),row(str(i),'after',.5,.8),row(str(i),'master-repeat-seed43',.01,.99)])
        result=review({'rows':rows},'before','after')
        self.assertEqual(result['automatic_promotion'],'hold')
        rows[-2]['comparison']={'status':'candidate_no_active_audio'}
        self.assertEqual(review({'rows':rows},'before','after')['failed_passages'],['2'])


if __name__=='__main__':unittest.main()
