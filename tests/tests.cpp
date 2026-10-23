#include "rmc.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
using namespace rmc;
namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void roundtrip(const std::string& text,const std::string& fen={}) {
    auto moves=parse_moves(text);
    for(auto mode:{Mode::Hybrid,Mode::Conditional,Mode::Structural,Mode::Coordinates,Mode::Auto,Mode::DMGTruncated,Mode::EntropyUniform,Mode::EntropyStatic,Mode::EntropyAdaptive,Mode::EntropyPositional,Mode::EntropyTactical,Mode::EntropyRich,Mode::EntropyCheckFiltered,Mode::EntropyTacticalCheck,Mode::EntropyRichCheck,Mode::EntropyPhase,Mode::EntropyPhaseTactical,Mode::EntropyPhaseCheck,Mode::EntropyPhaseForcing,Mode::EntropyPhaseOpening,Mode::EntropyPhaseMiddle,Mode::EntropyPhaseEndgame,Mode::EntropyCheckSignal,Mode::EntropyCheckSignalLight,Mode::Best}) {
        auto blob=encode(moves,mode,fen);
        require(decode(blob.bytes)==moves,"roundtrip mismatch");
        if(mode==Mode::Auto) {
            auto structural=encode(moves,Mode::Structural,fen);
            require(blob.stats.payload_bits<=structural.stats.payload_bits,"auto worse than baseline");
        }
        if(mode==Mode::Best) {
            for(auto alternative:{Mode::Structural,Mode::Conditional,Mode::DMGTruncated,Mode::EntropyUniform,Mode::EntropyStatic,Mode::EntropyAdaptive,Mode::EntropyPositional,Mode::EntropyTactical,Mode::EntropyRich,Mode::EntropyCheckFiltered,Mode::EntropyTacticalCheck,Mode::EntropyRichCheck,Mode::EntropyPhase,Mode::EntropyPhaseTactical,Mode::EntropyPhaseCheck,Mode::EntropyPhaseForcing,Mode::EntropyPhaseOpening,Mode::EntropyPhaseMiddle,Mode::EntropyPhaseEndgame,Mode::EntropyCheckSignal,Mode::EntropyCheckSignalLight}) {
                require(blob.stats.payload_bits<=encode(moves,alternative,fen).stats.payload_bits,"best mode is not the minimum");
            }
        }
        require(blob.stats.total==moves.size(),"incorrect stats");
        auto copy=encode(moves,mode,fen);
        require(copy.bytes==blob.bytes,"non-deterministic bytes");
    }
}
uint64_t perft(const Position& p,int depth) {
    if(depth==0)return 1;
    uint64_t total=0;
    for(auto m:p.legal_moves()) {
        auto next=p;
        require(next.apply_checked(m),"perft move invalid");
        total+=perft(next,depth-1);
    }
    return total;
}
void illegal(const std::string& moves,const std::string& fen={}) {
    bool rejected=false;
    try {encode(parse_moves(moves),Mode::Hybrid,fen);}catch(const std::exception&) {rejected=true;}
    require(rejected,"invalid input was accepted");
}
uint64_t rng(uint64_t& seed) {
    seed^=seed>>12;seed^=seed<<25;seed^=seed>>27;
    return seed*UINT64_C(2685821657736338717);
}
}
int main() {
    Position initial=Position::standard();
    require(perft(initial,1)==20,"perft depth 1 != 20");
    require(perft(initial,2)==400,"perft depth 2 != 400");
    require(perft(initial,3)==8902,"perft depth 3 != 8902");
    roundtrip("");
    roundtrip("e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 e1g1 f8c5 d2d3 d7d6 c2c3 e8g8");
    roundtrip("e2e4 a7a6 e4e5 d7d5 e5d6"); // en passant
    roundtrip("f2f3 e7e5 g2g4 d8h4"); // Fool's mate
    roundtrip("e2e4 e7e5 d2d4 e5d4 d1d4 b8c6 d4d1");
    const std::string promotion="4k3/P7/8/8/8/8/8/4K3 w - - 0 1";
    for(char x:std::string("qrbn")) roundtrip(std::string("a7a8")+x,promotion);
    const std::string black_promo="4k3/8/8/8/8/8/p7/4K3 b - - 0 1";
    for(char x:std::string("qrbn")) roundtrip(std::string("a2a1")+x,black_promo);
    const std::string castles="r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
    roundtrip("e1g1",castles);
    roundtrip("e1c1",castles);
    roundtrip("e1c1 e8g8",castles);
    illegal("e2e5");
    illegal("e1e2");
    illegal("e2e4 e7e5 e1g1"); // blocked castle
    illegal("a7a8",promotion); // missing promotion
    illegal("a7a8q", "4k3/P7/8/8/8/8/8/4K3 b - - 0 1"); // wrong turn
    uint64_t seed=0x1234ABCD5678ULL;
    uint64_t total=0;
    for(int game=0;game<60;++game) {
        Position p=Position::standard();
        std::vector<Move> moves;
        for(int ply=0;ply<100;++ply) {
            auto options=p.legal_moves();
            if(options.empty())break;
            Move m=options[rng(seed)%options.size()];
            require(p.apply_checked(m),"generated legal move rejected");
            moves.push_back(m);
        }
        for(auto mode:{Mode::Hybrid,Mode::Conditional,Mode::Structural,Mode::Coordinates,Mode::Auto,Mode::DMGTruncated,Mode::EntropyUniform,Mode::EntropyStatic,Mode::EntropyAdaptive,Mode::EntropyPositional,Mode::EntropyTactical,Mode::EntropyRich,Mode::EntropyCheckFiltered,Mode::EntropyTacticalCheck,Mode::EntropyRichCheck,Mode::EntropyPhase,Mode::EntropyPhaseTactical,Mode::EntropyPhaseCheck,Mode::EntropyPhaseForcing,Mode::EntropyPhaseOpening,Mode::EntropyPhaseMiddle,Mode::EntropyPhaseEndgame,Mode::EntropyCheckSignal,Mode::EntropyCheckSignalLight,Mode::Best}) {
            auto compressed=encode(moves,mode);
            require(decode(compressed.bytes)==moves,"fuzz roundtrip mismatch");
            if(compressed.bytes.size()>16) {
                auto truncated=compressed.bytes;truncated.pop_back();
                bool rejected=false;
                try{decode(truncated);}catch(const std::exception&){rejected=true;}
                require(rejected,"invalid input was accepted");
            }
        }
        total+=moves.size();
    }
    // Backward compatibility: v0.2 arithmetic-coded stream decodes unchanged.
    {
        const std::string name=std::string(RMC_SOURCE_DIR)+"/examples/caruana-static.rmc";
        std::ifstream f(name,std::ios::binary);
        if(!f)throw std::runtime_error("legacy fixture unavailable");
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{f},{}};
        auto original=std::string(RMC_SOURCE_DIR)+"/examples/human/caruana-kasparov-2017.uci";
        std::ifstream u(original);
        const std::string text{std::istreambuf_iterator<char>{u},{}};
        require(decode(bytes)==parse_moves(text),"v0.2 fixture compatibility mismatch");
    }
    for(const auto& name:{"caruana-kasparov-2017", "fischer-spassky-1992", "lichess-2013", "long-endgame-2024"}) {
        std::string path=std::string(RMC_SOURCE_DIR)+"/examples/human/"+name+".uci";
        std::ifstream f(path);
        if(!f)throw std::runtime_error("cannot open human corpus fixture: "+path);
        const std::string uci(std::istreambuf_iterator<char>{f},{});
        roundtrip(uci);
    }
    // Pinned piece must not be allowed to expose own king.
    illegal("e2f2","4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1");
    // En passant must not expose e1 king to e8 rook.
    illegal("e5d6","4r1k1/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    // Can't castle across attacked square f1.
    illegal("e1g1","4kr2/8/8/8/8/8/8/4K2R w K - 0 1");
    std::cout<<"PASS: fixed cases, promotions, en-passant, castles, invalid moves, "
             <<total<<" synthetic legal random plies across all encoding choices\n";
}
