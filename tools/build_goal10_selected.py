#!/usr/bin/env python3
"""Build one cumulative goal10 stage with retained LTO flags."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-goal10-selected');p.add_argument('--stage',choices=['g10rank','g10mob'],default='g10mob');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(r/'tools/generate_goal10.py')],check=True)
f=['g++','-std=c++20','-O3','-flto=1','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include')]
subprocess.run([*f,str(r/f'tools/r19_{a.stage}.cpp'),str(r/'src/rmc.cpp'),'-o',str(o/a.stage)],check=True)
print(o/a.stage)
