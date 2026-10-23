#!/usr/bin/env python3
"""Reproduce v0.5 paired per-game differences (not evidence of a large random corpus)."""
import csv, pathlib, random

ROOT=pathlib.Path(__file__).resolve().parents[1]
records=list(csv.DictReader((ROOT/'results-v0.5/per_game.csv').open()))
by_game={}
for r in records: by_game.setdefault(r['game'],{})[r['mode']]=r
baseline='tactical-check'
models=['phase','phase-tactical','phase-check','phase-forcing','phase-opening',
        'phase-middle','phase-endgame','check-signal','check-signal-light']
print('mode,plies,payload_bits,full_bytes,bits_per_ply,full_bits_per_ply,better_games,worse_games,bootstrap_95_lo,bootstrap_95_hi')
for mode in [baseline]+models:
    x=[(int(g[baseline]['bits']),int(g[mode]['bits']),int(g[mode]['plies']),
        int(g[mode]['file_bytes'])) for g in by_game.values()]
    n=sum(t[2] for t in x)
    bits=sum(t[1] for t in x);total_bytes=sum(t[3] for t in x)
    rng=random.Random(24145)
    samples=[]
    for i in range(10000):
        s=rng.choices(x,k=len(x))
        samples.append(sum(a-b for a,b,_,_ in s)/sum(t[2] for t in s))
    samples.sort()
    print(f'{mode},{n},{bits},{total_bytes},{bits/n:.6f},{8*total_bytes/n:.6f},'
          f'{sum(a>b for a,b,_,_ in x)},{sum(a<b for a,b,_,_ in x)},'
          f'{samples[250]:.6f},{samples[9750]:.6f}')
