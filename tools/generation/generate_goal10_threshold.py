#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'src/entropy_g10count.cpp').read_text()
a=s.index('std::array<uint16_t,256> r4_rank_features_count(');b=s.index('\nstd::array<uint16_t,256> r4_rank_features(',a)
new='''std::array<uint16_t,256> r4_rank_features_count(const Position& p,const V8Step& base,
                                           const std::array<uint16_t,256>& tactics) {
    std::array<uint16_t,256> flags{};
    std::array<int8_t,256> score;
    std::array<uint8_t,256> cls;
    std::array<uint16_t,42> global{};
    std::array<std::array<uint16_t,42>,12> histogram{};
    std::array<unsigned,12> totals{};
    std::array<int,12> maximum,cutoff;
    maximum.fill(-16);cutoff.fill(-16);
    int highest=-16;const unsigned n=base.moves.size;
    for(unsigned i=0;i<n;++i){
        Move m=base.moves.a[i].m;
        bool capture=bool(base.flags[i]&(1u<<4));
        int kind=abs_piece(p.board[m.from]);
        int from_c=std::abs(2*int(m.from&7)-7)+std::abs(2*int(m.from>>3)-7);
        int to_c=std::abs(2*int(m.to&7)-7)+std::abs(2*int(m.to>>3)-7);
        int v=(from_c-to_c)/2;
        if(kind==6)v=0;
        if(kind==1)v/=2;
        if(capture)v+=3;
        if(tactics[i]&(1u<<0))v+=6;
        if(tactics[i]&(1u<<1))v-=6;
        if(tactics[i]&(1u<<8))v+=3;
        if(tactics[i]&(1u<<9))v-=3;
        if(tactics[i]&(1u<<2))v+=4;
        if(tactics[i]&(1u<<3))v+=2;
        if(tactics[i]&(1u<<6))v+=2;
        if(v<-15||v>26)fail("relative rank score outside proven range");
        const unsigned c=unsigned(2*(kind-1)+capture);
        score[i]=int8_t(v);cls[i]=uint8_t(c);
        ++global[v+15];++histogram[c][v+15];++totals[c];
        maximum[c]=std::max(maximum[c],v);highest=std::max(highest,v);
    }
    // Only rank predicates are observed, so no ordered candidate list is needed.
    int global_cutoff=highest;
    for(int k=highest,seen=0;k>=-15;--k){
        seen+=global[k+15];
        if(unsigned(seen)>n/4){global_cutoff=k;break;}
    }
    for(unsigned c=0;c<12;++c)if(totals[c]>=3){
        for(int k=maximum[c],seen=0;k>=-15;--k){
            seen+=histogram[c][k+15];
            if(unsigned(seen)>totals[c]/3){cutoff[c]=k;break;}
        }
    }
    for(unsigned i=0;i<n;++i){
        const unsigned c=cls[i];const int v=score[i];
        const bool capture=bool(base.flags[i]&(1u<<4));
        uint16_t bits=0;
        if(v==highest)bits|=1u<<0;
        if(v>=global_cutoff)bits|=1u<<1;
        if(v==maximum[c] && totals[c]>=2)bits|=1u<<2;
        if(totals[c]>=3 && v>=cutoff[c])bits|=1u<<3;
        if(!capture && v>=2)bits|=1u<<4;
        if(!capture && v<=-2)bits|=1u<<5;
        if(capture && v>=6)bits|=1u<<6;
        if(capture && v<=-4)bits|=1u<<7;
        flags[i]=bits;
    }
    return flags;
}
'''
s=s[:a]+new+s[b:];(r/'src/entropy_g10threshold.cpp').write_text(s)
for name in ['r17_g10count.cpp','r18_g10count.cpp','r19_g10count_model.hpp','r19_g10count.cpp','test_goal10_count.cpp']:
 (r/'tools/native'/name.replace('g10count','g10threshold').replace('goal10_count','goal10_threshold')).write_text((r/'tools/native'/name).read_text().replace('g10count','g10threshold'))
