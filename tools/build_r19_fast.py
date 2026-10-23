#!/usr/bin/env python3
"""Build isolated aggregate-initialization candidate, preserving frozen R19."""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',default='build-r19-fast');a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
expected=(r/'tools/r19_model.hpp').read_text().replace('PriorStep s;static_cast<MidStep&>(s)=mid_step_legacy(p,prev,own,ply,1,MidProfile::R10ComboTact4Full,c.core);','PriorStep s{mid_step_legacy(p,prev,own,ply,1,MidProfile::R10ComboTact4Full,c.core)};')
assert expected==(r/'tools/r19_fast_model.hpp').read_text()
assert (r/'tools/r19_fast.cpp').read_text()==(r/'tools/r19_prior.cpp').read_text().replace('#include "r19_model.hpp"','#include "r19_fast_model.hpp"')
f=['g++','-std=c++20','-O3','-DNDEBUG','-DDMT_FAST_TACTICAL','-DDMT_DENSE_CATEGORY_UPDATES','-DDMT_FEATURE_LUT','-I'+str(r/'include')]
subprocess.run([*f,'-c',str(r/'src/rmc.cpp'),'-o',str(o/'rmc.o')],check=True)
subprocess.run([*f,str(r/'tools/r19_fast.cpp'),str(o/'rmc.o'),'-o',str(o/'r19_fast')],check=True)
print(o/'r19_fast')
