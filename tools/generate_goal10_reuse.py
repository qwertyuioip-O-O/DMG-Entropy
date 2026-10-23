#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1];s=(r/'src/entropy_g10index.cpp').read_text()
a=s.index('void v09_cached_observe(');b=s.index('// Compute check delivery',a)
chunk=s[a:b];needle='    for(unsigned i=0;i<c.size;++i){'
assert chunk.count(needle)==1
chunk=chunk.replace(needle,'''#ifdef DMT_VERIFY_ENTRY_CACHE
    if(entries)for(unsigned i=0;i<c.size;++i)
        if((*entries)[i]!=&db.square_op[keys[i]])fail("opportunity entry cache context/pointer mismatch");
#endif
'''+needle,1)
s=s[:a]+chunk+s[b:];(r/'src/entropy_g10reuse.cpp').write_text(s)
for name in ['r17_g10index.cpp','r18_g10index.cpp','r19_g10index_model.hpp','r19_g10index.cpp']:
 (r/'tools'/name.replace('g10index','g10reuse')).write_text((r/'tools'/name).read_text().replace('g10index','g10reuse'))
