#!/usr/bin/env python3
"""MIT. Reproducible full-reference signals, per-second locations and paired deltas."""
import argparse
import csv
import json
from pathlib import Path

import numpy as np

from reference_metrics import VERSION,cached_features,compare
import reference_metrics
from run import digest,dump,identity

SIGNALS=['mel_spectral_convergence','stft_spectral_convergence','logmel_mae_db','cepstral_envelope_distance_db','log_spectrum_mae_db','spectral_motion_mae_db',
         'energy_envelope_rmse_db','f0_rmse_semitones','f0_contour_correlation',
         'voicing_disagreement_fraction','active_span_abs_log_error','local_timing_deviation_log_p90']


def signal_value(metrics,key):
    if key=='active_span_abs_log_error':return float(abs(np.log(metrics['active_span_ratio'])))
    return metrics[key]


def summaries(rows):
    result={}
    for condition in sorted({r['condition'] for r in rows}):
        subset=[r for r in rows if r['condition']==condition];valid=[r for r in subset if r['comparison']['status']=='ok']
        item={'n':len(subset),'alignment_or_audio_failures':len(subset)-len(valid),'signals':{}}
        for key in SIGNALS:
            values=[signal_value(r['comparison']['metrics'],key) for r in valid if signal_value(r['comparison']['metrics'],key) is not None]
            item['signals'][key]={'n':len(values),'mean':float(np.mean(values)) if values else None,
                                 'median':float(np.median(values)) if values else None}
        item['quiet_missing_total']=sum(r['comparison']['metrics']['quiet_intervals']['missing'] for r in valid)
        item['quiet_extra_total']=sum(r['comparison']['metrics']['quiet_intervals']['extra'] for r in valid)
        result[condition]=item
    return result


def deltas(rows,before,after):
    groups={k:{r['id']:r['comparison'] for r in rows if r['condition']==k and r['comparison']['status']=='ok'} for k in (before,after)}
    ids=sorted(set(groups[before])&set(groups[after]));result={'before':before,'after':after,'paired_passages':len(ids),'signals':{}}
    rng=np.random.default_rng(1917)
    for key in SIGNALS:
        values=[signal_value(groups[after][id]['metrics'],key)-signal_value(groups[before][id]['metrics'],key) for id in ids
                if signal_value(groups[after][id]['metrics'],key) is not None and signal_value(groups[before][id]['metrics'],key) is not None]
        if not values:continue
        a=np.array(values);boot=a[rng.integers(0,len(a),size=(2000,len(a)))].mean(axis=1)
        result['signals'][key]={'n':len(a),'mean_after_minus_before':float(a.mean()),
                               'passage_bootstrap_ci95':np.percentile(boot,[2.5,97.5]).tolist(),
                               'better_direction':'higher' if key=='f0_contour_correlation' else 'lower'}
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--master',type=Path,required=True)
    p.add_argument('--manifest',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--reaper',type=Path,required=True)
    p.add_argument('--pair',nargs=2,action='append',default=[])
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    master=json.loads((args.master/'master.json').read_text());refs={r['id']:r for r in master['passages']}
    if 'unquantized FP32' not in master['provenance']['kind']:raise ValueError('Requires explicit unquantized FP32 master')
    suite=json.loads(args.manifest.read_text());rows=[]
    code=digest(Path(reference_metrics.__file__))
    report={'version':VERSION,'scope':'Distances to the frozen FP32 master, not an overall pleasantness score.',
            'master_provenance':master['provenance'],'master_manifest_sha256':digest(args.master/'master.json'),
            'manifest_sha256':digest(args.manifest),'metric_code_sha256':code,'reaper_sha256':digest(args.reaper),
            'report_driver_sha256':digest(Path(__file__)),
            'timing_decision':'abs(log(active_span_ratio)); total duration includes edge padding and is descriptive only',
            'manifest_metadata':suite.get('metadata',{}),'rows':rows}
    cache=args.output/'cache';cache.mkdir(exist_ok=True)
    for item in suite['pairs']:
        ref=refs[item['id']];reference=args.master/ref['audio'];candidate=Path(item['audio'])
        if digest(reference)!=ref['audio_sha256']:raise ValueError('Changed frozen master audio')
        ch=digest(candidate)
        if item.get('audio_sha256',ch)!=ch:raise ValueError('Changed candidate audio')
        key=identity([code,report['reaper_sha256'],ref['audio_sha256'],ch]);saved=cache/(key+'.json')
        if saved.exists():value=json.loads(saved.read_text())
        else:
            a=cached_features(reference,args.output/'features',args.reaper)
            b=cached_features(candidate,args.output/'features',args.reaper)
            value=compare(a,b);dump(saved,value)
        if value['status']=='ok':value['metrics']['active_span_abs_log_error']=signal_value(value['metrics'],'active_span_abs_log_error')
        row={**item,'audio_sha256':ch,'reference_sha256':ref['audio_sha256'],'comparison':value};rows.append(row)
        report['summary']=summaries(rows)
        if len(rows)%10==0:dump(args.output/'report.json',report)
        print(json.dumps({'condition':item['condition'],'id':item['id'],'status':value['status'],
                          'spectral_db':value.get('metrics',{}).get('logmel_mae_db')}),flush=True)
    report['paired_deltas']=[deltas(rows,*pair) for pair in args.pair]
    dump(args.output/'report.json',report)
    with (args.output/'signals.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=['condition','id','status',*SIGNALS]);writer.writeheader()
        for r in rows:
            c=r['comparison'];m=c.get('metrics',{})
            writer.writerow({'condition':r['condition'],'id':r['id'],'status':c['status'],**{k:m.get(k) for k in SIGNALS}})
    with (args.output/'error-windows.csv').open('w',newline='') as f:
        fields=['condition','id','reference_start_seconds','candidate_start_seconds','candidate_end_seconds',
                'logmel_mae_db','f0_mae_semitones','voicing_disagreement_fraction']
        writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader()
        for r in rows:
            for window in r['comparison'].get('windows',[]):writer.writerow({'condition':r['condition'],'id':r['id'],**window})


if __name__=='__main__':main()
