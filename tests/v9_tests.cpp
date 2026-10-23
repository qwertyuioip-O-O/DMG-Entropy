#include "dmg_v9.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using Games=std::vector<std::vector<rmc::Move>>;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static Games load_heldout(){
    Games games;
    namespace fs=std::filesystem;
    auto folder=fs::path(RMC_SOURCE_DIR)/"examples"/"heldout";
    std::vector<fs::path> paths;
    for(const auto& entry:fs::directory_iterator(folder))if(entry.path().extension()==".uci")paths.push_back(entry.path());
    std::sort(paths.begin(),paths.end());
    for(const auto& path:paths){
        std::ifstream f(path);
        std::string s{std::istreambuf_iterator<char>(f),{}};
        games.push_back(rmc::parse_moves(s));
    }
    return games;
}
static void roundtrip(const Games& games){
    for(auto m:{rmc::V9Model::DeltaOpportunity,rmc::V9Model::DeltaNoPattern,rmc::V9Model::Auto}){
        auto a=rmc::v9_encode(games,m);
        check(rmc::v9_decode(a.bytes)==games,"DMG9 roundtrip mismatch");
        auto b=rmc::v9_encode(games,m);
        check(a.bytes==b.bytes,"DMG9 not deterministic");
        auto corrupted=a.bytes;
        if(!corrupted.empty()){
            corrupted.back()^=16;bool rejected=false;
            try{(void)rmc::v9_decode(corrupted);}catch(const std::exception&){rejected=true;}
            check(rejected,"DMG9 checksum did not reject corruption");
        }
    }
}
int main(){
    try{
        roundtrip({});roundtrip({{}});
        Games games=load_heldout();roundtrip(games);
        std::mt19937_64 rng(20261020);
        Games random;
        for(int k=0;k<12;++k){
            rmc::Position p=rmc::Position::standard();std::vector<rmc::Move> g;
            for(int ply=0;ply<60;++ply){
                auto legal=p.legal_moves();if(legal.empty())break;
                auto m=legal[rng()%legal.size()];check(p.apply_checked(m),"random legal move failed");g.push_back(m);
            }
            random.push_back(std::move(g));
        }
        roundtrip(random);
        auto one=rmc::v9_encode(games,rmc::V9Model::DeltaOpportunity);
        auto gold_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-delta-opportunity.dmg9";
        std::ifstream gold(gold_path,std::ios::binary);
        if(gold){std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(gold),{}};
                 check(bytes==one.bytes,"DMG9 golden byte stream mismatch");}
        else throw std::runtime_error("missing DMG9 golden sample");
        auto bad=one.bytes;bad[5]=252;
        bool rejected=false;try{(void)rmc::v9_decode(bad);}catch(const std::exception&){rejected=true;}
        check(rejected,"DMG9 unknown mode accepted");
        auto old=rmc::v8_encode(games,rmc::V8Model::RelationalJoint);
        check(rmc::v9_decode(old.bytes)==games,"DMG8 legacy dispatch failed");
        auto v7=rmc::v7_encode(games,rmc::V7Model::OpportunitySquare);
        check(rmc::v9_decode(v7.bytes)==games,"DMG7 legacy dispatch failed");
        std::cout<<"DMG9 tests PASS: "<<games.size()<<" human games, "<<random.size()<<" random games, legacy and CRC\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
