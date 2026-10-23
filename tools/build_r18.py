#!/usr/bin/env python3
"""Build the isolated research codec without changing any old archive format."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r18');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include')]
subprocess.run([*flags,'-c',str(r/'src/rmc.cpp'),'-o',str(o/'rmc.o')],check=True)
subprocess.run([*flags,str(r/'tools/r18_prior.cpp'),str(o/'rmc.o'),'-o',str(o/'r18_prior')],check=True)
(o/'r18_prior').chmod(0o755);print(o/'r18_prior')
