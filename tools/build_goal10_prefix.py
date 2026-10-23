#!/usr/bin/env python3
"""Portable index ranks plus existing stable opportunity entry reuse."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-goal10-prefix');p.add_argument('--verify',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/generate_goal10_prefix.py')],check=True)
f=['g++','-std=c++20','-O3','-flto=1','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-DDMT_CACHE_OPPORTUNITY','-I'+str(r/'include')]
if a.verify:f+=['-DDMT_VERIFY_ENTRY_CACHE']
subprocess.run([*f,str(r/'tools/r19_g10prefix.cpp'),str(r/'src/rmc.cpp'),'-o',str(o/'g10prefix')],check=True)
print(o/'g10prefix')
