"""Meaningful metric and decision-rule tests; not evidence of speech intelligibility."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np

from metrics import acoustic, normalize, paired_delta, score
from run import compare_reports, controls, recognize, summarize, token_chunks


class MetricsTest(unittest.TestCase):
    def test_sweden_and_alignment(self):
        self.assertEqual(normalize('»Å, Ä—Ö! HERREN; 123«'), 'å ä ö herren 123')
        s = score('ett två tre fyra','ett tre fem sex',{'number':['två','fyra']})
        self.assertEqual(s['word_errors'],3)
        self.assertEqual(s['wer'],.75)
        self.assertEqual(score('Gud','')['deletions'],1)
        self.assertEqual(score('a','a a a')['wer'],2)
        self.assertEqual(score('Själ','SJÄL!')['cer'],0)
        self.assertEqual(score('ä','a')['cer'],1)
        silence = score('Guds ord', '<|nospeech|>\n<|nospeech|>')
        self.assertTrue(silence['empty_transcript'])
        self.assertEqual(silence['wer'],1)
        self.assertEqual(silence['cer'],1)

    def test_silence_is_finite_and_unvoiced(self):
        a = acoustic(np.zeros(2400),24000)
        self.assertEqual(a['silence_fraction'],1)
        self.assertEqual(a['voiced_fraction'],0)
        self.assertIsNone(a['f0_hz']['median'])
        json.dumps(a,allow_nan=False)

    def test_control_specific_sensitivity(self):
        rate = 24000
        time = np.arange(rate*2)/rate
        pcm = .2*np.sin(2*np.pi*125*time)
        reference = acoustic(pcm,rate)
        changed = {k:acoustic(v,rate) for k,v in controls(pcm,rate).items()}
        self.assertAlmostEqual(reference['f0_hz']['median'],125,delta=3)
        self.assertGreater(changed['clipping']['clipped_fraction'],.5)
        self.assertGreater(changed['misplaced_silence']['max_internal_silence_seconds'],2.8)
        # The conservative click detector misses moderate phase/DC joins: retain this limitation.
        self.assertEqual(changed['hard_joins']['wave_jump_candidates_per_second'],0)
        impulse = pcm.copy(); impulse[rate] = .99
        self.assertGreater(acoustic(impulse,rate)['wave_jump_candidates_per_second'],0)
        self.assertLess(changed['destructive_overlap']['duration_seconds'],reference['duration_seconds'])
        # A smooth time-reversed or wrong-phone waveform can evade click/clipping metrics.
        self.assertEqual(acoustic(pcm[::-1],rate)['clipped_fraction'],0)

    def test_bootstrap_pairing_and_micro_weight(self):
        a = {str(i):{'intelligibility':score('ett två tre fyra','ett två tre')} for i in range(10)}
        b = {str(i):{'intelligibility':score('ett två tre fyra','ett två tre fyra')} for i in range(10)}
        d = paired_delta(a,b)
        self.assertEqual(d['delta'],-.25)
        self.assertEqual(d['ci95'],[-.25,-.25])
        self.assertEqual(d,paired_delta(a,b))
        with self.assertRaises(ValueError):
            paired_delta(a,{'extra':b['1']})

    def report(self, correct=False):
        ids = [str(i) for i in range(10)]
        rows = [{'id':i,'voice':'compact','split':s,'intelligibility':
                 score('ett två tre fyra','ett två tre fyra' if correct else 'ett två tre')}
                for s in ['dev','heldout'] for i in ids]
        return {'provenance':{'dataset_sha256':'d','asr_identity':'a','metrics_identity':'m',
                             'passage_ids':{'dev':ids,'heldout':ids}},
                'summary':{'status':'complete','reference_gate':{s:{'status':'pass'} for s in ['dev','heldout']}},
                'rows':rows}

    def test_conservative_quality_verdict(self):
        a,b = self.report(),self.report(True)
        self.assertEqual(compare_reports(a,b)['splits']['dev']['verdict'],'probably_improved')
        self.assertEqual(compare_reports(b,a)['splits']['dev']['verdict'],'probably_deteriorated')
        self.assertEqual(compare_reports(a,a)['splits']['dev']['verdict'],'uncertain')
        b['summary']['reference_gate']['dev']['status']='unreliable'
        self.assertEqual(compare_reports(a,b)['splits']['dev']['verdict'],'uncertain')
        b['provenance']['asr_identity']='different-model'
        self.assertEqual(compare_reports(a,b)['status'],'incompatible')

    def test_missing_audio_never_becomes_improvement(self):
        a,b = self.report(),self.report(True)
        b['rows'].pop()
        self.assertEqual(compare_reports(a,b)['splits']['heldout']['verdict'],'uncertain')

    def test_asr_receives_no_reference_or_hints(self):
        from argparse import Namespace
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            args = Namespace(cache=root/'cache',output=root,asr_identity='asr',ffmpeg=Path('ffmpeg'),
                             whisper=Path('whisper-cli'),asr_model=Path('model'),cpu=True)
            calls = []
            def fake(command):
                command = [str(c) for c in command]
                calls.append(command)
                if command[0]=='whisper-cli':
                    prefix = Path(command[command.index('-of')+1])
                    prefix.with_suffix('.txt').write_text('Guds ord')
                    prefix.with_suffix('.json').write_text('{}')
                return ''
            with patch('run.execute',fake):
                result = recognize(args,{'audio':'opaque.wav','audio_sha256':'sha'})
                self.assertEqual(result['text'],'Guds ord')
                recognize(args,{'audio':'opaque.wav','audio_sha256':'sha'})
            self.assertEqual(len(calls),2) # FFmpeg + ASR, then content cache hit
            self.assertNotIn('--prompt',calls[1])
            self.assertNotIn('--grammar',calls[1])
            self.assertEqual(calls[1][calls[1].index('-mc')+1],'0')
            self.assertTrue(all('HERREN' not in str(c) for c in calls))

    def test_neural_context_matches_production_split(self):
        tokens = [3]*1100
        tokens[500] = tokens[990] = 16
        chunks = list(token_chunks(tokens))
        self.assertEqual([len(c) for c in chunks],[500,489,109])
        self.assertTrue(all(len(c)<=510 for c in chunks))
        self.assertEqual(list(token_chunks([4]*1021)),[[4]*510,[4]*510,[4]])

    def test_bootstrap_resamples_adjacent_passages_together(self):
        rows = {str(i):{'reference':f'Gen {i//2}:1','intelligibility':score('ett två','ett')} for i in range(8)}
        self.assertEqual(paired_delta(rows,rows)['clusters'],4)

    def test_dataset_is_frozen_and_group_disjoint(self):
        d = json.loads(Path(__file__).with_name('dataset.json').read_text())
        self.assertEqual(len(d['passages']),50)
        self.assertEqual(sum(p['split']=='heldout' for p in d['passages']),10)
        groups = {s:{p['reference'].split(':')[0] for p in d['passages'] if p['split']==s} for s in ['dev','heldout']}
        self.assertFalse(groups['dev'] & groups['heldout'])
        self.assertTrue(all(normalize(p['text']) for p in d['passages']))


if __name__ == '__main__':
    unittest.main()
