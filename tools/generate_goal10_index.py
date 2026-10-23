#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1];s=(r/'src/entropy_g10count.cpp').read_text()
a=s.index('std::array<uint16_t,256> r4_rank_features_count(');b=s.index('\nstd::array<uint16_t,256> r4_rank_features(',a)
new='''// Portable, fixed feature tables: no processor-specific instructions.
static constexpr auto rank_center=[](){
    std::array<int8_t,64> a{};
    for(int i=0;i<64;++i){int x=2*(i&7)-7,y=2*(i>>3)-7;a[i]=int8_t(((x<0?-x:x)+(y<0?-y:y))/2);}
    return a;
}();
static constexpr auto rank_tactical=[](){
    std::array<int8_t,1024> a{};
    for(unsigned f=0;f<a.size();++f)a[f]=int8_t(6*bool(f&1)-6*bool(f&2)+3*bool(f&256)-3*bool(f&512)+4*bool(f&4)+2*bool(f&8)+2*bool(f&64));
    return a;
}();
std::array<uint16_t,256> r4_rank_features_count(const Position& p,const V8Step& base,
                                           const std::array<uint16_t,256>& tactics) {
    std::array<uint16_t,256> flags{};
    std::array<int8_t,256> score;
    std::array<uint8_t,256> cls,order;
    std::array<uint16_t,42> count{},offset{};
    std::array<unsigned,12> class_totals{},class_seen{};
    const unsigned n=base.moves.size;
    for(unsigned i=0;i<n;++i){
        const auto m=base.moves.a[i].m;
        const unsigned kind=unsigned(abs_piece(p.board[m.from]));
        const bool capture=bool(base.flags[i]&(1u<<4));
        int v=int(rank_center[m.from])-rank_center[m.to];
        if(kind==6)v=0;
        if(kind==1)v/=2;
        v+=3*int(capture)+int(rank_tactical[tactics[i]&1023u]);
        if(v<-15||v>26)fail("relative rank score outside proven range");
        score[i]=int8_t(v);cls[i]=uint8_t(2*(kind-1)+capture);
        ++class_totals[cls[i]];++count[unsigned(v+15)];
    }
    unsigned cursor=0;
    for(int k=41;k>=0;--k){offset[k]=uint16_t(cursor);cursor+=count[k];}
    for(unsigned i=0;i<n;++i)order[offset[unsigned(score[i]+15)]++]=uint8_t(i);
    for(unsigned begin=0;begin<n;){
        unsigned end=begin+1;
        while(end<n && score[order[end]]==score[order[begin]])++end;
        for(unsigned j=begin;j<end;++j){
            const unsigned i=order[j],same_class=class_totals[cls[i]],better_class=class_seen[cls[i]];
            const bool capture=bool(base.flags[i]&(1u<<4));
            uint16_t bits=0;
            if(begin==0)bits|=1u<<0;
            if(4*begin<=n)bits|=1u<<1;
            if(better_class==0 && same_class>=2)bits|=1u<<2;
            if(same_class>=3 && 3*better_class<=same_class)bits|=1u<<3;
            if(!capture && score[i]>=2)bits|=1u<<4;
            if(!capture && score[i]<=-2)bits|=1u<<5;
            if(capture && score[i]>=6)bits|=1u<<6;
            if(capture && score[i]<=-4)bits|=1u<<7;
            flags[i]=bits;
        }
        for(unsigned j=begin;j<end;++j)++class_seen[cls[order[j]]];
        begin=end;
    }
    return flags;
}
'''
s=s[:a]+new+s[b:];(r/'src/entropy_g10index.cpp').write_text(s)
for name in ['r17_g10count.cpp','r18_g10count.cpp','r19_g10count_model.hpp','r19_g10count.cpp','test_goal10_count.cpp']:
 (r/'tools'/name.replace('g10count','g10index').replace('goal10_count','goal10_index')).write_text((r/'tools'/name).read_text().replace('g10count','g10index'))
