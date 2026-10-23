#include "dmg_v7.hpp"
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
static volatile uint64_t sink=0;
static Games read_games(int argc,char** argv,int from) {
    Games gs;
    for(int i=from;i<argc;++i) {
        std::ifstream f(argv[i]);
        if(!f)throw std::runtime_error(std::string("missing ")+argv[i]);
        std::string text{std::istreambuf_iterator<char>(f),{}};
        gs.push_back(rmc::parse_moves(text));
    }
    return gs;
}
static Games random_games() {
    std::mt19937_64 rng(20261020);
    Games games;
    for(int g=0;g<48;++g) {
        std::vector<rmc::Move> moves;
        auto p=rmc::Position::standard();
        for(int ply=0;ply<80;++ply) {
            auto legal=p.legal_moves();if(legal.empty())break;
            auto move=legal[rng()%legal.size()];
            if(!p.apply_checked(move))throw std::runtime_error("invalid oracle");
            moves.push_back(move);
        }
        games.push_back(std::move(moves));
    }
    return games;
}
static double time_rate(auto&& action,uint64_t n) {
    std::vector<double> timings;
    for(int k=0;k<3;++k) {
        auto t0=std::chrono::steady_clock::now();
        action();
        auto t1=std::chrono::steady_clock::now();
        timings.push_back(std::chrono::duration<double>(t1-t0).count());
    }
    std::sort(timings.begin(),timings.end());
    return n/timings[1]/1e6;
}
static void benchmark(const Games& games,const std::string& corpus) {
    uint64_t n=0;for(auto& game:games)n+=game.size();
    std::cerr<<corpus<<" "<<games.size()<<" games "<<n<<" plies\n";
    std::cout<<"corpus,mode,games,plies,payload_bits,payload_bpp,total_bytes,total_bpp,enc_Mply_s,dec_Mply_s,verified\n";
    for(int i=0;i<15;++i) {
        bool legacy=i<3;
        rmc::V6Archive enc;
        std::vector<std::vector<rmc::Move>> dec;
        const char* name;
        if(legacy) {
            auto m= i==0?rmc::V6Model::Baseline:i==1?rmc::V6Model::Dictionary:rmc::V6Model::All;
            name=i==0?"v6-baseline":i==1?"v6-exact":"v6-all";
            enc=rmc::v6_encode(games,m);
            dec=rmc::v6_decode(enc.bytes);
        } else {
            auto m=rmc::V7Model(i-3);
            name=rmc::v7_name(m);
            enc=rmc::v7_encode(games,m);
            dec=rmc::v7_decode(enc.bytes);
            if(rmc::v7_encode(games,m).bytes!=enc.bytes)throw std::runtime_error("nondeterministic output");
            auto corruption=enc.bytes;corruption[corruption.size()/2]^=8;
            bool rejected=false;
            try{(void)rmc::v7_decode(corruption);}catch(...){rejected=true;}
            if(!rejected)throw std::runtime_error("CRC failed");
        }
        if(dec!=games)throw std::runtime_error(std::string("round-trip mismatch ")+name);
        auto tenc=time_rate([&]{
            if(legacy) {
                auto m=i==0?rmc::V6Model::Baseline:i==1?rmc::V6Model::Dictionary:rmc::V6Model::All;
                sink=rmc::v6_encode(games,m).bytes.size();
            } else sink=rmc::v7_encode(games,rmc::V7Model(i-3)).bytes.size();
        },n);
        auto tdec=time_rate([&]{
            if(legacy)sink=rmc::v6_decode(enc.bytes).size();
            else sink=rmc::v7_decode(enc.bytes).size();
        },n);
        std::cout<<corpus<<","<<name<<","<<games.size()<<","<<n<<","<<enc.payload_bits<<","<<std::fixed<<std::setprecision(4)
                 <<(n?double(enc.payload_bits)/n:0.0)<<","<<enc.bytes.size()<<","<<(n?double(enc.bytes.size())*8/n:0.0)
                 <<","<<tenc<<","<<tdec<<",PASS\n"<<std::flush;
    }
}
int main(int argc,char** argv){
 try{
    if(argc>1&&std::string(argv[1])=="--random")benchmark(random_games(),"random48");
    else if(argc>3&&std::string(argv[1])=="--repeat"){
        int n=std::stoi(argv[2]);if(n<1||n>30)throw std::runtime_error("repeats must be 1..30");
        auto original=read_games(argc,argv,3);Games all;
        for(int i=0;i<n;++i)all.insert(all.end(),original.begin(),original.end());
        benchmark(all,"repeated"+std::to_string(n));
    } else if(argc>1)benchmark(read_games(argc,argv,1),"human");
    else throw std::runtime_error("usage: dmg_v7_bench [--random | --repeat N] game.uci ...");
 }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
