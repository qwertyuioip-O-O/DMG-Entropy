#!/usr/bin/env python3
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--base',choices=['index'],default='index');a=p.parse_args()
r=Path(__file__).resolve().parents[1];base='g10'+a.base;s=(r/f'src/entropy_{base}.cpp').read_text()
start=s.index('struct V8Learn {');end=s.index('struct V8Context',start);part=s[start:end]
needle='        for(unsigned f=0;f<V8_F;++f){';assert part.count(needle)==1
part=part.replace(needle,'        const ExactDivisor divide_joint(joint.total),divide_base(base.total);\n'+needle)
old='int32_t((uint64_t(pj[f])*4096u+joint.total/2)/joint.total)';assert part.count(old)==1;part=part.replace(old,'int32_t(divide_joint(uint64_t(pj[f])*4096u+joint.total/2))')
old='(uint64_t(pb[f])*4096u+base.total/2)/base.total';assert part.count(old)==1;part=part.replace(old,'divide_base(uint64_t(pb[f])*4096u+base.total/2)');s=s[:start]+part+s[end:]
start=s.index('void v09_cached_observe(');end=s.index('// Compute check delivery once',start);part=s[start:end]
needle='* entries=nullptr){';assert part.count(needle)==1;part=part.replace(needle,needle+'\n    const ExactDivisor divide(c.total);')
old='unsigned((uint64_t(c.a[i].w)*4096u+c.total/2)/c.total)';assert part.count(old)==1;part=part.replace(old,'unsigned(divide(uint64_t(c.a[i].w)*4096u+c.total/2))');s=s[:start]+part+s[end:]
(r/'src/entropy_g10hoist.cpp').write_text(s)
for old,new,incold,incnew in [(f'r17_{base}.cpp','r17_g10hoist.cpp',f'entropy_{base}.cpp','entropy_g10hoist.cpp'),(f'r18_{base}.cpp','r18_g10hoist.cpp',f'r17_{base}.cpp','r17_g10hoist.cpp'),(f'r19_{base}_model.hpp','r19_g10hoist_model.hpp',f'r18_{base}.cpp','r18_g10hoist.cpp'),(f'r19_{base}.cpp','r19_g10hoist.cpp',f'r19_{base}_model.hpp','r19_g10hoist_model.hpp')]:
 (r/'tools'/new).write_text((r/'tools'/old).read_text().replace(incold,incnew))
(r/'tools/goal10_hoist_base.json').write_text(json.dumps({'base':a.base,'change':'hoist exact invariant divisors in V8Learn.observe and v09_cached_observe'},indent=2))
print('Generated exact invariant-division candidate on',a.base)
