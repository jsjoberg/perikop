#!/usr/bin/env python3
"""MIT. Export the reference-based iteration report; optional scientific figure."""
import argparse
import json
from pathlib import Path
import shutil

import numpy as np

from run import dump,digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--evaluation',type=Path,required=True);p.add_argument('--validation',type=Path,required=True)
    p.add_argument('--master',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--plot',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    report=json.loads((a.evaluation/'report.json').read_text())
    gate=json.loads((a.evaluation/'v4-v5-review.json').read_text())
    validation=json.loads((a.validation/'validation.json').read_text())
    master=json.loads((a.master/'master.json').read_text());asr=json.loads((a.master/'evaluation.json').read_text())
    tests=json.loads((a.evaluation/'tests.json').read_text())
    ids={'1cor-013-004-007','1kgs-019-011-012','1sam-003-009-010'}
    order=['master-repeat-seed43','compressed-neural','shipped-compact','compact-v2','v3-rhythm','v3-rhythm-and-pitch','compact-v4','compact-v5']
    signals=['mel_spectral_convergence','logmel_mae_db','f0_rmse_semitones','voicing_disagreement_fraction','local_timing_deviation_log_p90']
    paired={}
    for condition in order:
        subset=[r for r in report['rows'] if r['id'] in ids and r['condition']==condition and r['comparison']['status']=='ok']
        if len(subset)!=3:raise ValueError('Incomplete matched comparison: '+condition)
        paired[condition]={key:[r['comparison']['metrics'][key] for r in subset] for key in signals}
    for source,name in [(a.evaluation/'report.json','full-reference-results.json'),(a.evaluation/'signals.csv','full-reference-signals.csv'),
        (a.evaluation/'error-windows.csv','full-reference-error-windows.csv'),(a.evaluation/'v4-v5-review.json','full-reference-v4-v5-review.json'),
        (a.validation/'validation.json','full-reference-validation.json'),(a.validation/'report.json','full-reference-controls-report.json'),
        (a.master/'master.json','full-precision-master-manifest.json'),(a.master/'evaluation.json','full-precision-master-asr.json')]:
        shutil.copyfile(source,a.output/name)
    method=Path(__file__).resolve().parents[3]/'docs/reports/full-reference-evaluation.md'
    shutil.copyfile(method,a.output/'full-reference-method.md')
    summary={'scope':'Separate acoustic error signals, not an overall human-quality score.',
        'reference_passages':len(master['passages']),'experiment_pairs':len(report['rows']),
        'control_pairs':len(json.loads((a.validation/'report.json').read_text())['rows']),
        'validation':validation,'tests':tests,'master_provenance':master['provenance'],
        'master_asr':asr['aggregate'],'matched_three_passage_values':paired,'v4_v5_review':gate,
        'experiment_report_sha256':digest(a.evaluation/'report.json')}
    summary['human_preference_audit']={
        'known_disagreement':'Several average distances favour rejected v3 over v4, although the listener preferred v4.',
        'interpretation':'This profile measures selected physical errors; it is not validated as an overall naturalness ranking.',
        'v5_interpretation':'Average spectral distances improve, while three local spectral windows regress. The timing decision excludes outer silence; raw total duration is descriptive only. This is not proof of the cause of the listener complaint.'}
    dump(a.output/'full-reference-summary.json',summary)
    labels={'master-repeat-seed43':'FP32 repeat, seed 43','compressed-neural':'Compressed neural','shipped-compact':'Original compact',
            'compact-v2':'Compact v2','v3-rhythm':'V3 rhythm (rejected)','v3-rhythm-and-pitch':'V3 rhythm + pitch (rejected)',
            'compact-v4':'V4 (better, staccato)','compact-v5':'V5 (mixed listening result)'}
    lines=['# Full-reference numerical feedback','',
        'The master is the original **327,216,523-byte FP32 Swedish checkpoint**, not the 166 MB compressed ONNX model. Forty development WAVs are frozen with hashes. No synthesis change was made in this stage.','',
        f"Completed **{len(report['rows'])} experiment comparisons**, **70 controlled comparisons**, and **{len(validation['checks'])} controlled-signal checks**. Validation passed: **{validation['passed']}**. The full master has {asr['aggregate']['wer']*100:.2f}% WER and {asr['aggregate']['cer']*100:.2f}% CER on the 40-passage independent ASR check.",'',
        '## Matched comparison','',
        'These means use the **same three original problem passages** for every row. Lower is closer to the master on each listed error. The columns are not a combined pleasantness score.','',
        '| Version | Mel magnitude error, relative | Log-mel error, dB | Pitch RMSE, semitones | Voicing mismatch | Local timing deviation, log |',
        '|---|---:|---:|---:|---:|---:|']
    for k in order:
        m={s:float(np.mean(paired[k][s])) for s in signals}
        lines.append(f"| {labels[k]} | {m[signals[0]]:.3f} | {m[signals[1]]:.2f} | {m[signals[2]]:.2f} | {m[signals[3]]*100:.1f}% | {m[signals[4]]:.3f} |")
    lines+=['','## V4 to V5 review','',f"Automatic numerical promotion: **{gate['automatic_promotion']}**.",'',
            *['- '+reason+'.' for reason in gate['reasons']],'',
            'A lower pitch error cannot offset worse spectral fidelity. The review includes paired bootstrap deltas, provisional FP32 repeat allowances, ASR changes and local spectral regressions. Three passages and three repeat renders give provisional evidence.','',
            'Largest local increases in log-mel error, with timestamps in each recording:','',
            '| Passage | Master second | V5 seconds | Increase, dB |','|---|---:|---:|---:|']
    for w in gate['worst_local_spectral_regressions'][:6]:
        lines.append(f"| {w['id']} | {w['reference_start_seconds']} | {w['candidate_start_seconds']:.2f}–{w['candidate_end_seconds']:.2f} | {w['spectral_error_increase_db']:+.2f} |")
    lines+=['','## Use the loop','',
        'Human-preference audit: several averages favour rejected v3 over v4, despite the listener preferring v4. The profile therefore **must not be used as an overall naturalness ranking**. V5’s measured tradeoffs are useful diagnostic leads, not proof that its three flagged windows caused the listening complaint.','',
        '1. Change one synthesis mechanism and render a fixed passage set.',
        '2. Run the reference comparator and independent ASR; inspect the separate errors and their paired deltas.',
        '3. Investigate the worst windows and their source-unit traces. Retain or revert the trial from those signals, then broaden promising trials before another listening request.','',
        'The implementation does not predict MOS. Log spectral errors can overemphasize weak bands; spectral convergence complements them. F0 tracking, quiet-interval detection and DTW remain approximate. The controlled tests establish sensitivity and invariance, not complete agreement with human naturalness judgments.','',
        f"Tests: {tests['tests_run']} run, {tests['failures']} failures, {tests['errors']} errors. Native tests were executed using byte-identical copies from a temporary directory after intermittent SIGKILL at workspace paths; details and hashes are retained in the machine-readable summary.",'',
        '[Method and reproducible commands](full-reference-method.md) · [Per-passage CSV](full-reference-signals.csv) · [Localized errors](full-reference-error-windows.csv) · [Numerical review](full-reference-v4-v5-review.json) · [Validation](full-reference-validation.json) · [Frozen FP32 master WAVs, lossless ZIP](fp32-bjorn-master-wavs.zip)','']
    (a.output/'full-reference-findings.md').write_text('\n'.join(lines))
    if a.plot:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        selected=order[1:];names=[labels[k] for k in selected]
        fig,axes=plt.subplots(2,2,figsize=(13,8.5),layout='constrained')
        for ax,key,title,scale in zip(axes.flat,[signals[0],signals[2],signals[3],signals[4]],
             ['Mel spectral convergence (relative error)','Pitch RMSE (semitones)','Voicing mismatch (%)','Local timing deviation (log ratio)'],[1,1,100,1]):
            values=[np.array(paired[k][key])*scale for k in selected]
            means=np.array([v.mean() for v in values]);lo=means-np.array([v.min() for v in values]);hi=np.array([v.max() for v in values])-means
            ax.barh(np.arange(len(names)),means,xerr=np.vstack([lo,hi]),color=['#97a6b4']*5+['#34536d','#157a92'],capsize=3)
            ax.set_yticks(np.arange(len(names)),names);ax.invert_yaxis();ax.set_title(title,loc='left',fontsize=12)
            ax.set_axisbelow(True);ax.grid(axis='x',alpha=.2);ax.spines[['top','right']].set_visible(False)
        fig.suptitle('Distance to the original FP32 Björn master\nSame three passages; bars are means, whiskers show passage range; lower is closer on these signals',fontsize=15)
        fig.savefig(a.output/'full-reference-comparison.png',dpi=180);plt.close(fig)
    print(json.dumps({'exported':str(a.output),'conditions':len(paired),'review':gate['automatic_promotion']}))


if __name__=='__main__':main()
