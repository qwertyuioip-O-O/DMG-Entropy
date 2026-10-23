#!/usr/bin/env python3
"""Exact bounded opportunity division seeded by portable floating division."""
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'src/entropy_g10index.cpp').read_text()
marker='V6Weights v09_cached_opportunity('
helper='''// BEGIN GOAL10_EXACT_OP_RATIO
static uint64_t goal10_exact_op_ratio(uint32_t actual,uint32_t expected){
    const uint64_t numerator=(uint64_t(actual)+16384u)*1024u;
    const uint64_t denominator=uint64_t(expected)+16384u;
    // Entire uint32 counter domain: n<2^43, d<2^33 and quotient<2^29.
    // IEEE binary precision >=53 represents both operands exactly; quotient
    // rounding changes the truncated integer by at most one. Product fits u64.
    if constexpr(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits>=53){
        uint64_t q=uint64_t(double(numerator)/double(denominator));
        const uint64_t product=q*denominator;
        if(product>numerator)return q-1;
        return q+uint64_t(numerator-product>=denominator);
    }else return numerator/denominator;
}
// END GOAL10_EXACT_OP_RATIO
'''
s=s.replace(marker,helper+marker,1)
old='std::clamp((hits+prior)*1024u/(exp+prior),uint64_t(341),uint64_t(3072))'
a=s.index(marker);b=s.index('void v09_cached_observe(',a)
chunk=s[a:b];assert chunk.count(old)==1
chunk=chunk.replace(old,'std::clamp(goal10_exact_op_ratio(uint32_t(hits),uint32_t(exp)),uint64_t(341),uint64_t(3072))')
s=s[:a]+chunk+s[b:]
(r/'src/entropy_g10ratio.cpp').write_text(s)
for name in ['r17_g10index.cpp','r18_g10index.cpp','r19_g10index_model.hpp','r19_g10index.cpp']:
 (r/'tools/native'/name.replace('g10index','g10ratio')).write_text((r/'tools/native'/name).read_text().replace('g10index','g10ratio'))
