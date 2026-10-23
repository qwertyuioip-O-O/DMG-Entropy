#include "dmg_v6.hpp"
#include <iostream>
#include <stdexcept>
#include <random>
#include <vector>
#include <string>
using Games=std::vector<std::vector<rmc::Move>>;
static void require(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
static void test(const Games& gs) {
    for(int k=0;k<5;++k) {
        auto m=rmc::V6Model(k);
        auto e=rmc::v6_encode(gs,m);
        require(rmc::v6_decode(e.bytes)==gs,"V6 decoded mismatched games");
        require(rmc::v6_encode(gs,m).bytes==e.bytes,"V6 non-deterministic output");
        for(size_t i=0;i<e.bytes.size();++i){
            auto corrupt=e.bytes;corrupt[i]^=1;
            bool rejected=false;
            try{(void)rmc::v6_decode(corrupt);}catch(const std::exception&){rejected=true;}
            require(rejected,"V6 corrupted byte not detected");
        }
    }
}
int main(){
    try {
        test({});test({{}, {}, {}});
        test({rmc::parse_moves("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1"),{}});
        std::mt19937 rng(2026);
        Games all;
        for(int g=0;g<12;++g) {
            rmc::Position p=rmc::Position::standard();std::vector<rmc::Move> game;
            for(int ply=0;ply<45;++ply) {
                auto valid=p.legal_moves();if(valid.empty())break;
                auto m=valid[rng()%valid.size()];require(p.apply_checked(m),"oracle bad");game.push_back(m);
            }
            all.push_back(game);
        }
        test(all);
        std::cout<<"v6 tests PASS\n";
    } catch(const std::exception& e) {std::cerr<<"V6 TEST FAIL: "<<e.what()<<'\n';return 1;}
}
