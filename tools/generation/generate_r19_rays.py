#!/usr/bin/env python3
"""Isolate incremental slider-ray updates on the attack-union candidate."""
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'src/entropy_r19_attacks.cpp').read_text()
needle='    explicit OwnAttackReuse(const Position& p){'
helper='''    uint64_t updated_slider(const Position& before,const Position& next,int sq,uint64_t changed)const{
        if(before.board[sq]!=next.board[sq])return piece_attack(next,sq);
        uint64_t out=attacks[sq];const int kind=abs_piece(next.board[sq]);
        const uint64_t occ=next.occupied[0]|next.occupied[1];
        const auto& t=fast_attack_tables;
        for(int d=kind==3?4:0;d<(kind==4?4:8);++d){
            const uint64_t full=t.ray[sq][d];if(!(full&changed))continue;
            auto ray=full;const auto blockers=ray&occ;
            if(blockers){int near=(d==0||d==2||d==4||d==6)?std::countr_zero(blockers):63-std::countl_zero(blockers);
                ray&=~t.ray[near][d];}
            out=(out&~full)|ray;
        }
        return out;
    }
'''
assert s.count(needle)==1;s=s.replace(needle,helper+needle)
old='out|=(before.board[sq]!=next.board[sq]||(rays[sq]&changed))?piece_attack(next,sq):attacks[sq];'
new='out|=(before.board[sq]!=next.board[sq]||(rays[sq]&changed))?updated_slider(before,next,sq,changed):attacks[sq];'
assert s.count(old)==1;s=s.replace(old,new);(r/'src/entropy_r19_rays.cpp').write_text(s)
for old,new,incold,incnew in [('r17_attacks.cpp','r17_rays.cpp','entropy_r19_attacks.cpp','entropy_r19_rays.cpp'),('r18_attacks.cpp','r18_rays.cpp','r17_attacks.cpp','r17_rays.cpp'),('r19_attacks_model.hpp','r19_rays_model.hpp','r18_attacks.cpp','r18_rays.cpp'),('r19_attacks.cpp','r19_rays.cpp','r19_attacks_model.hpp','r19_rays_model.hpp'),('test_r19_attacks.cpp','test_r19_rays.cpp','entropy_r19_attacks.cpp','entropy_r19_rays.cpp')]:
 (r/'tools/native'/new).write_text((r/'tools/native'/old).read_text().replace(incold,incnew))
print('Generated per-ray update candidate')
