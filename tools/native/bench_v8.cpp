#include "dmg_v8.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <vector>
using Games=std::vector<std::vector<rmc::Move>>;
static volatile unsigned sink=0;
static Games random_games(){
    std::mt19937_64 rng(20261020);Games result;
    for(int g=0;g<16;++g){
        rmc::Position p=rmc::Position::standard();std::vector<rmc::Move> v;
        for(int t=0;t<64;++t){
            auto a=p.legal_moves();if(a.empty())break;
            auto m=a[rng()%a.size()];p.apply_checked(m);v.push_back(m);
        }
        result.push_back(std::move(v));
    }
    return result;
}
static Games read(int argc,char** argv,int from){
    Games games;
    for(int i=from;i<argc;++i){std::ifstream f(argv[i]);if(!f)throw std::runtime_error("unreadable input");
        std::string s{std::istreambuf_iterator<char>(f),{}};games.push_back(rmc::parse_moves(s));}
    return games;
}
template<class Fn> double rate(Fn f,uint64_t n){
    std::vector<double> times;
    for(int j=0;j<2;++j){auto a=std::chrono::steady_clock::now();f();auto b=std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double>(b-a).count());}
    return double(n)/(*std::min_element(times.begin(),times.end()))/1e6;
}
static void bench(const Games& games,const std::string& label){
    uint64_t n=0;for(auto& g:games)n+=g.size();
    std::cout<<"dataset,mode,games,plies,payload_bits,payload_bpp,total_bytes,total_bpp,encode_Mply_s,decode_Mply_s,verified\n";
    auto row=[&](const char* name,const rmc::V6Archive& a,auto encoder,auto decoder){
        auto b=decoder(a.bytes);if(b!=games)throw std::runtime_error("roundtrip mismatch "+std::string(name));
        double enc=rate([&]{sink=encoder().bytes.size();},n);
        double dec=rate([&]{sink=decoder(a.bytes).size();},n);
        std::cout<<label<<","<<name<<","<<games.size()<<","<<n<<","<<a.payload_bits<<","<<std::fixed<<std::setprecision(5)
            <<(n?double(a.payload_bits)/n:0)<<","<<a.bytes.size()<<","<<(n?8.0*a.bytes.size()/n:0)
            <<","<<enc<<","<<dec<<",PASS\n"<<std::flush;
    };
    for(auto m: {rmc::V7Model::OpportunitySquare,rmc::V7Model::Auto}) {
        auto a=rmc::v7_encode(games,m);
        row(m==rmc::V7Model::Auto?"v7-auto":"v7-opportunity-square",a,
            [&]{return rmc::v7_encode(games,m);},[&](const auto& x){return rmc::v7_decode(x);});
    }
    for(unsigned i=0;i<=unsigned(rmc::V8Model::Auto);++i){
        auto m=rmc::V8Model(i);auto a=rmc::v8_encode(games,m);
        row(rmc::v8_name(m),a,[&]{return rmc::v8_encode(games,m);},
            [&](const auto& x){return rmc::v8_decode(x);});
    }
}
int main(int argc,char** argv){
  try {
    if(argc==2&&std::string(argv[1])=="--random")bench(random_games(),"random16");
    else if(argc>3 && std::string(argv[1])=="--repeat"){
        int count=std::stoi(argv[2]);if(count<1||count>10)throw std::runtime_error("repeat must be 1..10");
        auto originals=read(argc,argv,3);Games repeated;
        for(int i=0;i<count;++i)repeated.insert(repeated.end(),originals.begin(),originals.end());
        bench(repeated,"repeated"+std::to_string(count));
    }
    else if(argc>1)bench(read(argc,argv,1),"human-development");
    else throw std::runtime_error("usage: dmg_v8_bench --random | game1.uci ...");
  }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
