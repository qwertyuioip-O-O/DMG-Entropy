#!/usr/bin/env python3
import csv,json,statistics
from pathlib import Path
r=Path(__file__).resolve().parents[2];b=r/'benchmarks-r17'
compression=[]
for name in ['new','wcc','r16confirm','r15','human']:
 rows={int(x['mode']):x for x in csv.DictReader((b/(name+'.csv')).open())};z,p=rows[0],rows[2]
 assert len(rows)==6 and all(x['roundtrip']=='PASS' for x in rows.values())
 item={'dataset':name,'games':int(z['games']),'plies':int(z['plies']),'middle_plies':int(z['mid_n']),'baseline_payload_bits':int(z['bits']),'candidate_payload_bits':int(p['bits']),'same_header_baseline_bytes':int(z['bytes']),'native_baseline_bytes':int(z['bytes'])-1,'candidate_bytes':int(p['bytes']),'baseline_middle':float(z['mid_cdf']),'candidate_middle':float(p['mid_cdf'])};item['native_file_reduction_percent']=100*(1-item['candidate_bytes']/item['native_baseline_bytes']);compression.append(item)
speed=[]
if (b/'speed.csv').exists():
 rows=list(csv.DictReader((b/'speed.csv').open()));assert len(rows)==24
 for ds in ['new','r16confirm']:
  group=[x for x in rows if x['dataset']==ds];pairs={(int(x['round']),int(x['mode'])):x for x in group};item={'dataset':ds,'rounds':6}
  for direction in ['encode','decode']:
   gains=[100*(float(pairs[i,0][direction+'_seconds'])/float(pairs[i,2][direction+'_seconds'])-1) for i in range(6)]
   item[direction]={'paired_throughput_gain_percent_median':statistics.median(gains),'paired_throughput_gain_percent_each_round':gains,'faster_rounds':sum(x>0 for x in gains),'baseline_ply_per_second_median':statistics.median(float(x['plies'])/float(x[direction+'_seconds']) for x in group if x['mode']=='0'),'candidate_ply_per_second_median':statistics.median(float(x['plies'])/float(x[direction+'_seconds']) for x in group if x['mode']=='2')}
  speed.append(item)
out={'compression':compression,'speed':speed};(b/'summary.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
