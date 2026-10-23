#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1];s=(r/'src/entropy_g10pages.cpp').read_text();start=s.index('struct OwnAttackReuse {');brace=s.index('{',start);i=brace+1;depth=1
while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
assert s[i]==';';end=i+1;old=s[start:end]
a=old.index('    static uint64_t piece_attack(');b=old.index('{',a);j=b+1;depth=1
while depth:depth+=(old[j]=='{')-(old[j]=='}');j+=1
helper=old[a:j]
new='''struct OwnAttackReuse {
    std::array<uint16_t,64> source_id{},ray_sources{};
    std::array<uint8_t,16> source_square{};
    std::array<std::array<uint64_t,16>,4> unions{};
    uint16_t all_ids=0;unsigned groups=0;bool fallback=false;
'''+helper+'''
    explicit OwnAttackReuse(const Position&p){
        const unsigned count=std::popcount(p.occupied[p.side]);
        if(count>16){fallback=true;return;}
        all_ids=count==16?uint16_t(65535):uint16_t((1u<<count)-1);groups=(count+3)/4;
        std::array<uint64_t,16> attacks{};unsigned id=0;
        for(uint64_t pieces=p.occupied[p.side];pieces;pieces&=pieces-1,++id){
            const int sq=std::countr_zero(pieces);const uint16_t flag=uint16_t(1u<<id);
            source_square[id]=uint8_t(sq);source_id[sq]=flag;attacks[id]=piece_attack(p,sq);
            const int kind=abs_piece(p.board[sq]);if(kind<3||kind>5)continue;
            uint64_t rays=0;for(int d=kind==3?4:0;d<(kind==4?4:8);++d)rays|=fast_attack_tables.ray[sq][d];
            while(rays){int target=std::countr_zero(rays);rays&=rays-1;ray_sources[target]|=flag;}
        }
        for(unsigned group=0;group<groups;++group)for(unsigned mask=1;mask<16;++mask){
            unsigned low=std::countr_zero(mask);unions[group][mask]=unions[group][mask&(mask-1)]|attacks[group*4+low];
        }
    }
    uint64_t after(const Position&before,const Position&next)const{
        const unsigned side=before.side;uint64_t out=0;
        if(fallback){for(uint64_t pieces=next.occupied[side];pieces;pieces&=pieces-1)out|=piece_attack(next,std::countr_zero(pieces));}
        else{
            uint16_t affected=0;
            uint64_t changed=(before.occupied[0]|before.occupied[1])^(next.occupied[0]|next.occupied[1]);
            while(changed){int sq=std::countr_zero(changed);changed&=changed-1;affected|=ray_sources[sq];}
            uint64_t removed=before.occupied[side]&~next.occupied[side];
            while(removed){int sq=std::countr_zero(removed);removed&=removed-1;affected|=source_id[sq];}
            const unsigned unchanged=all_ids&~affected;
            for(unsigned group=0;group<groups;++group)out|=unions[group][(unchanged>>(group*4))&15];
            unsigned pending=affected;
            while(pending){unsigned id=std::countr_zero(pending);pending&=pending-1;int sq=source_square[id];if(next.occupied[side]&bit(sq))out|=piece_attack(next,sq);}
            uint64_t added=next.occupied[side]&~before.occupied[side];
            while(added){int sq=std::countr_zero(added);added&=added-1;out|=piece_attack(next,sq);}
        }
#ifdef DMT_VERIFY_REUSE
        AttackCache oracle(next);if(out!=oracle.attacked_mask)fail("grouped attack reuse mismatch");
#endif
        return out;
    }
};'''
s=s[:start]+new+s[end:];(r/'src/entropy_g10group.cpp').write_text(s)
for old,new,incold,incnew in [('r17_g10pages.cpp','r17_g10group.cpp','entropy_g10pages.cpp','entropy_g10group.cpp'),('r18_g10pages.cpp','r18_g10group.cpp','r17_g10pages.cpp','r17_g10group.cpp'),('r19_g10pages_model.hpp','r19_g10group_model.hpp','r18_g10pages.cpp','r18_g10group.cpp'),('r19_g10pages.cpp','r19_g10group.cpp','r19_g10pages_model.hpp','r19_g10group_model.hpp')]:
 (r/'tools'/new).write_text((r/'tools'/old).read_text().replace(incold,incnew))
s=(r/'tools/test_r19_attacks.cpp').read_text().replace('entropy_r19_attacks.cpp','entropy_g10group.cpp').replace('reuse.after(p,next,m)','reuse.after(p,next)')
s=s.replace('"4k3/8/8/8/8/8/4R3/4K3 b - - 0 1"','"4k3/8/8/8/8/8/4R3/4K3 b - - 0 1","4k3/8/8/8/8/PPPPPPPP/PPPPPPPP/4K3 w - - 0 1"')
(r/'tools/test_goal10_group.cpp').write_text(s)
print('Generated grouped own-attack unions with exact affected-source recomputation')
