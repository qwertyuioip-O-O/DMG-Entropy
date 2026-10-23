#include "dmg_v9.hpp"
#include <chrono>
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
static uint64_t sink=0;
static Games load(int argc,char**argv,int start){
    Games g;
    for(int i=start;i<argc;++i){
        std::ifstream f(argv[i]);if(!f)throw std::runtime_error("cannot open input");
        std::string s{std::istreambuf_iterator<char>(f),{}};g.push_back(rmc::parse_moves(s));
    }return g;
}
static Games random_set(){
    std::mt19937_64 rng(20261020);Games result;
    for(int k=0;k<16;++k){
        rmc::Position p=rmc::Position::standard();std::vector<rmc::Move> game;
        for(int ply=0;ply<64;++ply){auto legal=p.legal_moves();if(legal.empty())break;
            auto m=legal[rng()%legal.size()];p.apply_checked(m);game.push_back(m);}
        result.push_back(std::move(game));
    }return result;
}
template<class Function>double benchmark(Function f,uint64_t n,int repeat=4){
    std::vector<double> times;
    for(int trial=0;trial<5;++trial){
        auto start=std::chrono::steady_clock::now();
        for(int i=0;i<repeat;++i)sink+=f();
        auto end=std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double>(end-start).count());
    }
    std::sort(times.begin(),times.end());
    return n*repeat/times[times.size()/2]/1e6;
}
int main(int argc,char**argv){
 try{
    Games games;std::string dataset;
    if(argc==2 && std::string(argv[1])=="--random"){games=random_set();dataset="random16";}
    else if(argc>=4 && std::string(argv[1])=="--repeat"){
        int repeat=std::stoi(argv[2]);auto original=load(argc,argv,3);
        if(repeat<1||repeat>16)throw std::runtime_error("repeat count must be 1..16");
        for(int i=0;i<repeat;++i)games.insert(games.end(),original.begin(),original.end());
        dataset="repeat"+std::to_string(repeat);
    }else if(argc>=2){games=load(argc,argv,1);dataset="human-development";}
    else throw std::runtime_error("usage: dmg_v9_bench files.uci | --random | --repeat N files.uci");
    uint64_t n=0;for(const auto&g:games)n+=g.size();
    std::cout<<"dataset,mode,games,plies,payload_bits,payload_bpp,archive_bytes,total_bpp,encode_Mply_s,decode_Mply_s,verified\n";
    auto show=[&](const std::string& mode,auto enc,auto dec){
        auto a=enc();if(dec(a.bytes)!=games)throw std::runtime_error("roundtrip mismatch "+mode);
        auto rate_enc=benchmark([&](){return enc().bytes.size();},n);
        auto rate_dec=benchmark([&](){return dec(a.bytes).size();},n);
        std::cout<<dataset<<","<<mode<<","<<games.size()<<","<<n<<","<<a.payload_bits<<","
                 <<std::fixed<<std::setprecision(6)<<double(a.payload_bits)/n<<","<<a.bytes.size()<<","
                 <<8.0*a.bytes.size()/n<<","<<rate_enc<<","<<rate_dec<<",PASS\n"<<std::flush;
    };
    show("v8-fast-joint",[&]{return rmc::v8_encode(games,rmc::V8Model::RelationalJoint);},
        [&](auto const&bytes){return rmc::v8_decode(bytes);});
    for(auto m:{rmc::V9Model::DeltaOpportunity,rmc::V9Model::DeltaNoPattern,rmc::V9Model::Auto})
        show(rmc::v9_name(m),[&]{return rmc::v9_encode(games,m);},
            [&](auto const&bytes){return rmc::v9_decode(bytes);});
    std::cerr<<"benchmark sink="<<sink<<"\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
