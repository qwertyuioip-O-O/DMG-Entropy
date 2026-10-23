#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/entropy_g10group.cpp').read_text()
a=s.index('unsigned material_phase(const Position& p) {');b=s.index('\n}\n',a)+2
s=s[:a]+'''unsigned material_phase(const Position& p) {
    // Disjoint piece masks allow merging equal phase weights before counting.
    const auto& a=p.piece_bits[0];const auto& b=p.piece_bits[1];
    unsigned phase=std::popcount(a[1]|b[1]|a[2]|b[2])
        +2u*std::popcount(a[3]|b[3])+4u*std::popcount(a[4]|b[4]);
#ifdef DMT_VERIFY_SHARED
    unsigned reference=0;
    for(unsigned c=0;c<2;++c){reference+=std::popcount(p.piece_bits[c][1])+std::popcount(p.piece_bits[c][2])+2u*std::popcount(p.piece_bits[c][3])+4u*std::popcount(p.piece_bits[c][4]);}
    if(phase!=reference)throw std::runtime_error("phase union mismatch");
#endif
    return std::min(24u,phase);
}'''+s[b:]
s=s.replace('if(ply<24 && material_phase(p)>=18)return 0;\n        if(material_phase(p)<=8)return 2;','const unsigned value=material_phase(p);\n        if(ply<24 && value>=18)return 0;\n        if(value<=8)return 2;')
(r/'src/entropy_g10phase.cpp').write_text(s)
for name in ['r17_g10group.cpp','r18_g10group.cpp','r19_g10group_model.hpp','r19_g10group.cpp']:
 (r/'tools'/name.replace('g10group','g10phase')).write_text((r/'tools'/name).read_text().replace('g10group','g10phase'))
