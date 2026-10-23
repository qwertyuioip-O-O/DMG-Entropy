#!/usr/bin/env python3
"""Build isolated leaper-union experiment on the frozen direct-output candidate."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
s=(r/'src/entropy_r19_inplace.cpp').read_text()
s=s.replace('std::array<uint64_t,64> attacks{},rays{};','std::array<uint64_t,64> attacks{},rays{},without_leaper{};\n    uint64_t leapers=0,leaper_union=0;')
needle='    uint64_t after(const Position& before,const Position& next)const{'
start=s.index(needle)
# Add prefix/suffix union construction to OwnAttackReuse constructor.
end_constructor=s.rfind('    }',0,start)
s=s[:end_constructor]+'''        leapers=p.piece_bits[p.side][0]|p.piece_bits[p.side][1]|p.piece_bits[p.side][5];
        uint64_t pending=leapers,prefix=0;
        while(pending){int sq=std::countr_zero(pending);pending&=pending-1;
            without_leaper[sq]=prefix;prefix|=attacks[sq];}
        leaper_union=prefix;pending=leapers;uint64_t suffix=0;
        while(pending){int sq=63-std::countl_zero(pending);pending&=~bit(sq);
            without_leaper[sq]|=suffix;suffix|=attacks[sq];}
'''+s[end_constructor:]
s=s.replace(needle,'    uint64_t after(const Position& before,const Position& next,Move move)const{')
old='''        uint64_t pieces=next.occupied[before.side],out=0;
        while(pieces){int sq=std::countr_zero(pieces);pieces&=pieces-1;
            out|=(before.board[sq]!=next.board[sq]||(rays[sq]&changed))?piece_attack(next,sq):attacks[sq];
        }'''
new='''        uint64_t out=(leapers&bit(move.from))?without_leaper[move.from]:leaper_union;
        const int moved_kind=abs_piece(next.board[move.to]);
        if(moved_kind==1||moved_kind==2||moved_kind==6)out|=piece_attack(next,move.to);
        const int side=before.side;
        uint64_t pieces=next.piece_bits[side][2]|next.piece_bits[side][3]|next.piece_bits[side][4];
        while(pieces){int sq=std::countr_zero(pieces);pieces&=pieces-1;
            out|=(before.board[sq]!=next.board[sq]||(rays[sq]&changed))?piece_attack(next,sq):attacks[sq];
        }'''
assert old in s;s=s.replace(old,new).replace('reuse->after(p,after)','reuse->after(p,after,m)')
(r/'src/entropy_r19_attacks.cpp').write_text(s)
for old,new,incold,incnew in [('r17_inplace.cpp','r17_attacks.cpp','entropy_r19_inplace.cpp','entropy_r19_attacks.cpp'),('r18_inplace.cpp','r18_attacks.cpp','r17_inplace.cpp','r17_attacks.cpp'),('r19_inplace_model.hpp','r19_attacks_model.hpp','r18_inplace.cpp','r18_attacks.cpp'),('r19_inplace.cpp','r19_attacks.cpp','r19_inplace_model.hpp','r19_attacks_model.hpp')]:
 (r/'tools/native'/new).write_text((r/'tools/native'/old).read_text().replace(incold,incnew))
print('Generated isolated cached-leaper-union candidate')
