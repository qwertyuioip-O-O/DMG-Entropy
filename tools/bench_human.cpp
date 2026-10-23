#include "rmc.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc,char** argv) {
 try {
    if(argc<2)throw std::runtime_error("usage: rmc_human_bench path/to/game.uci ...");
    std::vector<std::vector<rmc::Move>> games;
    unsigned long long total=0;
    for(int i=1;i<argc;++i) {
        std::ifstream file(argv[i]);if(!file)throw std::runtime_error("cannot open input file");
        std::string text(std::istreambuf_iterator<char>{file},{});
        auto moves=rmc::parse_moves(text);
        total+=moves.size();games.push_back(std::move(moves));
    }
    std::cout<<"Human sourced SAN->UCI games="<<games.size()<<" plies="<<total<<"\n";
    const std::array modes={rmc::Mode::Structural,rmc::Mode::Conditional,rmc::Mode::DMGTruncated,rmc::Mode::EntropyUniform,rmc::Mode::EntropyStatic,rmc::Mode::EntropyAdaptive,rmc::Mode::EntropyPositional,rmc::Mode::EntropyTactical,rmc::Mode::EntropyRich,rmc::Mode::EntropyCheckFiltered,rmc::Mode::EntropyTacticalCheck,rmc::Mode::EntropyRichCheck,rmc::Mode::EntropyPhase,rmc::Mode::EntropyPhaseTactical,rmc::Mode::EntropyPhaseCheck,rmc::Mode::EntropyPhaseForcing,rmc::Mode::EntropyPhaseOpening,rmc::Mode::EntropyPhaseMiddle,rmc::Mode::EntropyPhaseEndgame,rmc::Mode::EntropyCheckSignal,rmc::Mode::EntropyCheckSignalLight,rmc::Mode::Best};
    std::cout<<"method,bits_per_ply,payload_bytes,full_file_bytes,encode_Mply_s,decode_Mply_s\n";
    for(auto mode:modes) {
        std::vector<rmc::Encoded> blobs;
        unsigned long long bits=0,bytes=0,payload_bytes=0;
        for(const auto& g:games) {
            auto enc=rmc::encode(g,mode);
            if(rmc::decode(enc.bytes)!=g)throw std::runtime_error("roundtrip mismatch");
            bits+=enc.stats.payload_bits;bytes+=enc.bytes.size();payload_bytes+=enc.bytes.size()-16;blobs.push_back(std::move(enc));
        }
        auto measure=[&](bool encode) {
            std::vector<double> secs;
            for(int run=0;run<5;++run) {
                const auto begin=std::chrono::steady_clock::now();
                for(int repeat=0;repeat<8;++repeat)for(size_t i=0;i<games.size();++i) {
                    if(encode) {auto enc=rmc::encode(games[i],mode);if(enc.stats.total!=games[i].size())throw std::runtime_error("stats");}
                    else {auto dec=rmc::decode(blobs[i].bytes);if(dec.size()!=games[i].size())throw std::runtime_error("decode");}
                }
                const auto end=std::chrono::steady_clock::now();
                secs.push_back(std::chrono::duration<double>(end-begin).count());
            }
            std::sort(secs.begin(),secs.end());
            return double(total)*8/secs[2]/1e6;
        };
        double enc_m=measure(true),dec_m=measure(false);
        std::cout<<rmc::mode_name(mode)<<','<<std::fixed<<std::setprecision(3)<<double(bits)/total<<','
                 <<payload_bytes<<','<<bytes<<','<<enc_m<<','<<dec_m<<'\n';
    }
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
