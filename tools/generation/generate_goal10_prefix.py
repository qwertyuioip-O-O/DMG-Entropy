#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2]
s=(r/'src/entropy_g10reuse.cpp').read_text()
needle='struct V8Step {'
wrapper='// Only initialized active pointers are copied; inactive slots are never read.\nstruct Goal10EntryPrefix {\n    std::array<V7Patterns::Opportunity*,256> slots;\n    unsigned size=0;\n    Goal10EntryPrefix() noexcept {}\n    Goal10EntryPrefix(const Goal10EntryPrefix& other) noexcept : size(other.size) {\n        std::copy_n(other.slots.begin(),size,slots.begin());\n    }\n    Goal10EntryPrefix& operator=(const Goal10EntryPrefix& other) noexcept {\n        if(this!=&other){size=other.size;std::copy_n(other.slots.begin(),size,slots.begin());}\n        return *this;\n    }\n    auto& operator[](unsigned i){return slots[i];}\n    auto const& operator[](unsigned i) const{return slots[i];}\n};\n'
s=s.replace(needle,wrapper+needle,1)
s=s.replace('std::array<V7Patterns::Opportunity*,256> square_entries{};','Goal10EntryPrefix square_entries;')
s=s.replace('std::array<V7Patterns::Opportunity*,256>*','Goal10EntryPrefix*')
a=s.index('V6Weights v09_cached_opportunity(');b=s.index('void v09_cached_observe(',a)
chunk=s[a:b];chunk=chunk.replace('    return v6_quantize(raw,c.size);','    if(entries)entries->size=c.size;\n    return v6_quantize(raw,c.size);');s=s[:a]+chunk+s[b:]
s=s.replace('    if(entries)for(unsigned i=0;i<c.size;++i)','    if(entries && entries->size!=c.size)fail("entry prefix size mismatch");\n    if(entries)for(unsigned i=0;i<c.size;++i)')
(r/'src/entropy_g10prefix.cpp').write_text(s)
for name in ['r17_g10reuse.cpp','r18_g10reuse.cpp','r19_g10reuse_model.hpp','r19_g10reuse.cpp']:
 (r/'tools/native'/name.replace('g10reuse','g10prefix')).write_text((r/'tools/native'/name).read_text().replace('g10reuse','g10prefix'))
