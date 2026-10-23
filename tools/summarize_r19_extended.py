#!/usr/bin/env python3
"""Block-level timing analysis; do not treat ABBA calls as independent samples."""
import argparse,csv,json,math,statistics
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser();p.add_argument('study');a=p.parse_args();d=Path(a.study);protocol=json.loads((d/'protocol.json').read_text());sessions=json.loads((d/'sessions.json').read_text());assert len(sessions)==6
ratio=lambda values:100*math.expm1(statistics.mean(values))
all_logs={direction:[] for direction in ['encode','decode']};strata=[]
for session in sessions:
 rows=list(csv.DictReader((d/(session['session']+'.csv')).open()));assert len(rows)==64
 result={'session':session['session'],'cpu':session['cpu'],'dataset':session['dataset'],'nr_throttled_delta':int(session['cgroup_after'].get('nr_throttled',0))-int(session['cgroup_before'].get('nr_throttled',0))}
 for direction in ['encode','decode']:
  item={}
  for metric in ['cpu_seconds','wall_seconds']:
   logs=[]
   for rnd in range(8):
    group=[x for x in rows if int(x['round'])==rnd and x['direction']==direction];assert len(group)==4
    base=sum(float(x[metric]) for x in group if x['mode']=='0');candidate=sum(float(x[metric]) for x in group if x['mode']=='3');assert base>0 and candidate>0;logs.append(math.log(base/candidate))
   item[metric]={'gain_geomean_percent':ratio(logs),'gain_median_percent':statistics.median(100*math.expm1(x) for x in logs),'faster_blocks':sum(x>0 for x in logs),'block_gains_percent':[100*math.expm1(x) for x in logs]}
   if metric=='cpu_seconds':all_logs[direction].append(logs)
  for mode,label in [('0','r18'),('3','r19')]:item[label+'_cpu_ply_per_second_median']=statistics.median(int(x['plies'])/float(x['cpu_seconds']) for x in rows if x['direction']==direction and x['mode']==mode)
  result[direction]=item
 strata.append(result)
# Exploratory hierarchical bootstrap: resample sessions, then whole ABBA blocks.
rng=np.random.default_rng(20261023);pooled={}
for direction,values in all_logs.items():
 values=np.asarray(values);assert values.shape==(6,8);sids=rng.integers(0,6,size=(20000,6));bids=rng.integers(0,8,size=(20000,6,8));samples=values[sids[:,:,None],bids].mean(axis=(1,2));lo,hi=np.quantile(samples,[0.025,0.975]);pooled[direction]={'equal_session_weight_gain_percent':100*math.expm1(float(values.mean())),'exploratory_95_percent_interval':[100*math.expm1(float(lo)),100*math.expm1(float(hi))],'positive_strata':int(sum(np.mean(x)>0 for x in values)),'faster_blocks':int((values>0).sum()),'total_blocks':48,'note':'Exploratory interval for these serial sessions; not proof for other CPUs, workloads, or independent time periods'}
native=list(csv.DictReader((d/'native-cli.csv').open()));assert len(native)==24;direct={}
for metric in ['encode_seconds','decode_seconds','process_cpu_seconds','process_wall_seconds']:
 logs=[]
 for rnd in range(6):
  group=[x for x in native if int(x['round'])==rnd];assert len(group)==4
  base=sum(float(x[metric]) for x in group if x['variant']=='r18');candidate=sum(float(x[metric]) for x in group if x['variant']=='r19');logs.append(math.log(base/candidate))
 direct[metric]={'gain_geomean_percent':ratio(logs),'gain_median_percent':statistics.median(100*math.expm1(x) for x in logs),'faster_blocks':sum(x>0 for x in logs),'block_gains_percent':[100*math.expm1(x) for x in logs]}
for direction in ['encode','decode']:
 for variant in ['r18','r19']:direct[variant+'_'+direction+'_ply_per_second_median']=statistics.median(int(x['plies'])/float(x[direction+'_seconds']) for x in native if x['variant']==variant)
consistent=all(pooled[direction]['positive_strata']==6 and pooled[direction]['exploratory_95_percent_interval'][0]>0 for direction in ['encode','decode']) and all(direct[key]['gain_geomean_percent']>0 for key in ['encode_seconds','decode_seconds','process_cpu_seconds'])
output={'strata':strata,'pooled_cpu':pooled,'actual_cli':direct,'consistent_speed_win_on_this_study':consistent,'cgroup_throttling_observed':any(x['nr_throttled_delta']>0 for x in strata) or any(int(x['nr_throttled_delta'])>0 for x in native),'native_cpu_threshold_60k_both_directions':all(direct['r19_'+direction+'_ply_per_second_median']>=60000 for direction in ['encode','decode'])}
(d/'summary.json').write_text(json.dumps(output,indent=2)+'\n');print(json.dumps(output,indent=2))
