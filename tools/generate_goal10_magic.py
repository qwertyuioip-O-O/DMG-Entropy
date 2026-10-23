#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1];s=(r/'src/rmc.cpp').read_text()
a=s.index('uint64_t sliding(');b=s.index('unsigned width(',a)
new=r"""uint64_t sliding(const Position& p,int sq,bool orthogonal,bool diagonal){
    const uint64_t occupied=p.occupied[0]|p.occupied[1];
    uint64_t moves=0;
    if(orthogonal){const auto& d=goal10_magic[0][sq];moves=goal10_magic_attacks[d.offset+(((occupied&d.mask)*d.multiplier)>>d.shift)];}
    if(diagonal){const auto& d=goal10_magic[1][sq];moves|=goal10_magic_attacks[d.offset+(((occupied&d.mask)*d.multiplier)>>d.shift)];}
    return moves&~p.occupied[color(p.board[sq])];
}
"""
s=s[:a]+new+s[b:];s='#include "goal10_magic_data.hpp"\n'+s
(r/'src/rmc_g10magic.cpp').write_text(s)
