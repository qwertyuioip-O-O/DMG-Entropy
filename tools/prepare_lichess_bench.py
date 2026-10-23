#!/usr/bin/env python3
"""Convert frozen UCI cohorts to SAN text accepted by the upstream Lichess codec."""
import argparse, hashlib, json
from pathlib import Path
import chess
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,default=ROOT/'benchmarks-chess-comparison-20261023/san');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 manifest={}
 for cohort in ('general','elite','random'):
  src=ROOT/f'data-cohorts-20261023/{cohort}/all.uci-bundle';dst=a.out/f'{cohort}.san.txt';games=plies=0
  with src.open() as fi,dst.open('w') as fo:
   for line in fi:
    uci=line.split();board=chess.Board();san=[]
    for move in uci:
     m=chess.Move.from_uci(move);san.append(board.san(m));board.push(m)
    fo.write(' '.join(san)+'\n');games+=1;plies+=len(san)
  h=hashlib.sha256(dst.read_bytes()).hexdigest();manifest[cohort]={'input':str(src.relative_to(ROOT)),'input_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'games':games,'plies':plies,'san_path':str(dst.relative_to(ROOT)),'san_sha256':h,'san_bytes':dst.stat().st_size}
  print(cohort,games,plies,dst.stat().st_size,h,flush=True)
 (a.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__=='__main__':main()
