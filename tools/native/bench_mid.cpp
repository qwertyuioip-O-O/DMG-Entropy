#include "dmg_mid.hpp"
#include "dmg_v8.hpp"
#include "rmc.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using Games=std::vector<std::vector<rmc::Move>>;
static Games readgames(int argc,char**argv,int start){
    Games g;
    for(int k=start;k<argc;++k){std::ifstream f(argv[k]);if(!f)throw std::runtime_error("missing game");
        std::string text{std::istreambuf_iterator<char>(f),{}};g.push_back(rmc::parse_moves(text));}
    return g;
}
static Games random_games(){
    std::mt19937_64 rng(20261020);Games g;
    for(unsigned i=0;i<16;++i){rmc::Position p=rmc::Position::standard();std::vector<rmc::Move> seq;
        for(unsigned j=0;j<64;++j){auto a=p.legal_moves();if(a.empty())break;
            auto m=a[rng()%a.size()];if(!p.apply_checked(m))throw std::runtime_error("random bug");seq.push_back(m);}
        g.push_back(std::move(seq));}
    return g;
}
int main(int argc,char**argv){
    try{
        if(argc<2)throw std::runtime_error("usage bench_mid *.uci | --random | --repeat K *.uci");
        std::string dataset="games";Games games;
        if(argc==3&&std::string(argv[1])=="--bundle"){
            dataset="bundle";std::ifstream input(argv[2]);
            if(!input)throw std::runtime_error("missing bundle");
            std::string line;
            while(std::getline(input,line)){
                if(line.empty())continue;
                games.push_back(rmc::parse_moves(line));
            }
        }
        else if(argc==2&&std::string(argv[1])=="--random"){dataset="random16";games=random_games();}
        else if(argc>3&&std::string(argv[1])=="--repeat"){
            auto factor=std::stoi(argv[2]);if(factor<1||factor>10)throw std::runtime_error("bad repeat");
            auto base=readgames(argc,argv,3);for(int i=0;i<factor;++i)games.insert(games.end(),base.begin(),base.end());dataset="repeat"+std::to_string(factor);
        }else games=readgames(argc,argv,1);
        uint64_t n=0;for(auto&g:games)n+=g.size();
        if(!n)throw std::runtime_error("empty dataset");
        std::cout<<std::fixed<<std::setprecision(7);
        std::cout<<"dataset,profile,games,plies,payload_bits,payload_bpp,total_bytes,total_bpp,encode_s,decode_s,stage,stage_plies,stage_logloss,stage_bpp,tail64,tailbits,roundtrip,features_selected,features_candidate\n";
        for(unsigned profile=0;profile<rmc::MID_PROFILE_COUNT;++profile){
            auto p=rmc::MidProfile(profile);rmc::MidDiagnostics d;
            const auto start=std::chrono::steady_clock::now();
            auto archive=rmc::mid_encode(games,p,&d);
            const auto encoded=std::chrono::steady_clock::now();
            auto decoded=rmc::mid_decode(archive.bytes);
            const auto finished=std::chrono::steady_clock::now();
            if(decoded!=games)throw std::runtime_error("roundtrip failed: "+std::string(rmc::mid_name(p)));
            double te=std::chrono::duration<double>(encoded-start).count();
            double td=std::chrono::duration<double>(finished-encoded).count();
            constexpr const char* stages[]={"opening","middlegame","endgame"};
            for(unsigned s=0;s<3;++s){auto&r=d.phase[s];
                std::cout<<dataset<<","<<rmc::mid_name(p)<<","<<games.size()<<","<<n<<","<<archive.payload_bits<<","<<double(archive.payload_bits)/n<<","<<archive.bytes.size()<<","<<8.0*archive.bytes.size()/n<<","<<te<<","<<td<<","<<stages[s]<<","<<r.moves<<","<<r.logloss<<","<<(r.moves?r.logloss/r.moves:0)<<","<<r.tails<<","<<r.tailbits<<",PASS,\"";
                for(unsigned k=0;k<10;++k){if(k)std::cout<<"|";std::cout<<r.feature_selected[k];}
                std::cout<<"\",\"";
                for(unsigned k=0;k<10;++k){if(k)std::cout<<"|";std::cout<<r.feature_candidates[k];}
                std::cout<<"\"\n";
            }
            std::cout<<std::flush;
        }
    }catch(const std::exception&e){std::cerr<<"FAILED: "<<e.what()<<"\n";return 1;}
}
