#include "rmc.hpp"
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
int main(int argc,char**argv){
 try{
 if(argc<2)throw std::runtime_error("usage: alphabet_report game.uci ...");
 double pseudo_bits=0,legal_bits=0;unsigned long long plies=0;
 for(int k=1;k<argc;++k){
    std::ifstream f(argv[k]);if(!f)throw std::runtime_error("cannot open game");
    std::string content(std::istreambuf_iterator<char>{f},{});
    auto moves=rmc::parse_moves(content);auto p=rmc::Position::standard();
    for(auto m:moves){
       uint64_t ours=p.occupied[p.side];unsigned pseudo=0;
       while(ours){int from=std::countr_zero(ours);ours&=ours-1;uint64_t ds=p.pseudo_destinations(from);
         while(ds){int to=std::countr_zero(ds);ds&=ds-1;
           int piece=p.board[from];if(piece<0)piece=-piece;
           pseudo+=(piece==1&&(to>>3==0||to>>3==7))?4:1;
         }
       }
       auto legal=p.legal_moves();
       if(legal.empty()||pseudo==0)throw std::runtime_error("empty move alphabet");
       pseudo_bits+=std::log2(double(pseudo));legal_bits+=std::log2(double(legal.size()));
       if(!p.apply_checked(m))throw std::runtime_error("illegal corpus move");
       ++plies;
    }
 }
 std::cout<<"plies="<<plies<<" mean log2 pseudo="<<pseudo_bits/plies<<" mean log2 legal="<<legal_bits/plies<<" gap="<<(pseudo_bits-legal_bits)/plies<<" bits/ply\n";
 } catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
