#!/usr/bin/env python3
"""Recheck compression and correctness. Existing data are development/reused data."""
import argparse,csv,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--build',default='build-r11');ap.add_argument('--out',default='reproduced-r11');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];build=Path(args.build).resolve();out=Path(args.out).resolve();out.mkdir(parents=True,exist_ok=True)
for name in ['dmt_default_tests','dmt10_tests','dmt11_tests','dmt11_kernel_tests']:subprocess.run([build/name],check=True)
bundles={'human23':'benchmarks-r4/human23.uci-bundle','human4':'benchmarks-r4/human4.uci-bundle','wcc13':'benchmarks-r4/wcc2024_01_13.uci-bundle','random48':'benchmarks-r2/random48.uci-bundle','fischer3_reused':'benchmarks-r10/fischer_3_final.uci-bundle'}
rows=[]
for name,bundle in bundles.items():
 text=subprocess.check_output([build/'dmt9_experiments',name,root/bundle,'119,182,234,243,244,245,246,247,248'],text=True)
 rows.extend(csv.DictReader(text.splitlines()))
with (out/'compression.csv').open('w',newline='') as f:
 writer=csv.DictWriter(f,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
print('Saved:',out/'compression.csv')
