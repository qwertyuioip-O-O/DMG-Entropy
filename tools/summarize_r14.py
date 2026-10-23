#!/usr/bin/env python3
import argparse,csv,json,statistics
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--directory',type=Path);args=ap.parse_args();r=Path(__file__).resolve().parents[1];out=args.directory or r/'benchmarks-r14'
results=[]
for path in sorted(out.glob('*.csv')):
    if path.name.startswith(('dev-loss','loss-groups','speed-','summary')):continue
    rows=list(csv.DictReader(path.open()))
    if not rows or 'features' not in rows[0]:continue
    candidates=[x for x in rows if x['features']=='7' and x['rate']=='1']
    if not candidates:continue
    base=next(x for x in rows if x['features']=='-2');fast=next(x for x in rows if x['features']=='-1');c=candidates[0];controls=[x for x in rows if x['features']=='0' and x['rate']=='1']
    results.append({'file':path.name,'games':int(base['games']),'plies':int(base['plies']),'bits250':int(base['bits']),'bits95':int(fast['bits']),'bits252':int(c['bits']),'bytes250':int(base['bytes']),'bytes95':int(fast['bytes']),'bytes252':int(c['bytes']),'delta_bits_vs250':int(c['bits'])-int(base['bits']),'delta_bytes_vs250':int(c['bytes'])-int(base['bytes']),'feature_delta_bits':int(c['bits'])-int(controls[0]['bits']) if controls else None,'mid_cdf250':float(base['mid_cdf']),'mid_cdf252':float(c['mid_cdf'])})
(out/'summary.json').write_text(json.dumps(results,indent=2))
speed={}
for label in ['select','heldout']:
    path=out/f'speed-{label}.csv'
    if not path.exists():continue
    rows=list(csv.DictReader(path.open()));assert len(rows)==36
    summary={}
    for column in ['encode_wall_s','decode_wall_s','encode_cpu_s','decode_cpu_s']:
        summary[column]={}
        for baseline in [0,1]:
            paired=[]
            for round in range(12):
                xs={int(x['variant']):float(x[column]) for x in rows if int(x['round'])==round};assert len(xs)==3
                paired.append((xs[baseline]/xs[2]-1)*100)
            summary[column][str(baseline)]={'median_pct':statistics.median(paired),'min_pct':min(paired),'max_pct':max(paired),'candidate_faster_rounds':sum(x>0 for x in paired),'rounds':12,'paired_pct':paired}
    rates={}
    for v in range(3):
        xs=[x for x in rows if int(x['variant'])==v]
        rates[str(v)]={c:int(xs[0]['plies'])/statistics.median(float(x[c]) for x in xs) for c in ['encode_wall_s','decode_wall_s']}
    speed[label]={'variant0':'250','variant1':'251/mask95','variant2':'252/mask95/features7/rate1','paired':summary,'median_wall_plies_per_s':rates}
(out/'speed-summary.json').write_text(json.dumps(speed,indent=2))
lines=['# DMT14 speed measurements','',
       'O3 + FAST_TACTICAL + FEATURE_LUT. No PGO/LTO/opportunity cache. Three variants, 12 balanced rounds, 2 encode/decode repetitions per variant per round. All compilers and other tests ended before timing. Encode determinism and decode equality were checked during every round. Percentages are medians of paired throughput ratios; a single shared host, not cross-hardware evidence.','',
       '| Set | Baseline | Encode throughput change | Decode throughput change | Faster rounds E/D |',
       '|---|---|---:|---:|---:|']
for label,s in speed.items():
    for base in [0,1]:
        e=s['paired']['encode_wall_s'][str(base)];d=s['paired']['decode_wall_s'][str(base)]
        lines.append(f"| {label} | {s['variant'+str(base)]} | {e['median_pct']:+.2f}% | {d['median_pct']:+.2f}% | {e['candidate_faster_rounds']}/12, {d['candidate_faster_rounds']}/12 |")
lines+=['','Raw per-round wall/CPU measurements, full ranges and absolute median rates are in speed-*.csv and speed-summary.json. Negative changes mean the candidate is slower.']
(out/'speed-results.md').write_text('\n'.join(lines)+'\n');print('\n'.join(lines))
