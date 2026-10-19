#!/usr/bin/env python3
"""MIT. Conservative signal regression review; does not certify pleasantness."""
import argparse
import json
from pathlib import Path

import numpy as np

from reference_compare import SIGNALS,deltas,signal_value
from run import digest,dump


def review(report,before,after,minimum_passages=5):
    rows=report['rows'];paired=deltas(rows,before,after)
    repeated=[r['comparison']['metrics'] for r in rows if r['condition']=='master-repeat-seed43' and r['comparison']['status']=='ok']
    decisions={};regressions=[];improvements=[]
    # These are empirical repeat-variation allowances, not perceptual JNDs.
    for key,data in paired['signals'].items():
        observations=[abs(1-signal_value(r,key)) if key=='f0_contour_correlation' else abs(signal_value(r,key))
                      for r in repeated if signal_value(r,key) is not None]
        margin=float(np.percentile(observations,95)) if observations else None
        sign=-1 if key=='f0_contour_correlation' else 1
        delta=data['mean_after_minus_before']*sign
        ci=sorted(v*sign for v in data['passage_bootstrap_ci95'])
        decision='uncertain'
        if margin is not None:
            if ci[0]>margin:decision='regressed';regressions.append(key)
            elif ci[1]<-margin:decision='improved';improvements.append(key)
        decisions[key]={**data,'repeat_variation_allowance':margin,'signal_decision':decision}
    conditions={k:{r['id']:r for r in rows if r['condition']==k} for k in (before,after)}
    local=[]
    for id in sorted(set(conditions[before])&set(conditions[after])):
        a={w['reference_start_seconds']:w for w in conditions[before][id]['comparison'].get('windows',[])}
        for b in conditions[after][id]['comparison'].get('windows',[]):
            if b['reference_start_seconds'] in a:
                old=a[b['reference_start_seconds']]
                local.append({'id':id,**b,'previous_logmel_mae_db':old['logmel_mae_db'],
                              'spectral_error_increase_db':b['logmel_mae_db']-old['logmel_mae_db']})
    failures=[id for id,r in conditions[after].items() if r['comparison']['status']!='ok']
    repeat_windows=[w['logmel_mae_db'] for r in rows if r['condition']=='master-repeat-seed43'
                    for w in r['comparison'].get('windows',[])]
    local_allowance=float(np.percentile(repeat_windows,95)) if repeat_windows else None
    local_regressions=[w for w in local if local_allowance is not None and w['spectral_error_increase_db']>local_allowance]
    asr=[]
    for id in sorted(set(conditions[before])&set(conditions[after])):
        a,b=conditions[before][id],conditions[after][id]
        if 'asr_identity' in a and a.get('asr_identity')==b.get('asr_identity') and 'asr_wer' in a and 'asr_wer' in b:
            asr.append(b['asr_wer']-a['asr_wer'])
    reasons=[]
    if failures:reasons.append('Candidate acoustic-alignment/audio failures')
    if regressions:reasons.append('Mean signal regressions beyond repeat variation')
    if local_regressions:reasons.append('Local spectral regressions beyond provisional repeat variation')
    if asr and np.mean(asr)>0:reasons.append('Mean paired ASR word error worsened')
    if paired['paired_passages']<minimum_passages:reasons.append('Too few paired passages for automatic promotion')
    if not improvements:reasons.append('No signal improvement established beyond repeat variation')
    return {'scope':'Empirical signal review only; never an acceptance of voice pleasantness.',
        'before':before,'after':after,'paired_passages':paired['paired_passages'],
        'repeat_calibration_passages':len(repeated),'minimum_passages_policy':minimum_passages,
        'automatic_promotion':'hold' if reasons else 'eligible_for_broader_evaluation',
        'reasons':reasons,'failed_passages':failures,'signals':decisions,
        'asr_paired_passages':len(asr),'asr_mean_passage_wer_delta':float(np.mean(asr)) if asr else None,
        'local_repeat_variation_allowance_db':local_allowance,'local_regression_count':len(local_regressions),
        'worst_local_spectral_regressions':sorted(local,key=lambda w:w['spectral_error_increase_db'],reverse=True)[:12],
        'limits':'Three repeat renders give a provisional noise allowance, not a validated perceptual threshold. Five passages is a workflow minimum, not a statistical power guarantee. Inspect local regressions even when utterance means improve.'}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--report',type=Path,required=True)
    p.add_argument('--before',required=True);p.add_argument('--after',required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();result=review(json.loads(a.report.read_text()),a.before,a.after)
    result['comparison_sha256']=digest(a.report);dump(a.output,result)
    print(json.dumps({k:result[k] for k in ('automatic_promotion','reasons','paired_passages')},indent=2))


if __name__=='__main__':main()
