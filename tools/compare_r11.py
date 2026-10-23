#!/usr/bin/env python3
"""Build bundled immutable baseline and compare all 243 legacy archive bytes."""
import argparse,subprocess,csv
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--build',default='build-r11');ap.add_argument('--out',default='comparison-r11');ap.add_argument('--speed',action='store_true');args=ap.parse_args()
r=Path(__file__).resolve().parents[1];build=Path(args.build).resolve();out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
ref=r/'reference-dmt10';flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-I'+str(ref/'include')]
for name in ['entropy','rmc']:subprocess.run(flags+['-c',str(ref/'src'/f'{name}.cpp'),'-o',str(out/f'{name}.o')],check=True)
for name,tool in [('audit','dmt11_audit.cpp'),('speed','mid_speed.cpp')]:
 subprocess.run(flags+[str(r/'tools'/tool),str(out/'entropy.o'),str(out/'rmc.o'),'-o',str(out/name)],check=True)
bundles={'human23':r/'benchmarks-r4/human23.uci-bundle','wcc13':r/'benchmarks-r4/wcc2024_01_13.uci-bundle','random48':r/'benchmarks-r2/random48.uci-bundle'}
for dataset in ['human23','random48']:
 for name,exe in [('original',out/'audit'),('optimized',build/'dmt11_audit')]:
  subprocess.run([exe,bundles[dataset],out/f'{dataset}-{name}','243'],check=True)
 for id in range(243):
  a=(out/f'{dataset}-original'/f'{id}.dmt').read_bytes();b=(out/f'{dataset}-optimized'/f'{id}.dmt').read_bytes()
  if a!=b:raise RuntimeError(f'byte mismatch {dataset}/{id}')
 print(dataset+': all 243 profiles byte-identical')
if args.speed:
 with (out/'speed.csv').open('w',newline='') as f:
  writer=csv.writer(f);writer.writerow(['dataset','run','implementation','profile','games','plies','bits','bytes','enc_plies_s','dec_plies_s','mid_cdf','checksum'])
  for dataset,bundle in bundles.items():
   for trial in range(3):
    tasks=[('original',out/'speed',234),('optimized',build/'dmg_mid_speed',234),('optimized',build/'dmg_mid_speed',245),('optimized',build/'dmg_mid_speed',247)]
    if trial%2:tasks.reverse()
    for label,exe,id in tasks:
     row=subprocess.check_output([exe,str(id),'--bundle',bundle],text=True).strip().split(',')
     writer.writerow([dataset,trial,label,id]+row[1:]);f.flush()
 print('Timing data:',out/'speed.csv')
