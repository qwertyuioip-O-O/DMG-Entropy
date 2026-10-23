#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2]
h=(r/'src/experiments/goal10/goal10_op_table.hpp').read_text().replace('Goal10OpTable','Goal10ReadTable')
h=h.replace('    const Cell* end()const{return nullptr;}',r"""    // An allocated unused cell is already value-initialized to all-zero counters.
    // Read-only prediction does not need the insertion-presence bitmap.
    const Value& get_or_zero(uint32_t key) const {
        check(key);static const Value zero{};
        if(pages.empty())return zero;
        const auto& p=pages[key>>shift];
        return p?p->cells[key&(page_size-1)].second:zero;
    }
    const Cell* end()const{return nullptr;}""")
(r/'src/experiments/goal10/goal10_read_table.hpp').write_text(h)
s=(r/'src/experiments/goal10/entropy_g10index.cpp').read_text().replace('experiments/goal10/goal10_op_table.hpp','experiments/goal10/goal10_read_table.hpp').replace('Goal10OpTable<Opportunity>','Goal10ReadTable<Opportunity>')
a=s.index('V6Weights v09_cached_opportunity(');b=s.index('void v09_cached_observe(',a);chunk=s[a:b]
a1=chunk.index('        const V7Patterns::Opportunity* record=nullptr;');b1=chunk.index('        uint64_t ratio=',a1)
chunk=chunk[:a1]+r"""#ifdef DMT_CACHE_OPPORTUNITY
        const V7Patterns::Opportunity* record=nullptr;
        if(entries){auto* entry=&db.square_op[key];(*entries)[i]=entry;record=entry;}
        else record=&db.square_op.get_or_zero(key);
        uint64_t hits=record->actual,exp=record->expected;
#else
        (void)entries;
        const auto& record=db.square_op.get_or_zero(key);
        uint64_t hits=record.actual,exp=record.expected;
#endif
"""+chunk[b1:];s=s[:a]+chunk+s[b:]
(r/'src/experiments/goal10/entropy_g10read.cpp').write_text(s)
for name in ['r17_g10index.cpp','r18_g10index.cpp','r19_g10index_model.hpp','r19_g10index.cpp']:
 (r/'tools/native'/name.replace('g10index','g10read')).write_text((r/'tools/native'/name).read_text().replace('g10index','g10read'))
