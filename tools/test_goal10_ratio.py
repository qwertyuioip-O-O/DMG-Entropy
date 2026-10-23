#!/usr/bin/env python3
"""Extract actual production helper and compare against native uint64 division."""
from pathlib import Path
import subprocess,tempfile,json,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument('--out',required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entropy_g10ratio.cpp').read_text();helper=s.split('// BEGIN GOAL10_EXACT_OP_RATIO\n',1)[1].split('// END GOAL10_EXACT_OP_RATIO',1)[0]
code='''#include <cstdint>
#include <limits>
#include <random>
#include <iostream>
#include <stdexcept>
'''+helper+'''
int main(){uint64_t checks=0;std::mt19937 rng(20261023);
 auto test=[&](uint32_t x,uint32_t y){uint64_t n=(uint64_t(x)+16384)*1024,d=uint64_t(y)+16384;
  if(goal10_exact_op_ratio(x,y)!=n/d)throw std::runtime_error("exact opportunity division mismatch");++checks;};
 const uint32_t edge[]={0,1,2,15,16,31,32,127,255,4095,4096,16383,16384,32767,65535,65536,999999999,1000000000,UINT32_MAX-1,UINT32_MAX};
 for(auto x:edge)for(auto y:edge)test(x,y);
 for(uint32_t x=0;x<2048;++x)for(uint32_t y=0;y<2048;++y)test(x,y);
 for(unsigned i=0;i<2000000;++i)test(rng(),rng());
 for(unsigned i=0;i<200000;++i){uint32_t y=rng();uint64_t d=uint64_t(y)+16384;
  uint64_t maxq=((uint64_t(UINT32_MAX)+16384)*1024)/d;
  uint64_t q=1+uint64_t(rng())%maxq;
  int64_t x=int64_t((q*d+511)/1024)-16384;
  for(int delta=-2;delta<=2;++delta)if(x+delta>=0 && uint64_t(x+delta)<=UINT32_MAX)test(uint32_t(x+delta),y);
 }
 std::cout<<"PASS "<<checks<<" exact opportunity quotient comparisons; full uint32 random domain, 2048x2048 exhaustive, integer-boundary probes\\n";
}
'''
with tempfile.TemporaryDirectory(prefix='goal10-ratio-') as temp:
 f=Path(temp)/'test.cpp';b=Path(temp)/'test';f.write_text(code)
 subprocess.run(['g++','-std=c++20','-O3',str(f),'-o',str(b)],check=True)
 result=subprocess.run([str(b)],check=True,capture_output=True,text=True)
(o/'ratio-oracle.json').write_text(json.dumps({'helper_sha256':hashlib.sha256(helper.encode()).hexdigest(),'stdout':result.stdout,'seed':20261023,'reference':'uint64 numerator / uint64 denominator'},indent=2));print(result.stdout,end='')
