#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,json
r=Path(__file__).resolve().parents[2]
s=(r/'src/entropy_g10guard.cpp').read_text();old=(r/'src/entropy_g10index.cpp').read_text()
def part(s,a,b):return s[s.index(a):s.index(b,s.index(a))]
code='#include <array>\n#include <cstdint>\n#include <stdexcept>\n#include <random>\n#include <iostream>\n#include <climits>\nvoid fail(const char* s){throw std::runtime_error(s);}\nconstexpr uint32_t V6_SCALE=16000;\nstruct V6Weights{std::array<uint16_t,256> weights{};uint32_t total=0;};\n'
code+=part(s,'struct ExactDivisor {','V6Weights v6_quantize(')
code+=part(s,'V6Weights v6_quantize(','V6Weights v6_uniform(')
code+=part(old,'V6Weights mid_guard(','int mid_piece_value(').replace('mid_guard(','reference_guard(')
code+=part(s,'V6Weights mid_guard(','int mid_piece_value(')
code+=r"""
int main(){std::mt19937 rng(1729);unsigned checks=0;
for(unsigned n=1;n<=256;++n)for(unsigned trial=0;trial<128;++trial){
 V6Weights w;
 for(unsigned i=0;i<n;++i){w.weights[i]=1+rng()%100;w.total+=w.weights[i];}
 if(trial==0){w.total=32767;w.weights.fill(1);w.weights[n-1]=32767-(n-1);}
 for(unsigned p:{0u,1u,2u,5u,12u,50u,99u,100u}){
  auto x=mid_guard(w,n,p),y=reference_guard(w,n,p);
  if(x.total!=y.total || x.weights!=y.weights)fail("differential mismatch");
  ++checks;
 }
}std::cout<<checks<<" exact distributions PASS\n";}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O2','-DDMT_VERIFY_GUARD',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 print(subprocess.run([str(p/'test')],check=True,capture_output=True,text=True).stdout,end='')
