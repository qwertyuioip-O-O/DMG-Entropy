#!/usr/bin/env python3
"""Run the 8-round in-process ABBA diagnostic pinned to one allowed CPU."""
import argparse,subprocess,os,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('executable');p.add_argument('bundle');p.add_argument('--out',default='paired-results');a=p.parse_args();r=Path(__file__).resolve().parents[1];out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
exe=Path(a.executable).resolve();original=os.sched_getaffinity(0);cpu=min(original)
try:
 os.sched_setaffinity(0,{cpu})
 with (out/'paired-speed.csv').open('w') as f:subprocess.run([str(exe),str(Path(a.bundle).resolve())],stdout=f,check=True)
finally:os.sched_setaffinity(0,original)
(out/'paired-timing.json').write_text(json.dumps({'rounds':8,'order':'ABBA/BAAB alternating per direction/round','cpu_affinity':[cpu],'source_sha256':hashlib.sha256((r/'tools/r19_pair_speed.cpp').read_bytes()).hexdigest(),'scope':'in-process diagnostic, not replacement for released CLI timing'},indent=2))
