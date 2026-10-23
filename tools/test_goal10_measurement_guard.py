#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,csv,json,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument('--out',required=True);a=p.parse_args();r=Path(__file__).resolve().parents[1];results=[]
with tempfile.TemporaryDirectory() as temp:
 for name,times,expected,wrong_clock in [('stable',[1,1.01],0,False),('boundary',[1,1.25],0,False),('exceeds',[1,1.2501],2,False),('apparent_gain_but_unstable',[.5,1],2,False),('invalid_cpu_clock',[1,1],2,True)]:
  d=Path(temp)/name;d.mkdir();rows=[]
  for label in ['baseline','candidate']:
   for value in (times if label=='candidate' else [1.,1.]):
    row={'dataset':'test','label':label,('encode_cpu' if wrong_clock else 'encode'):value,('decode_cpu' if wrong_clock else 'decode'):value}
    if wrong_clock:row['process_cpu']=.1
    rows.append(row)
  f=d/'timings.csv'
  with f.open('w') as stream:w=csv.DictWriter(stream,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
  before=hashlib.sha256(f.read_bytes()).hexdigest()
  run=subprocess.run(['python3',str(r/'tools/audit_goal10_measurements.py'),'--dir',str(d),'--strict'],capture_output=True,text=True)
  assert run.returncode==expected,(name,run.stdout,run.stderr)
  q=json.loads((d/'measurement-quality.json').read_text());assert q['rows_removed']==0 and q['rows_preserved']==4
  assert hashlib.sha256(f.read_bytes()).hexdigest()==before
  results.append({'case':name,'returncode':run.returncode,'result':'PASS','raw_rows_unchanged':True})
out=Path(a.out).resolve();out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(results,indent=2));print('PASS 5 measurement guard cases; boundaries, unstable apparent gain, invalid CPU clock, raw-row preservation')
