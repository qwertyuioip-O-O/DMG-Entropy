#!/usr/bin/env python3
"""Build the isolated research codec without changing any old archive format."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r19');p.add_argument('--paired',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
flags=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include'),'-I'+str(r/'src')]
subprocess.run([*flags,'-c',str(r/'src/rmc.cpp'),'-o',str(o/'rmc.o')],check=True)
subprocess.run([*flags,str(r/'tools/native/r19_prior.cpp'),str(o/'rmc.o'),'-o',str(o/'r19_prior')],check=True)
(o/'r19_prior').chmod(0o755);print(o/'r19_prior')
if a.paired:
 prefix=(r/'tools/native/r19_prior.cpp').read_text().split('int main(int argc,char**argv)')[0]
 if not (r/'tools/native/r19_pair_speed.cpp').read_text().startswith(prefix):raise RuntimeError('Paired codec copy differs from production codec')
 subprocess.run([*flags,str(r/'tools/native/r19_pair_speed.cpp'),str(o/'rmc.o'),'-o',str(o/'r19_pair_speed')],check=True)
 (o/'r19_pair_speed').chmod(0o755);print(o/'r19_pair_speed')
