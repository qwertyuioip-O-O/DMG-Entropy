#!/usr/bin/env python3
"""Reject highly variable experiments without deleting or trimming any rows."""
from pathlib import Path
import argparse,csv,json,statistics
p=argparse.ArgumentParser();p.add_argument('--dir',required=True);p.add_argument('--out');p.add_argument('--max-repeat-ratio',type=float,default=1.25);p.add_argument('--strict',action='store_true');a=p.parse_args()
d=Path(a.dir).resolve();rows=list(csv.DictReader((d/'timings.csv').open()));assert rows
fields=['encode_cpu','decode_cpu'] if 'encode_cpu' in rows[0] else ['encode','decode']
groups=[]
for dataset in sorted({x['dataset'] for x in rows}):
 for label in sorted({x['label'] for x in rows}):
  q=[x for x in rows if x['dataset']==dataset and x['label']==label]
  for field in fields:
   values=[float(x[field]) for x in q];assert values and min(values)>0
   ratio=max(values)/min(values)
   groups.append({'dataset':dataset,'label':label,'phase':field,'samples':len(values),'minimum_seconds':min(values),'maximum_seconds':max(values),'median_seconds':statistics.median(values),'repeat_max_over_min':ratio,'large_variation':ratio>a.max_repeat_ratio})
clock_check=None
if fields[0]=='encode_cpu':
 clock_check=all(float(x['process_cpu'])+.03>=3*(float(x['encode_cpu'])+float(x['decode_cpu'])) for x in rows)
result={'purpose':'conservative large-variation gate, not a confidence interval or sufficient proof of a small gain','threshold_max_over_min':a.max_repeat_ratio,'rows_preserved':len(rows),'rows_removed':0,'metric':'CPU phase time' if fields[0]=='encode_cpu' else 'wall phase time','groups':groups,'large_variation_groups':sum(x['large_variation'] for x in groups),'large_variation_gate_passed':not any(x['large_variation'] or x['samples']<2 for x in groups) and clock_check is not False,'cpu_clock_consistent_with_total_child_cpu':clock_check,'scope':'post-study descriptive audit for existing experiments; threshold is predeclared for future guarded runs; no outlier trimming'}
if 'process_wall' in rows[0]:result['median_total_cpu_over_wall']=statistics.median(float(x['process_cpu'])/float(x['process_wall']) for x in rows)
out=Path(a.out).resolve() if a.out else d/'measurement-quality.json';out.write_text(json.dumps(result,indent=2))
print(json.dumps({k:v for k,v in result.items() if k!='groups'},indent=2))
if a.strict and not result['large_variation_gate_passed']:raise SystemExit(2)
