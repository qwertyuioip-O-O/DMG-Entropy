#include "dmg_v6.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using Game=std::vector<rmc::Move>;
using Games=std::vector<Game>;
static volatile uint64_t observed=0;
static Games read_games(int argc,char** argv,int start) {
    Games games;
    for(int i=start;i<argc;++i) {
        std::ifstream f(argv[i]);
        if(!f)throw std::runtime_error(std::string("cannot read ")+argv[i]);
        std::string s{std::istreambuf_iterator<char>(f),{}};
        games.push_back(rmc::parse_moves(s));
    }
    return games;
}
static Games random_games() {
    std::mt19937_64 rng(20261020);
    Games all;
    for(int g=0;g<48;++g){
        Game game;
        auto p=rmc::Position::standard();
        for(int ply=0;ply<80;++ply) {
            auto legal=p.legal_moves();if(legal.empty())break;
            auto chosen=legal[rng()%legal.size()];
            if(!p.apply_checked(chosen))throw std::runtime_error("random oracle");
            game.push_back(chosen);
        }
        all.push_back(std::move(game));
    }
    return all;
}
static uint64_t count(const Games& games) {
    uint64_t n=0;for(auto& g:games)n+=g.size();return n;
}
template<class F> double timed(F&& fn,uint64_t n) {
    std::vector<double> samples;
    for(int run=0;run<3;++run) {
        auto start=std::chrono::steady_clock::now();
        for(int r=0;r<2;++r)fn();
        auto end=std::chrono::steady_clock::now();
        samples.push_back(std::chrono::duration<double>(end-start).count());
    }
    std::sort(samples.begin(),samples.end());
    return double(n)*2/samples[1]/1e6;
}
static void compare(Games games,const std::string& corpus) {
    uint64_t plies=count(games);
    if(!plies)throw std::runtime_error("empty corpus");
    std::cerr<<"Corpus "<<corpus<<": "<<games.size()<<" games / "<<plies<<" plies\n";
    std::cout<<"corpus,mode,games,plies,payload_bits,payload_bpp,total_bytes,total_bpp,encode_Mply_s,decode_Mply_s,verified\n";
    for(int m=0;m<6;++m) {
        uint64_t payload=0, bytes=0;
        double enc_rate=0,dec_rate=0;
        if(m==5){
            std::vector<rmc::Encoded> encoded;
            for(const auto& g:games) {
                auto e=rmc::encode(g,rmc::Mode::EntropyCheckSignalLight);
                if(rmc::decode(e.bytes)!=g)throw std::runtime_error("legacy mismatch");
                payload+=e.stats.payload_bits;bytes+=e.bytes.size();encoded.push_back(std::move(e));
            }
            enc_rate=timed([&]{uint64_t sink=0;for(const auto& g:games){auto e=rmc::encode(g,rmc::Mode::EntropyCheckSignalLight);sink+=e.bytes.size();}observed=sink;},plies);
            dec_rate=timed([&]{uint64_t sink=0;for(const auto& e:encoded){auto g=rmc::decode(e.bytes);sink+=g.size();}observed=sink;},plies);
        } else {
            auto mode=rmc::V6Model(m);
            auto blob=rmc::v6_encode(games,mode);
            auto result=rmc::v6_decode(blob.bytes);
            if(result!=games)throw std::runtime_error("v6 roundtrip mismatch");
            if(rmc::v6_encode(games,mode).bytes!=blob.bytes)throw std::runtime_error("nondeterministic v6 output");
            if(!blob.bytes.empty()) {
                auto corrupt=blob.bytes;corrupt[corrupt.size()/2]^=0x20;
                bool rejected=false;
                try{(void)rmc::v6_decode(corrupt);}catch(const std::exception&){rejected=true;}
                if(!rejected)throw std::runtime_error("checksum failed to reject flipped bit");
            }
            payload=blob.payload_bits;bytes=blob.bytes.size();
            enc_rate=timed([&]{auto e=rmc::v6_encode(games,mode);observed=e.bytes.size();},plies);
            dec_rate=timed([&]{auto g=rmc::v6_decode(blob.bytes);observed=g.size();},plies);
        }
        std::cout<<corpus<<','<<(m==5?"legacy-v05":rmc::v6_name(rmc::V6Model(m)))<<','<<games.size()<<','<<plies<<','<<payload<<','
                 <<std::fixed<<std::setprecision(5)<<double(payload)/plies<<','<<bytes<<','<<double(bytes)*8/plies<<','
                 <<std::setprecision(3)<<enc_rate<<','<<dec_rate<<",PASS\n"<<std::flush;
    }
}
int main(int argc,char** argv) {
    try {
        if(argc>1 && std::string(argv[1])=="--random")compare(random_games(),"random");
        else if(argc>2 && std::string(argv[1])=="--repeat"){
            int repeats=std::stoi(argv[2]);
            if(repeats<1||repeats>30)throw std::runtime_error("repeats outside 1..30");
            Games base=read_games(argc,argv,3),all;
            for(int i=0;i<repeats;++i)all.insert(all.end(),base.begin(),base.end());
            compare(std::move(all),"repeated"+std::to_string(repeats));
        } else if(argc>1)compare(read_games(argc,argv,1),"human23");
        else throw std::runtime_error("usage: dmg_v6_bench [--random | --repeat N] file.uci ...");
    } catch(const std::exception& e) {std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
