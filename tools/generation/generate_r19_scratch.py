#!/usr/bin/env python3
"""Remove only audited redundant raw scratch zeroing; preserve all model state."""
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'src/entropy_r19_inplace.cpp').read_text()
for name in ['V6Weights mid_residual_cdf(','V6Weights r7_mix(','void mid_step_into(']:
 start=s.index(name);brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 function=s[start:i];needle='std::array<uint64_t,256> raw{};';assert function.count(needle)==1,name
 function=function.replace(needle,'std::array<uint64_t,256> raw; // All entries [0,n) are assigned before quantization.')
 s=s[:start]+function+s[i:]
(r/'src/entropy_r19_scratch.cpp').write_text(s)
for old,new,incold,incnew in [('r17_inplace.cpp','r17_scratch.cpp','entropy_r19_inplace.cpp','entropy_r19_scratch.cpp'),('r18_inplace.cpp','r18_scratch.cpp','r17_inplace.cpp','r17_scratch.cpp'),('r19_inplace_model.hpp','r19_scratch_model.hpp','r18_inplace.cpp','r18_scratch.cpp'),('r19_inplace.cpp','r19_scratch.cpp','r19_inplace_model.hpp','r19_scratch_model.hpp')]:
 s=(r/'tools/native'/old).read_text().replace(incold,incnew)
 if new=='r19_scratch_model.hpp':
  needle='std::array<uint64_t,256>raw{};';assert s.count(needle)==1;s=s.replace(needle,'std::array<uint64_t,256>raw;')
 (r/'tools/native'/new).write_text(s)
print('Generated four audited raw scratch initialization changes')
