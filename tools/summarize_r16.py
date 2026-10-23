#!/usr/bin/env python3
import argparse,csv,json,statistics
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--directory',type=Path);args=ap.parse_args();r=Path(__file__).resolve().parents[1];out=args.directory or r/'benchmarks-r16'
summary=[]
for path in sorted(out.glob('*.csv')):
    if path.name.startswith(('speed-','block-','summary')):continue
    rows=list(csv.DictReader(path.open()))
    if not rows or 'layout' not in rows[0]:continue
    base=next((x for x in rows if x['layout']=='-1'),None)
    strong=next((x for x in rows if (x['layout'],x['rate'],x['cap'])==('0','2','128')),None)
    source=next((x for x in rows if (x['layout'],x['rate'],x['cap'])==('1','2','128')),None)
    if not base or not strong:continue
    row={'file':path.name,'games':int(base['games']),'plies':int(base['plies']),'baseline252_bits':int(base['bits']),'strong_bits':int(strong['bits']),'baseline252_bytes':int(base['bytes']),'strong_bytes':int(strong['bytes']),'strong_delta_bits':int(strong['bits'])-int(base['bits']),'strong_delta_bytes':int(strong['bytes'])-int(base['bytes']),'baseline_mid_cdf':float(base['mid_cdf']),'strong_mid_cdf':float(strong['mid_cdf']),'baseline_mid_source':float(base['mid_source']),'strong_mid_source':float(strong['mid_source']),'baseline_mid_conditional':float(base['mid_conditional']),'strong_mid_conditional':float(strong['mid_conditional'])}
    if source:row.update(source_bits=int(source['bits']),source_bytes=int(source['bytes']),source_vs_matched_control_bits=int(source['bits'])-int(strong['bits']))
    if path.name=='heldout.csv':row['role']='Independent initial source-model test; became discovery data for stronger original-layout control'
    if path.name=='confirm.csv':row['role']='Independent confirmation of frozen original-layout strong control'
    summary.append(row)
(out/'summary.json').write_text(json.dumps(summary,indent=2))
campaign=json.loads((out/'timing-kernel.json').read_text()) if (out/'timing-kernel.json').exists() else {}
kernel=campaign.get('kernel','branch_skip');rounds=int(campaign.get('rounds',12))
speed={};names=['252/reference','252/'+kernel,'254/layout0/rate2/cap128/'+kernel]
for label in ['heldout','confirm']:
    path=out/f'speed-{label}.csv'
    if not path.exists():continue
    rows=list(csv.DictReader(path.open()));assert len(rows)==rounds*3
    for round in range(rounds):
        xs={int(x['variant']):x for x in rows if int(x['round'])==round};assert len(xs)==3
        assert (xs[0]['bits'],xs[0]['bytes'])==(xs[1]['bits'],xs[1]['bytes'])
        assert (xs[2]['profile'],xs[2]['layout'],xs[2]['rate'],xs[2]['cap'])==('254','0','2','128')
    comparisons={}
    for name,base,candidate in [('exact_kernel',0,1),('combined',0,2),('model_increment',1,2)]:
        columns={}
        for column in ['encode_wall_s','decode_wall_s','encode_cpu_s','decode_cpu_s']:
            values=[]
            for round in range(rounds):
                xs={int(x['variant']):float(x[column]) for x in rows if int(x['round'])==round};values.append((xs[base]/xs[candidate]-1)*100)
            columns[column]={'median_pct':statistics.median(values),'min_pct':min(values),'max_pct':max(values),'faster_rounds':sum(x>0 for x in values),'rounds':rounds,'paired_pct':values}
        comparisons[name]={'baseline':names[base],'candidate':names[candidate],'metrics':columns}
    rates={}
    for variant in range(3):
        xs=[x for x in rows if int(x['variant'])==variant];rates[names[variant]]={col:int(xs[0]['plies'])/statistics.median(float(x[col]) for x in xs) for col in ['encode_wall_s','decode_wall_s']}
    speed[label]={'comparisons':comparisons,'absolute_median_plies_s':rates}
(out/'speed-summary.json').write_text(json.dumps(speed,indent=2))
lines=['# R16 speed: exact optimization and model change separated','',
       'O3/FAST_TACTICAL/FEATURE_LUT, no PGO/LTO/opportunity cache. The campaign metadata identifies the reference and candidate executables/kernel. u32_final compares prior released R15 profile252 with the exact R16 uint32/loop-invariant kernel. The earlier branch_skip campaign compared frozen R16 dense and branch-skip kernels. Balanced rounds (count in campaign metadata) across three variants, each call warmed before timing and encoded/decoded twice. Determinism and roundtrip checked inside measurements. Compilers and other tests finished first. Paired throughput ratios, single shared host.','',
       '| Set | Change | Encode | Decode | Faster E/D |','|---|---|---:|---:|---:|']
for label,s in speed.items():
    for name,x in s['comparisons'].items():
        e=x['metrics']['encode_wall_s'];d=x['metrics']['decode_wall_s'];lines.append(f"| {label} | {name} | {e['median_pct']:+.2f}% | {d['median_pct']:+.2f}% | {e['faster_rounds']}/{rounds}, {d['faster_rounds']}/{rounds} |")
lines+=['','exact_kernel: same252 probabilities with the stated experimental kernel. combined: stronger original-layout calibration with the stated kernel versus reference252. model_increment: calibration change versus candidate-build252. Source-layout1 prototype is not the strong variant timed here. The u32_final campaign uses the prior released R15 implementation for reference252; the earlier branch_skip campaign used frozen R16 dense. Full ranges, CPU measurements, per-round ratios and absolute rates are in speed-summary.json and speed-*.csv.']
(out/'speed-results.md').write_text('\n'.join(lines)+'\n');print('\n'.join(lines))
