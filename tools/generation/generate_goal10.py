#!/usr/bin/env python3
"""Exact rank buckets, then exact bitboard bishop mobility; frozen goal anchor."""
from pathlib import Path
r=Path(__file__).resolve().parents[2];original=(r/'src/entropy_r19_scratch.cpp').read_text()
def function(s,name):
 start=s.index(name);brace=s.index('{',start);i=brace+1;depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return start,i,s[start:i]
s=original;a,b,old=function(s,'std::array<uint16_t,256> r4_rank_features(')
start=old.index('    struct RankItem');end=old.index('    return flags;',start)
new='''    std::array<int,129> head;head.fill(-1);
    std::array<int,256> next;
    std::array<uint8_t,256> classes;
    std::array<unsigned,12> class_totals{},class_seen{};
    int minimum=64,maximum=-64;
    for(unsigned i=0;i<n;++i){
        const auto m=base.moves.a[i].m;
        const unsigned cls=2*(abs_piece(p.board[m.from])-1)+bool(base.flags[i]&(1u<<4));
        if(score[i]<-64||score[i]>64)fail("relative rank score outside proven range");
        classes[i]=uint8_t(cls);++class_totals[cls];
        next[i]=head[score[i]+64];head[score[i]+64]=int(i);
        minimum=std::min(minimum,score[i]);maximum=std::max(maximum,score[i]);
    }
    unsigned processed=0;
    for(int value=maximum;value>=minimum;--value){
        const int first=head[value+64];if(first<0)continue;
        unsigned count=0;
        for(int i=first;i>=0;i=next[i]){
            const bool capture=bool(base.flags[i]&(1u<<4));
            const unsigned cls=classes[i],same_class=class_totals[cls],better_class=class_seen[cls];
            uint16_t bits=0;
            if(processed==0)bits|=1u<<0;
            if(4*processed<=n)bits|=1u<<1;
            if(better_class==0&&same_class>=2)bits|=1u<<2;
            if(same_class>=3&&3*better_class<=same_class)bits|=1u<<3;
            if(!capture&&value>=2)bits|=1u<<4;
            if(!capture&&value<=-2)bits|=1u<<5;
            if(capture&&value>=6)bits|=1u<<6;
            if(capture&&value<=-4)bits|=1u<<7;
            flags[i]=bits;++count;
        }
        for(int i=first;i>=0;i=next[i])++class_seen[classes[i]];
        processed+=count;
    }
#ifdef DMT_VERIFY_GOAL10
    if(flags!=r4_rank_reference_goal10(p,base,tactics))fail("goal10 rank oracle mismatch");
#endif
'''
modified=old[:start]+new+old[end:]
ref='#ifdef DMT_VERIFY_GOAL10\n'+old.replace('r4_rank_features(','r4_rank_reference_goal10(',1)+'\n#endif\n'
s=s[:a]+ref+modified+s[b:]
stage1=s
# A slider attacks every empty ray square plus the first enemy blocker, excluding own blockers.
a,b,oldmob=function(s,'inline int r5_slider_mobility(')
helper='''inline int goal10_slider_mobility(int sq,int kind,uint64_t occupied,uint64_t own){
    const auto& t=fast_attack_tables;uint64_t available=0;
    for(int d=kind==3?4:0;d<(kind==4?4:8);++d){
        auto ray=t.ray[sq][d];auto blockers=ray&occupied;
        if(blockers){int near=(d==0||d==2||d==4||d==6)?std::countr_zero(blockers):63-std::countl_zero(blockers);ray&=~t.ray[near][d];}
        available|=ray;
    }
    return std::popcount(available&~own);
}
inline int r5_slider_mobility(const Position&p,int sq,int side,int kind,uint64_t occupied){
    return goal10_slider_mobility(sq,kind,occupied,p.occupied[side]);
}'''
refmob='#ifdef DMT_VERIFY_GOAL10\n'+oldmob.replace('r5_slider_mobility(','r5_slider_mobility_reference_goal10(',1)+'\n#endif\n'
s=s[:a]+refmob+helper+s[b:]
a,b,oldpos=function(s,'std::array<uint16_t,256> r5_positional_features(')
start=oldpos.index('                int new_mob=0;');end=oldpos.index('                if(new_mob>=old_mob+2)',start)
newpos=oldpos[:start]+'''                const uint64_t own_after=(p.occupied[us]&~bit(m.from))|bit(m.to);
                int new_mob=goal10_slider_mobility(m.to,3,after,own_after);
'''+oldpos[end:]
newpos=newpos.replace('    std::array<uint16_t,256> features{};','''#ifdef DMT_VERIFY_GOAL10
    auto oracle_quiet=quiet;
#endif
    std::array<uint16_t,256> features{};''',1)
newpos=newpos.replace('    return features;','''#ifdef DMT_VERIFY_GOAL10
    if(features!=r5_positional_reference_goal10(p,base,oracle_quiet,profile)||quiet!=oracle_quiet)fail("goal10 mobility oracle mismatch");
#endif
    return features;''')
refpos='#ifdef DMT_VERIFY_GOAL10\n'+oldpos.replace('r5_positional_features(','r5_positional_reference_goal10(',1).replace('r5_slider_mobility(','r5_slider_mobility_reference_goal10(')+'\n#endif\n'
s=s[:a]+refpos+newpos+s[b:]
for suffix,core in [('g10rank',stage1),('g10mob',s)]:
 (r/f'src/entropy_{suffix}.cpp').write_text(core)
 for old,new,incold,incnew in [('r17_scratch.cpp',f'r17_{suffix}.cpp','entropy_r19_scratch.cpp',f'entropy_{suffix}.cpp'),('r18_scratch.cpp',f'r18_{suffix}.cpp','r17_scratch.cpp',f'r17_{suffix}.cpp'),('r19_scratch_model.hpp',f'r19_{suffix}_model.hpp','r18_scratch.cpp',f'r18_{suffix}.cpp'),('r19_scratch.cpp',f'r19_{suffix}.cpp','r19_scratch_model.hpp',f'r19_{suffix}_model.hpp')]:
  (r/'tools/native'/new).write_text((r/'tools/native'/old).read_text().replace(incold,incnew))
print('Generated goal10 stage1 exact rank buckets and cumulative stage2 bitboard mobility')
