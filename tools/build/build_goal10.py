#!/usr/bin/env python3
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-goal10');p.add_argument('--verify',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/generation/generate_goal10.py')],check=True)
f=['g++','-std=c++20','-O3','-flto=1','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include'),'-I'+str(r/'src')]
for label in ['g10rank','g10mob']+(['g10verify'] if a.verify else []):
 source='g10mob' if label=='g10verify' else label;flags=['-DDMT_VERIFY_GOAL10'] if label=='g10verify' else []
 subprocess.run([*f,*flags,str(r/f'tools/native/r19_{source}.cpp'),str(r/'src/rmc.cpp'),'-o',str(o/label)],check=True)
 print(o/label,flush=True)
