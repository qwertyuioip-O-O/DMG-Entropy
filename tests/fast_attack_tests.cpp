#include "rmc.hpp"
#include <bit>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace rmc;
static bool reference_attacked(const Position& p,int sq,int attacker){
    int f=sq&7,r=sq>>3;
    int pawn_rank=r+(attacker==0?-1:1);
    if(pawn_rank>=0&&pawn_rank<8)for(int df:{-1,1}){
        if(f+df<0||f+df>=8)continue;
        int8_t piece=p.board[pawn_rank*8+f+df];
        if(piece==(attacker==0?1:-1))return true;
    }
    constexpr int knight[8][2]={{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    for(const auto&d:knight){int nf=f+d[0],nr=r+d[1];if(unsigned(nf)<8&&unsigned(nr)<8){int v=p.board[nr*8+nf];if(v==(attacker==0?2:-2))return true;}}
    for(int df=-1;df<=1;++df)for(int dr=-1;dr<=1;++dr){if(!df&&!dr)continue;
        int nf=f+df,nr=r+dr;if(unsigned(nf)<8&&unsigned(nr)<8){int v=p.board[nr*8+nf];if(v==(attacker==0?6:-6))return true;}}
    constexpr int ray[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    for(int d=0;d<8;++d){int nf=f+ray[d][0],nr=r+ray[d][1];
        while(unsigned(nf)<8&&unsigned(nr)<8){
            int v=p.board[nr*8+nf];if(v){
                int kind=v<0?-v:v;
                if((v>0)==(attacker==0)&&(kind==5||kind==(d<4?4:3)))return true;
                break;
            }
            nf+=ray[d][0];nr+=ray[d][1];
        }
    }
    return false;
}
int main(){
    std::mt19937_64 rng(20261020);
    uint64_t positions=0;
    for(int g=0;g<128;++g){
        Position p=Position::standard();
        for(int ply=0;ply<144;++ply){
            for(int side=0;side<2;++side)for(int sq=0;sq<64;++sq)
                if(p.square_attacked(sq,side)!=reference_attacked(p,sq,side))throw std::runtime_error("attack mismatch");
            ++positions;
            auto moves=p.legal_moves();if(moves.empty())break;
            if(!p.apply_checked(moves[rng()%moves.size()]))throw std::runtime_error("invalid legal move");
        }
    }
    std::cout<<"attack oracle check passed for "<<positions<<" positions and "<<positions*128<<" square/side queries\n";
}
