#include "dmg_phase.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
using Games=std::vector<std::vector<rmc::Move>>;
static volatile uint64_t sink=0;
static Games load(int argc,char**argv,int start){
    Games g;
    for(int i=start;i<argc;++i){std::ifstream f(argv[i]);if(!f)throw std::runtime_error("unreadable input "+std::string(argv[i]));
      std::string txt{std::istreambuf_iterator<char>(f),{}};g.push_back(rmc::parse_moves(txt));}
    return g;
}
static Games random_set(){
    std::mt19937_64 rng(20261020);Games g;
    for(int k=0;k<16;++k){rmc::Position p=rmc::Position::standard();std::vector<rmc::Move> moves;
        for(int j=0;j<64;++j){auto legal=p.legal_moves();if(legal.empty())break;
            auto m=legal[rng()%legal.size()];p.apply_checked(m);moves.push_back(m);}
        g.push_back(std::move(moves));}return g;
}
template<class Fn> double speed(Fn f,uint64_t n){
    std::vector<double> times;
    for(int trial=0;trial<3;++trial){auto start=std::chrono::steady_clock::now();
        for(int k=0;k<3;++k)sink=sink+f();
        auto end=std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double>(end-start).count());}
    std::sort(times.begin(),times.end());return n*3/times[1];
}
int main(int argc,char**argv){
 try{
    std::string dataset="development";Games games;
    if(argc==2&&std::string(argv[1])=="--random"){games=random_set();dataset="random16";}
    else if(argc>3&&std::string(argv[1])=="--repeat"){
        int n=std::stoi(argv[2]);if(n<1||n>10)throw std::runtime_error("invalid repeat");
        auto base=load(argc,argv,3);for(int i=0;i<n;++i)games.insert(games.end(),base.begin(),base.end());dataset="repeat"+std::to_string(n);
    }else if(argc>1)games=load(argc,argv,1);
    else throw std::runtime_error("usage: dmg_phase_bench *.uci | --random | --repeat 3 *.uci");
    uint64_t plies=0;for(const auto&g:games)plies+=g.size();
    std::cout<<std::fixed<<std::setprecision(7);
    std::cout<<"dataset,mode,games,plies,payload_bits,payload_bpp,total_bytes,total_bpp,encode_plies_s,decode_plies_s,stage,stage_plies,stage_ideal_bits,stage_ideal_bpp,top1_percent,tail64_percent,tail64_bits,verified\n";
    auto show=[&](const std::string&name,const rmc::V6Archive&archive,const rmc::PhaseDiagnostics&d,auto enc,auto dec){
        if(dec(archive.bytes)!=games)throw std::runtime_error("roundtrip failed "+name);
        double encspeed=speed([&]{return enc().bytes.size();},plies);
        double decspeed=speed([&]{return dec(archive.bytes).size();},plies);
        constexpr const char* stages[]={"opening","middlegame","endgame"};
        for(unsigned k=0;k<3;++k){auto &s=d.stage[k];
            std::cout<<dataset<<","<<name<<","<<games.size()<<","<<plies<<","<<archive.payload_bits<<","<<double(archive.payload_bits)/plies<<","<<archive.bytes.size()<<","<<8.0*archive.bytes.size()/plies<<","<<encspeed<<","<<decspeed<<","<<stages[k]<<","<<s.plies<<","<<s.ideal_bits<<","<<(s.plies?s.ideal_bits/s.plies:0)<<","<<(s.plies?100.0*s.top1/s.plies:0)<<","<<(s.plies?100.0*s.long_tail_64/s.plies:0)<<","<<s.long_tail_bits<<",PASS\n";
        }std::cout<<std::flush;
    };
    auto reference=rmc::v8_encode(games,rmc::V8Model::RelationalJoint);
    auto base=rmc::phase_baseline_diagnostics(games);
    show("DMG8-Fast",reference,base,
         [&]{return rmc::v8_encode(games,rmc::V8Model::RelationalJoint);},
         [&](const auto&b){return rmc::v8_decode(b);});
    for(auto m:{rmc::V8Model::LegalOpportunity,rmc::V8Model::RelationalAE,rmc::V8Model::ExpertMixture}){
        auto archive=rmc::v8_encode(games,m);
        auto diag=rmc::phase_v8_diagnostics(games,m);
        show(rmc::v8_name(m),archive,diag,[&]{return rmc::v8_encode(games,m);},
            [&](const auto&b){return rmc::v8_decode(b);});
    }
    for(auto p:{rmc::PhaseProfile::Shared,rmc::PhaseProfile::Residual1,rmc::PhaseProfile::Residual2,rmc::PhaseProfile::Residual4,rmc::PhaseProfile::Residual6,rmc::PhaseProfile::EndgameOnly,rmc::PhaseProfile::OpeningMixtureEndgame}){
        rmc::PhaseDiagnostics d{};
        auto a=rmc::phase_encode(games,p,&d);
        show(rmc::phase_name(p),a,d,[&]{return rmc::phase_encode(games,p);},
            [&](const auto&b){return rmc::phase_decode(b);});
    }
    std::cerr<<"sink="<<sink<<"\n";
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
