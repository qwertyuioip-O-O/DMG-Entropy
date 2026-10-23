#!/usr/bin/env python3
"""Independently reconstruct units and paired aggregate from every native row."""
import argparse,csv,json,math,hashlib
from pathlib import Path
from collections import defaultdict
p=argparse.ArgumentParser();p.add_argument('--dir',required=True);a=p.parse_args();d=Path(a.dir)
raw=list(csv.DictReader((d/'raw/timings.csv').open()));units=list(csv.DictReader((d/'timings.csv').open()));groups=defaultdict(list)
for row in raw:groups[tuple(row[k] for k in ['dataset','block','position','label'])].append(row)
assert len(raw)==96 and len(units)==48 and len(groups)==48
for row in units:
 samples=groups[tuple(row[k] for k in ['dataset','block','position','label'])]
 assert len(samples)==2 and {s['native'] for s in samples}=={'0','1'}
 assert all(s['plies']==row['plies'] for s in samples)
 for k in ['encode','decode','process_cpu','process_wall']:
  assert math.isclose(float(row[k]),sum(float(s[k]) for s in samples)/2,rel_tol=1e-13,abs_tol=1e-13),(k,row)
summary=json.loads((d/'summary.json').read_text());rebuilt={}
for phase in ['encode','decode','process_cpu']:
 logs=[]
 for dataset,block in sorted({(s['dataset'],s['block']) for s in raw}):
  sums={label:sum(float(s[phase]) for s in raw if (s['dataset'],s['block'],s['label'])==(dataset,block,label)) for label in ['baseline','candidate']}
  assert all(x>0 for x in sums.values());logs.append(math.log(sums['baseline']/sums['candidate']))
 rebuilt[phase]=100*math.expm1(sum(logs)/len(logs))
 assert math.isclose(rebuilt[phase],summary['aggregate_gain_percent'][phase],rel_tol=1e-11,abs_tol=1e-11)
result={'raw_rows':len(raw),'units':len(units),'native_calls_per_unit':2,'dropped_rows':0,'every_unit_reconstructed':True,'aggregate_independently_reconstructed':rebuilt,'raw_csv_sha256':hashlib.sha256((d/'raw/timings.csv').read_bytes()).hexdigest()}
(d/'batch-integrity.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
