#!/usr/bin/env python3
"""Exercise actual generated prefix copy code, including empty/full/self copies."""
from pathlib import Path
import subprocess,tempfile,json,os
r=Path(__file__).resolve().parents[2]
s=(r/'src/entropy_g10prefix.cpp').read_text();a=s.index('struct Goal10EntryPrefix {');b=s.index('struct V8Step {',a)
code='#include <array>\n#include <algorithm>\n#include <cassert>\nstruct V7Patterns {struct Opportunity {unsigned x;};};\n'+s[a:b]+r"""
int main(){
 std::array<V7Patterns::Opportunity,256> values{};
 for(unsigned n=0;n<=256;++n){
  Goal10EntryPrefix a;
  for(unsigned i=0;i<n;++i)a[i]=&values[i];
  a.size=n;
  Goal10EntryPrefix b(a),c;c=a;c=c;
  Goal10EntryPrefix d;d=b;d=a;
  for(auto p:{&b,&c,&d}){assert(p->size==n);for(unsigned i=0;i<n;++i)assert((*p)[i]==&values[i]);}
  Goal10EntryPrefix empty;c=empty;assert(c.size==0);
 }
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O1','-g','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':os.environ.get('ASAN_OPTIONS','')+':detect_leaks=0'})
print(json.dumps({'active_lengths':257,'copy_construction':True,'assignment':True,'self_assignment':True,'clear_assignment':True,'asan_ubsan':'PASS'}))
