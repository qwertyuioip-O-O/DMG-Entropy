#include "rmc.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string read_text(const std::string& path) {
    std::ifstream f(path,std::ios::binary);
    if(!f)throw std::runtime_error("cannot read: "+path);
    return std::string(std::istreambuf_iterator<char>(f),{});
}
std::vector<uint8_t> read_bytes(const std::string& path) {
    std::ifstream f(path,std::ios::binary);
    if(!f)throw std::runtime_error("cannot read: "+path);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{});
}
void write_bytes(const std::string& path,const std::vector<uint8_t>& data) {
    std::ofstream f(path,std::ios::binary);
    if(!f)throw std::runtime_error("cannot write: "+path);
    f.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));
    if(!f)throw std::runtime_error("write failed: "+path);
}
void write_text(const std::string& path,const std::string& data) {
    std::ofstream f(path,std::ios::binary);
    if(!f)throw std::runtime_error("cannot write: "+path);
    f << data;
}
rmc::Mode parse_mode(std::string value) {
    if(value=="rmc")return rmc::Mode::Hybrid;
    if(value=="smc")return rmc::Mode::Structural;
    if(value=="raw")return rmc::Mode::Coordinates;
    if(value=="conditional")return rmc::Mode::Conditional;
    if(value=="auto")return rmc::Mode::Auto;
    if(value=="dmg")return rmc::Mode::DMGTruncated;
    if(value=="uniform")return rmc::Mode::EntropyUniform;
    if(value=="static")return rmc::Mode::EntropyStatic;
    if(value=="adaptive")return rmc::Mode::EntropyAdaptive;
    if(value=="positional")return rmc::Mode::EntropyPositional;
    if(value=="tactical")return rmc::Mode::EntropyTactical;
    if(value=="rich")return rmc::Mode::EntropyRich;
    if(value=="checkfilter")return rmc::Mode::EntropyCheckFiltered;
    if(value=="tactical-check")return rmc::Mode::EntropyTacticalCheck;
    if(value=="rich-check")return rmc::Mode::EntropyRichCheck;
    if(value=="phase")return rmc::Mode::EntropyPhase;
    if(value=="phase-tactical")return rmc::Mode::EntropyPhaseTactical;
    if(value=="phase-check")return rmc::Mode::EntropyPhaseCheck;
    if(value=="phase-forcing")return rmc::Mode::EntropyPhaseForcing;
    if(value=="phase-opening")return rmc::Mode::EntropyPhaseOpening;
    if(value=="phase-middle")return rmc::Mode::EntropyPhaseMiddle;
    if(value=="phase-endgame")return rmc::Mode::EntropyPhaseEndgame;
    if(value=="check-signal")return rmc::Mode::EntropyCheckSignal;
    if(value=="check-signal-light")return rmc::Mode::EntropyCheckSignalLight;
    if(value=="best")return rmc::Mode::Best;
    throw std::runtime_error("mode must be rmc, conditional, smc, raw, auto, dmg, uniform, static, adaptive, positional, tactical, rich, checkfilter, tactical-check, rich-check, phase, phase-tactical, phase-check, phase-forcing, best");
}
std::string get_fen(int argc,char** argv,int start) {
    for(int i=start;i<argc;++i)if(std::string(argv[i])=="--fen") {
        if(i+1>=argc)throw std::runtime_error("missing --fen argument");
        return argv[i+1];
    }
    return {};
}
void print_stat(const rmc::Encoded& enc,rmc::Mode mode) {
    const auto& s=enc.stats;
    std::cout << std::left << std::setw(24)<<rmc::mode_name(mode)
              << " bits/ply " << std::setw(8)<< std::fixed<<std::setprecision(3)
              << (s.total?double(s.payload_bits)/s.total:0.0)
              << " payload " << std::setw(8) << (s.payload_bits+7)/8
              << " file " << std::setw(8)<<enc.bytes.size()
              << " full " << s.full << " partial " << s.partial
              << " general " << s.general << '\n';
}
void usage() {
    std::cerr<<"Usage:\n"
             <<"  rmc encode moves.uci moves.rmc [rmc|conditional|smc|raw|auto|dmg|uniform|static|adaptive|best] [--fen \"FEN\"]\n"
             <<"  rmc decode moves.rmc moves.uci\n"
             <<"  rmc compare moves.uci [--fen \"FEN\"]\n"
             <<"  rmc bench [games] [max_plies] [seed]    (synthetic legal games)\n";
}
uint64_t xorshift(uint64_t& state) {
    state ^= state>>12;state ^= state<<25;state ^= state>>27;
    return state*UINT64_C(2685821657736338717);
}
void bench(int games,int max_plies,uint64_t seed) {
    if(games<1||games>10000||max_plies<1||max_plies>1000)throw std::runtime_error("invalid benchmark dimensions");
    std::vector<std::vector<rmc::Move>> corpus;
    corpus.reserve(games);
    size_t total=0;
    for(int i=0;i<games;++i) {
        auto p=rmc::Position::standard();
        std::vector<rmc::Move> moves;
        for(int j=0;j<max_plies;++j) {
            auto candidates=p.legal_moves();
            if(candidates.empty())break;
            auto m=candidates[xorshift(seed)%candidates.size()];
            if(!p.apply_checked(m))throw std::runtime_error("benchmark generator error");
            moves.push_back(m);
        }
        total+=moves.size();corpus.push_back(std::move(moves));
    }
    std::cout<<"Synthetic uniformly-random LEGAL move games; NOT representative of human chess.\n";
    std::cout<<"games="<<games<<" plies="<<total<<"\n";
    for(auto mode:{rmc::Mode::Hybrid,rmc::Mode::Conditional,rmc::Mode::Structural,rmc::Mode::Coordinates,rmc::Mode::DMGTruncated,rmc::Mode::EntropyUniform,rmc::Mode::EntropyStatic,rmc::Mode::EntropyAdaptive,rmc::Mode::EntropyPositional,rmc::Mode::EntropyTactical,rmc::Mode::EntropyRich,rmc::Mode::EntropyCheckFiltered,rmc::Mode::EntropyTacticalCheck,rmc::Mode::EntropyRichCheck,rmc::Mode::EntropyPhase,rmc::Mode::EntropyPhaseTactical,rmc::Mode::EntropyPhaseCheck,rmc::Mode::EntropyPhaseForcing,rmc::Mode::EntropyPhaseOpening,rmc::Mode::EntropyPhaseMiddle,rmc::Mode::EntropyPhaseEndgame,rmc::Mode::EntropyCheckSignal,rmc::Mode::EntropyCheckSignalLight,rmc::Mode::Best}) {
        auto start=std::chrono::steady_clock::now();
        std::vector<rmc::Encoded> encoded;encoded.reserve(corpus.size());
        uint64_t bits=0,bytes=0,full=0,part=0;
        for(const auto& moves:corpus) {
            encoded.push_back(rmc::encode(moves,mode));
            const auto& e=encoded.back();bits+=e.stats.payload_bits;bytes+=e.bytes.size();
            full+=e.stats.full;part+=e.stats.partial;
        }
        auto mid=std::chrono::steady_clock::now();
        for(size_t i=0;i<corpus.size();++i)
            if(rmc::decode(encoded[i].bytes)!=corpus[i])throw std::runtime_error("roundtrip mismatch");
        auto end=std::chrono::steady_clock::now();
        double enc_s=std::chrono::duration<double>(mid-start).count();
        double dec_s=std::chrono::duration<double>(end-mid).count();
        std::cout<<std::left<<std::setw(24)<<rmc::mode_name(mode)<<std::fixed<<std::setprecision(3)
                 <<" bits/ply="<< (total?double(bits)/total:0)
                 <<" filebytes="<<bytes
                 <<" enc Mply/s="<< (enc_s?total/enc_s/1e6:0)
                 <<" dec Mply/s="<< (dec_s?total/dec_s/1e6:0)
                 <<" full="<<full<<" partial="<<part<<'\n';
    }
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc<2){usage();return 2;}
        const std::string command=argv[1];
        if(command=="encode" && argc>=4) {
            auto moves=rmc::parse_moves(read_text(argv[2]));
            auto mode=(argc>=5&&std::string(argv[4])!="--fen")?parse_mode(argv[4]):rmc::Mode::Hybrid;
            auto output=rmc::encode(moves,mode,get_fen(argc,argv,4));
            write_bytes(argv[3],output.bytes);
            print_stat(output,mode);
        } else if(command=="decode" && argc==4) {
            auto moves=rmc::decode(read_bytes(argv[2]));
            write_text(argv[3],rmc::format_moves(moves));
            std::cout<<"decoded "<<moves.size()<<" plies\n";
        } else if(command=="compare"&&argc>=3) {
            auto moves=rmc::parse_moves(read_text(argv[2]));
            auto fen=get_fen(argc,argv,3);
            for(auto mode:{rmc::Mode::Hybrid,rmc::Mode::Conditional,rmc::Mode::Structural,rmc::Mode::Coordinates,rmc::Mode::DMGTruncated,rmc::Mode::EntropyUniform,rmc::Mode::EntropyStatic,rmc::Mode::EntropyAdaptive,rmc::Mode::EntropyPositional,rmc::Mode::EntropyTactical,rmc::Mode::EntropyRich,rmc::Mode::EntropyCheckFiltered,rmc::Mode::EntropyTacticalCheck,rmc::Mode::EntropyRichCheck,rmc::Mode::EntropyPhase,rmc::Mode::EntropyPhaseTactical,rmc::Mode::EntropyPhaseCheck,rmc::Mode::EntropyPhaseForcing,rmc::Mode::EntropyPhaseOpening,rmc::Mode::EntropyPhaseMiddle,rmc::Mode::EntropyPhaseEndgame,rmc::Mode::EntropyCheckSignal,rmc::Mode::EntropyCheckSignalLight,rmc::Mode::Best}) {
                auto compressed=rmc::encode(moves,mode,fen);
                if(rmc::decode(compressed.bytes)!=moves)throw std::runtime_error("roundtrip mismatch");
                print_stat(compressed,mode);
            }
        } else if(command=="bench"&&argc<=5) {
            bench(argc>=3?std::stoi(argv[2]):100,argc>=4?std::stoi(argv[3]):80,
                  argc>=5?std::stoull(argv[4]):0x12345678ULL);
        } else {usage();return 2;}
    } catch(const std::exception& e) {
        std::cerr << "error: " << e.what()<<'\n';return 1;
    }
}
