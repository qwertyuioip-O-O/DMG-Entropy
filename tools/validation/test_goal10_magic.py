#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
a=(r/'src/rmc.cpp').read_text();b=(r/'src/rmc_g10magic.cpp').read_text()
a=a[a.index('uint64_t sliding('):a.index('unsigned width(')].replace('sliding(','sliding_reference(')
b=b[b.index('uint64_t sliding('):b.index('unsigned width(')]
code='#include "goal10_magic_data.hpp"\n#include <array>\n#include <cstdint>\n#include <bit>\n#include <random>\n#include <cassert>\n#include <iostream>\nstruct Position{std::array<int8_t,64> board{};std::array<uint64_t,2> occupied{};};\nuint64_t bit(int s){return uint64_t(1)<<s;}\nint color(int8_t p){return p<0?1:0;}\n'+a+b+r"""
int main(){std::mt19937_64 rng(8021);unsigned checks=0;
 auto test=[&](uint64_t white,uint64_t black){
  black&=~white;Position p;p.occupied={white,black};
  for(int sq=0;sq<64;++sq)p.board[sq]=(white&bit(sq))?1:(black&bit(sq))?-1:0;
  for(int sq=0;sq<64;++sq)for(int o=0;o<2;++o)for(int d=0;d<2;++d){assert(sliding(p,sq,o,d)==sliding_reference(p,sq,o,d));++checks;}
 };
 for(unsigned bishop=0;bishop<2;++bishop)for(unsigned sq=0;sq<64;++sq){
  uint64_t mask=goal10_magic[bishop][sq].mask,subset=0;
  do{
   Position p;p.occupied={0,subset};for(unsigned j=0;j<64;++j)if(subset&bit(j))p.board[j]=-1;
   assert(sliding(p,sq,!bishop,bishop)==sliding_reference(p,sq,!bishop,bishop));++checks;
   subset=(subset-mask)&mask;
  }while(subset);
 }
 test(0,0);test(~uint64_t(0),0);test(0,~uint64_t(0));
 for(unsigned n=0;n<4096;++n)test(rng(),rng());
 for(unsigned sq=0;sq<64;++sq){test(bit(sq),0);test(0,bit(sq));}
 std::cout<<checks<<" exact sliding masks PASS\n";
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O2','-I'+str(r/'src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 print(subprocess.run([str(p/'test')],check=True,capture_output=True,text=True).stdout,end='')
