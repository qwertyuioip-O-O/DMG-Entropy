#!/usr/bin/env python3
"""Build immutable R11 baseline before timing, alternate execution order."""
import argparse,subprocess,csv
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--build',default='build-r12');ap.add_argument('--pgo-build');ap.add_argument('--lto-build');ap.add_argument('--out',default='benchmark-r12');ap.add_argument('--repeats',type=int,default=3);ap.add_argument('--audit',action='store_true');args=ap.parse_args()
r=Path(__file__).resolve().parents[1];b=Path(args.build).resolve();out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True);ref=r/'reference-r11'
flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-I'+str(ref/'include')]
for name in ['entropy','rmc']:subprocess.run(flags+['-c',str(ref/'src'/f'{name}.cpp'),'-o',str(out/f'{name}.o')],check=True)
for name,source in [('reference_speed','r12_speed.cpp'),('reference_audit','dmt11_audit.cpp')]:
 subprocess.run(flags+[str(r/'tools'/source),str(out/'entropy.o'),str(out/'rmc.o'),'-o',str(out/name)],check=True)
if args.audit:
 for dataset,bundle in [('human23',r/'benchmarks-r4/human23.uci-bundle'),('random48',r/'benchmarks-r2/random48.uci-bundle')]:
  for label,exe in [('reference',out/'reference_audit'),('current',b/'dmt11_audit')]:subprocess.run([exe,bundle,out/(dataset+'-'+label),'249'],check=True)
  for id in range(249):
   assert (out/(dataset+'-reference')/(str(id)+'.dmt')).read_bytes()==(out/(dataset+'-current')/(str(id)+'.dmt')).read_bytes()
  print('249 legacy profiles byte-identical:',dataset)
executables={'r11':out/'reference_speed','final':b/'r12_speed'}
if args.pgo_build:executables['pgo']=Path(args.pgo_build).resolve()/'r12_speed'
if args.lto_build:executables['lto']=Path(args.lto_build).resolve()/'r12_speed'
bundles={'wcc13':r/'benchmarks-r4/wcc2024_01_13.uci-bundle','new_sealed':r/'data-r12/new_sealed.uci-bundle','random48':r/'benchmarks-r2/random48.uci-bundle'}
with (out/'speed.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['dataset','run','build','profile','plies','bits','bytes','enc_wall','dec_wall','enc_cpu','dec_cpu'])
 for dataset,bundle in bundles.items():
  for repeat in range(args.repeats):
   tasks=[('r11',234),('final',234),('final',250)]+([('pgo',234),('pgo',250)] if args.pgo_build else [])+([('lto',234),('lto',250)] if args.lto_build else [])
   if repeat%2:tasks.reverse()
   for label,profile in tasks:
    row=subprocess.check_output([executables[label],str(profile),bundle],text=True).strip().split(',');w.writerow([dataset,repeat,label]+row);f.flush()
print('Timing data:',out/'speed.csv')
