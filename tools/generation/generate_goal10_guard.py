#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'src/experiments/goal10/entropy_g10index.cpp').read_text()
a=s.index('V6Weights mid_guard(');b=s.index('int mid_piece_value(',a)
old=s[a:b]
new='V6Weights mid_guard(const V6Weights& base,unsigned n,unsigned percent){\n    if(!n || n>=V6_SCALE)fail("bad V6 alphabet");\n    if(!base.total)fail("zero model mass");\n    if(percent>100){\n        std::array<uint64_t,256> raw{};\n        for(unsigned i=0;i<n;++i)raw[i]=uint64_t(base.weights[i])*(100-percent)*n+uint64_t(percent)*base.total;\n        return v6_quantize(raw,n);\n    }\n    // sum(base.weights[0:n]) == base.total is the CDF invariant.\n    // Thus sum(raw) == 100*n*base.total exactly, independent of percent.\n    const uint64_t factor=uint64_t(100-percent)*n,offset=uint64_t(percent)*base.total;\n    const ExactDivisor divide(uint64_t(100)*n*base.total);\n    V6Weights q;\n    for(unsigned i=0;i<n;++i){\n        q.weights[i]=uint16_t(1+divide((uint64_t(base.weights[i])*factor+offset)*(V6_SCALE-n)));\n        q.total+=q.weights[i];\n    }\n    if(q.total>=32768)fail("V6 weights too large");\n#ifdef DMT_VERIFY_GUARD\n    std::array<uint64_t,256> raw{};\n    for(unsigned i=0;i<n;++i)raw[i]=uint64_t(base.weights[i])*(100-percent)*n+uint64_t(percent)*base.total;\n    const auto oracle=v6_quantize(raw,n);\n    if(q.weights!=oracle.weights || q.total!=oracle.total)fail("fused guard mismatch");\n#endif\n    return q;\n}\n'
s=s[:a]+new+s[b:];(r/'src/experiments/goal10/entropy_g10guard.cpp').write_text(s)
for name in ['r17_g10index.cpp','r18_g10index.cpp','r19_g10index_model.hpp','r19_g10index.cpp']:
 (r/'tools/native'/name.replace('g10index','g10guard')).write_text((r/'tools/native'/name).read_text().replace('g10index','g10guard'))
