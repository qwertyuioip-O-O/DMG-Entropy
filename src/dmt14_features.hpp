#pragma once
#include "rmc.hpp"
#include <array>
#include <cstdlib>
namespace rmc::detail {
using R14PawnFiles=std::array<std::array<uint8_t,8>,2>;
inline R14PawnFiles r14_pawn_files(const Position& p){
    R14PawnFiles files{};
    for(unsigned sq=0;sq<64;++sq)if(std::abs(int(p.board[sq]))==1)
        files[p.board[sq]<0][sq&7]|=uint8_t(1u<<(sq>>3));
    return files;
}
// Called only for a legal pawn/rook move; describes the resulting pawn structure.
inline std::array<unsigned,2> r14_categories(const Position& p,Move m,const R14PawnFiles& pawn_files){
    const unsigned piece=unsigned(std::abs(int(p.board[m.from]))-1);
            auto own=pawn_files[p.side],enemy=pawn_files[p.side^1];
            const unsigned file=m.to&7,from_file=m.from&7,rr=m.to>>3;
            if(piece==0){
                own[from_file]&=uint8_t(~(1u<<(m.from>>3)));
                // Promotion removes the pawn rather than creating a new pawn.
                if(!m.promotion)own[file]|=uint8_t(1u<<rr);
            }
            // Capture of a pawn, including the en-passant victim behind destination.
            if(std::abs(int(p.board[m.to]))==1)enemy[file]&=uint8_t(~(1u<<rr));
            else if(piece==0&&from_file!=file&&p.board[m.to]==0)
                enemy[file]&=uint8_t(~(1u<<(p.side?rr+1:rr-1)));
            const uint8_t ahead=p.side?uint8_t((1u<<rr)-1):uint8_t(255u&~((1u<<(rr+1))-1));
            unsigned center_key=0,distance_key=0;
            if(piece==3){
                center_key=unsigned(own[file]!=0)+2*unsigned(enemy[file]!=0);
                distance_key=unsigned(bool(own[file]&ahead))+2*unsigned(bool(enemy[file]&ahead))+4*unsigned(from_file!=file);
            }else{
                const uint8_t adjacent=(file?own[file-1]:0)|(file<7?own[file+1]:0);
                const uint8_t enemy_adj=(file?enemy[file-1]:0)|(file<7?enemy[file+1]:0);
                const uint8_t supporters=p.side?uint8_t(1u<<(rr+1)):uint8_t(rr?1u<<(rr-1):0);
                center_key=unsigned(bool(adjacent&supporters))+2*unsigned(bool(enemy[file]&ahead));
                distance_key=unsigned(!adjacent)+2*unsigned(bool(own[file]&uint8_t(~(1u<<rr))))+4*unsigned(!((enemy[file]|enemy_adj)&ahead));
            }
    return {center_key,distance_key};
}
} // namespace rmc::detail
