#!/usr/bin/env python3
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];data=root/'data-r12';manifest=json.loads((data/'manifest.json').read_text());seen=set();total=0
for name,records in manifest['groups'].items():
 lines=[x.strip() for x in (data/(name+'.uci-bundle')).read_text().splitlines() if x.strip()]
 assert len(lines)==len(records)
 for line,record in zip(lines,records):
  digest=hashlib.sha256(line.encode()).hexdigest();assert digest==record['sha256'] and digest not in seen;seen.add(digest)
  assert len(line.split())==record['plies'];total+=record['plies']
 print(name,len(records),'games; hashes and plies match')
print('PASS:',len(seen),'unique games,',total,'plies')
