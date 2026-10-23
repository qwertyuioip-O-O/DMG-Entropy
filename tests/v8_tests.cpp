#include "dmg_v8.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
using Games=std::vector<std::vector<rmc::Move>>;
static void check(bool p,const char* msg){if(!p)throw std::runtime_error(msg);}
static void run(const Games& games) {
    for(unsigned i=0;i<=unsigned(rmc::V8Model::Auto);++i) {
        auto mode=rmc::V8Model(i);
        auto a=rmc::v8_encode(games,mode);
        auto b=rmc::v8_decode(a.bytes);
        check(b==games,"DMG8 round trip mismatch");
        check(rmc::v8_encode(games,mode).bytes==a.bytes,"DMG8 nondeterminism");
        if(a.bytes.size()>5){
            auto corrupt=a.bytes;
            corrupt[corrupt.size()/2]^=1u;
            bool rejected=false;
            try{(void)rmc::v8_decode(corrupt);}catch(const std::exception&){rejected=true;}
            check(rejected,"DMG8 CRC not validated");
        }
    }
}
int main(){
    try{
        run({});run({{},{}});
        auto open=rmc::parse_moves("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1");
        run({open,open,{},open});
        auto ep=rmc::parse_moves("e2e4 a7a6 e4e5 d7d5 e5d6");
        auto prom=rmc::parse_moves("a2a4 h7h5 a4a5 h5h4 a5a6 h4h3 a6b7 h3g2 b7a8q");
        for(const auto& series:{ep,prom}){
            auto p=rmc::Position::standard();
            for(const auto& m:series)check(p.apply_checked(m),"invalid edge-case fixture");
        }
        run({ep,prom,ep,prom});
        // Frozen binary golden stream for future Rust/ARM cross-codec compatibility.
        std::ifstream golden(std::string(RMC_SOURCE_DIR)+"/examples/golden/italian-relational.dmg8",std::ios::binary);
        check(bool(golden),"missing golden stream");
        std::vector<uint8_t> expected{std::istreambuf_iterator<char>(golden),{}};
        std::ifstream fixture(std::string(RMC_SOURCE_DIR)+"/examples/italian-opening.uci");
        check(bool(fixture),"missing golden source");
        std::string source{std::istreambuf_iterator<char>(fixture),{}};
        auto italian=rmc::parse_moves(source);
        check(rmc::v8_encode({italian},rmc::V8Model::RelationalJoint).bytes==expected,
              "DMG8 golden byte mismatch");
        check(rmc::v8_decode(expected)==Games({italian}),"DMG8 golden decode mismatch");
        // Random legal games include varied pins, checks and tactical positions.
        std::mt19937_64 rng(152367u);
        Games all;
        for(int g=0;g<8;++g){
            rmc::Position p=rmc::Position::standard();
            std::vector<rmc::Move> moves;
            for(int ply=0;ply<45;++ply){
                auto legal=p.legal_moves();if(legal.empty())break;
                auto m=legal[rng()%legal.size()];
                check(p.apply_checked(m),"bad legal oracle");
                moves.push_back(m);
            }
            all.push_back(std::move(moves));
        }
        run(all);
        auto old7=rmc::v7_encode({open,open},rmc::V7Model::OpportunitySquare);
        check(rmc::v8_decode(old7.bytes)==Games({open,open}),"legacy DMG7 decode failed");
        auto old6=rmc::v6_encode({open,open},rmc::V6Model::All);
        check(rmc::v8_decode(old6.bytes)==Games({open,open}),"legacy DMG6 decode failed");
        std::cout<<"DMG8 tests PASS\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
