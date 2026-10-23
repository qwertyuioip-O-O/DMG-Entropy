#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'src/rmc.cpp').read_text()
a=s.index('uint64_t sliding(');b=s.index('unsigned width(',a)
new=r"""struct Goal10SlidingRays {
    uint64_t ray[64][8]{};
    constexpr Goal10SlidingRays(){
        constexpr int d[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
        for(int sq=0;sq<64;++sq)for(int i=0;i<8;++i){
            int f=(sq&7)+d[i][0],rank=(sq>>3)+d[i][1];
            while(unsigned(f)<8 && unsigned(rank)<8){ray[sq][i]|=uint64_t(1)<<(rank*8+f);f+=d[i][0];rank+=d[i][1];}
        }
    }
};
inline constexpr Goal10SlidingRays goal10_sliding_rays{};
uint64_t sliding(const Position& p,int sq,bool orthogonal,bool diagonal){
    const uint64_t occupied=p.occupied[0]|p.occupied[1];
    uint64_t moves=0;
    for(int d=orthogonal?0:4;d<(diagonal?8:4);++d){
        uint64_t ray=goal10_sliding_rays.ray[sq][d];
        const uint64_t blockers=ray&occupied;
        if(blockers){
            const unsigned nearest=(d&1)?63u-std::countl_zero(blockers):std::countr_zero(blockers);
            ray&=~goal10_sliding_rays.ray[nearest][d];
        }
        moves|=ray;
    }
    return moves&~p.occupied[color(p.board[sq])];
}
"""
(r/'src/rmc_g10rays.cpp').write_text(s[:a]+new+s[b:])
