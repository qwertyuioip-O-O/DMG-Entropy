#!/usr/bin/env python3
"""Test the actual source ExactDivisor against native division, not a duplicate."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',default='test-goal10-div');a=p.parse_args()
r=Path(__file__).resolve().parents[2];o=Path(a.out).resolve();o.mkdir(parents=True,exist_ok=True)
s=(r/'src/entropy_g10div.cpp').read_text();start=s.index('struct ExactDivisor {');end=s.index('V6Weights v6_quantize',start);implementation=s[start:end]
program='''#include <cstdint>
#include <climits>
#include <random>
#include <iostream>
#include <stdexcept>
'''+implementation+'''
int main(){std::mt19937_64 rng(20261023);uint64_t checks=0;
 auto check=[&](uint64_t n,uint64_t d){ExactDivisor divide(d);if(divide(n)!=n/d)throw std::runtime_error("exact divider mismatch");++checks;};
 for(uint64_t d=1;d<=32768;++d)for(uint64_t n:{uint64_t(0),d-1,d,d+1,uint64_t(UINT64_MAX),uint64_t(UINT64_MAX)-d})check(n,d);
 for(unsigned i=0;i<1000000;++i){uint64_t d=rng()|1;check(rng(),d);uint64_t total=1+rng()%16000;check((rng()%16001)*4096+total/2,total);}
 std::cout<<"PASS "<<checks<<" actual ExactDivisor/native division comparisons\\n";
}
'''
(o/'actual-divider-test.cpp').write_text(program);subprocess.run(['g++','-std=c++20','-O2',str(o/'actual-divider-test.cpp'),'-o',str(o/'actual-divider-test')],check=True)
run=subprocess.run([str(o/'actual-divider-test')],check=True,capture_output=True,text=True);(o/'divider-oracle.txt').write_text(run.stdout);print(run.stdout,end='')
