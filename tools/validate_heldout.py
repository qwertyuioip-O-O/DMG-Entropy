#!/usr/bin/env python3
"""Held-out, deterministic, byte-for-byte chess UCI codec comparison.

Run from project root: python3 tools/validate_heldout.py --binary build/rmc
No Python packages needed. Source games in examples/heldout/ are public, manually
transcribed, and checked with the project's legal-move-aware SAN converter.
"""
from __future__ import annotations
import argparse
import csv
import pathlib
import random
import statistics
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
MODES = ["smc", "dmg", "uniform", "static", "adaptive", "positional",
         "tactical", "checkfilter", "tactical-check", "rich-check",
         "phase", "phase-tactical", "phase-check", "phase-forcing",
         "phase-opening", "phase-middle", "phase-endgame",
         "check-signal", "check-signal-light", "best"]

def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True)

def read_manifest():
    with (ROOT / 'examples/heldout/SOURCES.csv').open(newline='') as file:
        return list(csv.DictReader(file))

def pooled(rows, mode):
    n = sum(int(r['plies']) for r in rows if r['mode'] == mode)
    b = sum(int(r['bits']) for r in rows if r['mode'] == mode)
    f = sum(int(r['file_bytes']) for r in rows if r['mode'] == mode)
    return (n, b/n, (f*8/n), f)

def game_bootstrap(records, mode_a, mode_b, seed=24145):
    a = {r['game']: r for r in records if r['mode'] == mode_a}
    b = {r['game']: r for r in records if r['mode'] == mode_b}
    names = sorted(a)
    rng = random.Random(seed)
    samples = []
    for _ in range(3000):
        chosen = rng.choices(names, k=len(names))
        total = sum(a[k]['plies'] for k in chosen)
        difference = sum(a[k]['bits'] - b[k]['bits'] for k in chosen)/total
        samples.append(difference)
    samples.sort()
    return (samples[int(.025*len(samples))], samples[int(.975*len(samples))])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', default=str(ROOT/'build/rmc'))
    parser.add_argument('--out', default=str(ROOT/'results'))
    args = parser.parse_args()
    binary = str(pathlib.Path(args.binary).resolve())
    outdir = pathlib.Path(args.out).resolve()
    outdir.mkdir(parents=True, exist_ok=True)
    games = read_manifest()
    records = []
    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        for game in games:
            src = ROOT/'examples/heldout'/(game['file']+'.uci')
            original = src.read_text().split()
            for mode in MODES:
                dest = tmp / 'game.rmc'
                restored = tmp / 'restored.uci'
                run(binary, 'encode', str(src), str(dest), mode)
                stream = dest.read_bytes()
                assert stream[:4] == b'RMC1', (game['file'], mode)
                n, bits = struct.unpack_from('<II', stream, 8)
                assert n == len(original)
                assert len(stream) == 16+(bits+7)//8, (game['file'], mode)
                run(binary, 'decode', str(dest), str(restored))
                assert restored.read_text().split() == original, (game['file'],mode)
                if mode in ('tactical-check', 'phase', 'phase-tactical', 'phase-check', 'phase-forcing', 'phase-opening', 'phase-middle', 'phase-endgame', 'check-signal', 'check-signal-light'):
                    run(binary, 'encode', str(src), str(dest), mode)
                    assert dest.read_bytes() == stream, (game['file'], 'nondeterministic encoder')
                records.append({'game':game['file'], 'group':game['category'],
                                'mode':mode, 'plies':n, 'bits':bits,
                                'file_bytes':len(stream), 'bits_per_ply':round(bits/n,6)})
    with (outdir/'per_game.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f,fieldnames=records[0].keys())
        writer.writeheader(); writer.writerows(records)
    print('VERIFIED GAMES', len(games), 'PLIES', sum(r['plies'] for r in records if r['mode'] == MODES[0]))
    print('group,mode,plies,payload_bits_per_ply,total_file_bits_per_ply')
    summary=[]
    for label in ('Historical','Modern','All'):
        subset = [r for r in records if label == 'All' or r['group'] == label]
        for mode in MODES:
            n, rate, full, _ = pooled(subset,mode)
            row = {'group':label, 'mode':mode,'plies':n,'payload_bits_per_ply':round(rate,6),
                   'total_file_bits_per_ply':round(full,6)}
            summary.append(row)
            print(f'{label},{mode},{n},{rate:.5f},{full:.5f}')
    with (outdir/'summary.csv').open('w', newline='') as f:
        writer=csv.DictWriter(f,fieldnames=summary[0].keys());writer.writeheader();writer.writerows(summary)
    for label in ('Historical','Modern','All'):
        subset = [r for r in records if label == 'All' or r['group'] == label]
        lo,hi = game_bootstrap(subset,'uniform','tactical-check')
        print(f'{label} uniform-minus-tactical-check (bits/ply), game bootstrap 95%: {lo:.4f} to {hi:.4f}')
    print('ALL VERIFIED: true')

if __name__ == '__main__':
    main()
