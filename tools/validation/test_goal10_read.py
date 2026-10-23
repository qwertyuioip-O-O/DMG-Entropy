#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
code=r"""
#include "experiments/goal10/goal10_op_table.hpp"
#include "experiments/goal10/goal10_read_table.hpp"
#include <random>
#include <cassert>
#include <iostream>
struct Value{uint32_t actual=0,expected=0;};
int main(){Goal10OpTable<Value> reference;Goal10ReadTable<Value> trial;
 for(unsigned k:{0u,1u,255u,256u,2097151u}){auto& v=trial.get_or_zero(k);assert(!v.actual&&!v.expected);assert(!trial.find(k));}
 std::mt19937 rng(319);
 for(unsigned i=0;i<100000;++i){unsigned k=rng()&2097151u;Value v{uint32_t(rng()),uint32_t(rng())};reference[k]=v;trial[k]=v;}
 for(unsigned k=0;k<2097152;++k){auto p=reference.find(k);auto q=trial.find(k);const auto& v=trial.get_or_zero(k);assert(bool(p)==bool(q));assert(v.actual==(p?p->second.actual:0));assert(v.expected==(p?p->second.expected:0));}
 auto copy=trial;for(unsigned k=0;k<2097152;k+=17){auto a=trial.get_or_zero(k),b=copy.get_or_zero(k);assert(a.actual==b.actual&&a.expected==b.expected);}
 std::cout<<"2097152 key reads, presence and zero semantics; deep-copy sample PASS\n";
}
"""
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-O2','-I'+str(r/'src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 print(subprocess.run([str(p/'test')],check=True,capture_output=True,text=True).stdout,end='')
