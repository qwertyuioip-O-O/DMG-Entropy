#include "dmg_v7.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
using Games=std::vector<std::vector<rmc::Move>>;
static void must(bool cond,const char* message) {if(!cond)throw std::runtime_error(message);}
static void run(const Games& games) {
    for(int k=0;k<=11;++k) {
        auto mode=rmc::V7Model(k);
        auto blob=rmc::v7_encode(games,mode);
        must(rmc::v7_decode(blob.bytes)==games,"DMG7 roundtrip failed");
        must(rmc::v7_encode(games,mode).bytes==blob.bytes,"DMG7 non-deterministic");
        for(size_t i=0;i<blob.bytes.size();++i){
            auto bad=blob.bytes;bad[i]^=0x01;
            bool rejected=false;
            try{(void)rmc::v7_decode(bad);}catch(...){rejected=true;}
            must(rejected,"DMG7 CRC did not reject corruption");
        }
    }
}
int main(){
 try {
    run({});run({{}, {}, {}});
    auto first=rmc::parse_moves("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1");
    run({first,{},first,first});
    std::mt19937_64 rng(20261020);
    Games random;
    for(int g=0;g<12;++g){
        auto p=rmc::Position::standard();std::vector<rmc::Move> moves;
        for(int i=0;i<35;++i){
            auto lm=p.legal_moves();if(lm.empty())break;
            auto m=lm[rng()%lm.size()];must(p.apply_checked(m),"bad oracle");
            moves.push_back(m);
        }
        random.push_back(moves);
    }
    run(random);
    // Old DMG6 streams remain decodable through DMG7 dispatcher.
    auto old=rmc::v6_encode({first,first},rmc::V6Model::All);
    must(rmc::v7_decode(old.bytes)==Games({first,first}),"DMG6 backwards compatibility failed");
    std::cout<<"DMG7 tests PASS\n";
 }catch(const std::exception& e){std::cerr<<"DMG7 TEST FAILED: "<<e.what()<<"\n";return 1;}
}
