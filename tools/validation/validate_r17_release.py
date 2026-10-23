from pathlib import Path
import subprocess,json,csv,hashlib,platform
r=Path('work/DMG-Entropy-DMT17');out=r/'benchmarks-r17';exe=Path('work/build-r17-final/r17_prior').resolve();exe.chmod(0o755);prior=out/'prior-frozen.txt'
with (out/'final-codec-tests.log').open('w') as f:subprocess.run([str(exe),'test',str(r/'benchmarks-r4/human4.uci-bundle'),'builtin'],stdout=f,check=True)
# The final builtin coefficients and the efficient mixture table swap must be exact.
for name,rel in [('new','data-r17/heldout.uci-bundle'),('wcc','benchmarks-r4/wcc2024_01_13.uci-bundle'),('human','benchmarks-r4/human4.uci-bundle')]:
 s=subprocess.check_output([str(exe),'eval',str(r/rel),'builtin'],text=True);assert s==(out/(name+'.csv')).read_text();(out/('final-'+name+'.csv')).write_text(s)
print('final matrix identity PASS',flush=True)
for source in ['blitz1300','blitz_open','rapid2000','superblitz1500']:
 with (out/('source-'+source+'.csv')).open('w') as f:subprocess.run([str(exe),'eval-frozen',str(r/'data-r17'/f'{source}.uci-bundle'),'builtin'],stdout=f,check=True)
with (out/'blocks.csv').open('w') as f:subprocess.run([str(exe),'blocks',str(r/'data-r17/heldout.uci-bundle'),'builtin'],stdout=f,check=True)
print('blocks and sources PASS',flush=True)
for name,rel in [('new','data-r17/heldout.uci-bundle'),('wcc','benchmarks-r4/wcc2024_01_13.uci-bundle'),('r16confirm','data-r16-confirm/heldout.uci-bundle')]:
 subprocess.run([str(exe),'pack',str(r/rel),'builtin','2',str(out/(name+'-frozen.d17'))],check=True)
 native=Path('work/build-r16-final/r16_dump').resolve();native.chmod(0o755)
 subprocess.run([str(native),str(r/rel),str(out/(name+'-baseline.dmt')),'0','2','128'],check=True,stdout=subprocess.DEVNULL)
 # Builtin and sidecar prior give identical streams.
 subprocess.run([str(exe),'pack',str(r/rel),str(prior),'2',str(out/(name+'-sidecar.d17'))],check=True)
 assert (out/(name+'-frozen.d17')).read_bytes()==(out/(name+'-sidecar.d17')).read_bytes()
 subprocess.run([str(exe),'unpack',str(out/(name+'-frozen.d17')),'builtin',str(out/(name+'-decoded.uci-bundle'))],check=True)
 assert [' '.join(x.split()) for x in (r/rel).read_text().splitlines()]==[' '.join(x.split()) for x in (out/(name+'-decoded.uci-bundle')).read_text().splitlines()]
print('pack/unpack and builtin identity PASS',flush=True)
meta={'rounds':6,'repetitions':3,'warm_roundtrip':True,'balanced_order':True,'platform':platform.platform(),'source_sha256':hashlib.sha256((r/'tools/r17_prior.cpp').read_bytes()).hexdigest(),'no_parallel_build_or_tests':True,'baseline':'same-executable mode0, payload-identical to R16 strong','candidate':'frozen mode2, selected before new data metrics'}
(out/'timing.json').write_text(json.dumps(meta,indent=2));rows=[]
# No work except these sequential warm/encode/decode comparisons beyond here.
with (out/'speed.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['dataset','round','mode','plies','encode_seconds','decode_seconds']);f.flush()
 for dataset,rel in [('new','data-r17/heldout.uci-bundle'),('r16confirm','data-r16-confirm/heldout.uci-bundle')]:
  for rnd in range(6):
   for mode in ([0,2] if rnd%2==0 else [2,0]):
    s=subprocess.check_output([str(exe),'speed',str(r/rel),'builtin',str(mode)],text=True).strip().split(',');w.writerow([dataset,rnd,*s]);f.flush()
   print('timing',dataset,rnd+1,'/6',flush=True)
print('DONE',flush=True)
