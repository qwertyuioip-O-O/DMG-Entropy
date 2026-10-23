#!/usr/bin/env python3
"""Balanced cross-build timing: dense252, sparse252 and sparse254."""
import argparse,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--candidate','--sparse',dest='candidate',type=Path,required=True);ap.add_argument('--dense',type=Path,required=True);ap.add_argument('--directory',type=Path,required=True);ap.add_argument('--kernel',default='branch_skip');ap.add_argument('--rounds',type=int,default=12);args=ap.parse_args()
r=Path(__file__).resolve().parents[2];args.directory.mkdir(parents=True,exist_ok=True)
import json
(args.directory/'timing-kernel.json').write_text(json.dumps({'kernel':args.kernel,'rounds':args.rounds,'baseline_executable':str(args.dense),'candidate_executable':str(args.candidate)},indent=2))
for exe in [args.candidate,args.dense]:exe.chmod(0o755)
variants=[(args.dense,252),(args.candidate,252),(args.candidate,254)]
for label,bundle in [('heldout','data-r16/heldout.uci-bundle'),('confirm','data-r16-confirm/heldout.uci-bundle')]:
    rows=['round,order,variant,profile,layout,rate,cap,plies,bits,bytes,encode_wall_s,decode_wall_s,encode_cpu_s,decode_cpu_s']
    for round in range(args.rounds):
        for order in range(3):
            variant=((2-order if round%2 else order)+round//2)%3;exe,profile=variants[variant]
            p=subprocess.run([str(exe),str(profile),str(r/bundle)],capture_output=True,text=True,check=True)
            rows.append(f'{round},{order},{variant},'+p.stdout.strip())
        if (round+1)%6==0 or round+1==args.rounds:print(label,'rounds',round+1,'/',args.rounds,flush=True)
    (args.directory/f'speed-{label}.csv').write_text('\n'.join(rows)+'\n');print('timing complete',label,flush=True)
