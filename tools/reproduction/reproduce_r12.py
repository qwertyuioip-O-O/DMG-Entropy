#!/usr/bin/env python3
import argparse,csv,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--build',default='build-r12');ap.add_argument('--out',default='reproduced-r12');ap.add_argument('--search',action='store_true');ap.add_argument('--full',action='store_true');args=ap.parse_args()
r=Path(__file__).resolve().parents[2];b=Path(args.build).resolve();out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',r/'tools/audit_r12_data.py'],check=True)
for test in ['dmt_default_tests','dmt10_tests','dmt11_tests','dmt12_tests','dmt11_kernel_tests']:
 program=b/test;program.chmod(program.stat().st_mode|0o111);subprocess.run([program],check=True)
if args.full:
 program=b/'dmg_mid_tests';program.chmod(program.stat().st_mode|0o111);subprocess.run([program],check=True)
bundles={'human23':r/'benchmarks-r4/human23.uci-bundle','human4':r/'benchmarks-r4/human4.uci-bundle','wcc13':r/'benchmarks-r4/wcc2024_01_13.uci-bundle','random48':r/'benchmarks-r2/random48.uci-bundle'}
for name in ['new_dev','new_select','new_sealed','kasparov_new']:bundles[name]=r/'data-r12'/(name+'.uci-bundle')
rows=[]
for name,bundle in bundles.items():
 text=subprocess.check_output([b/'dmt9_experiments',name,bundle,'119,234,247,250'],text=True);rows.extend(csv.DictReader(text.splitlines()))
with (out/'compression.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
if args.search:
 for round,dataset in [('round1','human23'),('round1','new_dev'),('round2','new_dev'),('selection','new_select')]:
  config=r/'benchmarks-r12'/(('selection_configs' if round=='selection' else round+'_configs')+'.csv')
  result=subprocess.check_output([b/'r12_search',dataset,bundles[dataset],config],text=True)
  (out/(round+'_'+dataset+'.csv')).write_text(result)
print('Results:',out)
