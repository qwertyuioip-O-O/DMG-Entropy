#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace rmc;
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::ifstream in(argv[1]); if(!in)return 3;
    std::vector<std::vector<Move>> games;
    std::string line;
    while(std::getline(in,line))if(!line.empty())games.push_back(parse_moves(line));
    for(unsigned id=0;id<MID_PROFILE_COUNT;++id){
        auto coded=mid_encode(games,MidProfile(id));
        if(mid_decode(coded.bytes)!=games)return 4;
        uint64_t hash=14695981039346656037ull;
        for(auto b:coded.bytes){hash^=b;hash*=1099511628211ull;}
        std::cout<<id<<','<<coded.payload_bits<<','<<coded.bytes.size()<<','<<hash<<'\n';
    }
}
