#!/usr/bin/env python3
"""Exact equal-counter opportunity shortcut."""
from pathlib import Path
r=Path(__file__).resolve().parents[1];s=(r/'src/entropy_g10index.cpp').read_text()
marker='V6Weights v09_cached_opportunity('
helper='// Exact equal-counter shortcut; no changed rounding or model states.\nstatic uint64_t goal10_equal_op_ratio(uint32_t actual,uint32_t expected){\n    if(actual==expected)return 1024u;\n    return (uint64_t(actual)+16384u)*1024u/(uint64_t(expected)+16384u);\n}\n'
s=s.replace(marker,helper+marker,1)
old='std::clamp((hits+prior)*1024u/(exp+prior),uint64_t(341),uint64_t(3072))'
a=s.index(marker);b=s.index('void v09_cached_observe(',a)
chunk=s[a:b];assert chunk.count(old)==1
chunk=chunk.replace(old,'std::clamp(goal10_equal_op_ratio(uint32_t(hits),uint32_t(exp)),uint64_t(341),uint64_t(3072))')
s=s[:a]+chunk+s[b:]
(r/'src/entropy_g10equal.cpp').write_text(s)
for name in ['r17_g10index.cpp','r18_g10index.cpp','r19_g10index_model.hpp','r19_g10index.cpp']:
 (r/'tools'/name.replace('g10index','g10equal')).write_text((r/'tools'/name).read_text().replace('g10index','g10equal'))
