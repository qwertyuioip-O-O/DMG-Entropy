#include "goal10_op_table.hpp"
#include "entropy.hpp"
#include "dmg_v6.hpp"
#include "dmg_v7.hpp"
#include "dmg_v8.hpp"
#include "dmg_v9.hpp"
#include "dmg_phase.hpp"
#include "dmg_mid.hpp"
#include "dmt14_features.hpp"
#include "dmt15_features.hpp"
#include "dmt16_features.hpp"
#include "dmt16_expected.hpp"
#include <unordered_map>
#include <map>
#include <deque>
#include <compare>
#include <string>
#include <cstring>
#include <numeric>
#include <cmath>
#include <array>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <cstdlib>
#include <stdexcept>

namespace rmc {
namespace {
[[noreturn]] void fail(const char* s) { throw std::runtime_error(s); }
inline uint64_t bit(int sq) { return uint64_t(1) << sq; }
inline int abs_piece(int v) { return v<0?-v:v; }
inline int pop(uint64_t x) { return std::popcount(x); }

struct BitSink {
    std::vector<uint8_t> data;
    uint32_t nbits=0;
    void bit(bool x) {
        if(nbits==UINT32_MAX) fail("bitstream overflow");
        if((nbits&7u)==0)data.push_back(0);
        if(x)data.back()|=uint8_t(1u<<(nbits&7));
        ++nbits;
    }
    void msb(unsigned x, unsigned length) {
        for(unsigned i=length;i>0;--i)bit((x>>(i-1))&1u);
    }
};
struct BitSource {
    const uint8_t* data;
    uint32_t nbits, offset=0;
    bool read(bool allow_zero_extension=false) {
        if(offset>=nbits) {
            if(allow_zero_extension)return false;
            fail("truncated payload");
        }
        bool b=(data[offset>>3]>>(offset&7))&1;
        ++offset;return b;
    }
    unsigned msb(unsigned n) {
        unsigned x=0;
        for(unsigned i=0;i<n;++i)x=(x<<1)|read();
        return x;
    }
};

// Exact compact integer code for a uniform n-symbol alphabet.
void write_truncated(BitSink& b, unsigned rank, unsigned n) {
    if(n==0||rank>=n)fail("invalid truncated code");
    if(n==1)return;
    unsigned k=std::bit_width(n)-1;
    unsigned split=(1u<<(k+1))-n;
    if(rank<split)b.msb(rank,k);
    else b.msb(rank+split,k+1);
}
unsigned read_truncated(BitSource& b, unsigned n) {
    if(n==0)fail("empty alphabet");
    if(n==1)return 0;
    unsigned k=std::bit_width(n)-1;
    unsigned split=(1u<<(k+1))-n;
    unsigned r=b.msb(k);
    if(r<split)return r;
    unsigned v=((r<<1)|b.read())-split;
    if(v>=n)fail("invalid truncated code");
    return v;
}

// 32-bit classic binary arithmetic coder. Integer weights (sum < 32768).
struct ArithmeticEncoder {
    BitSink bits;
    uint64_t low=0,high=UINT32_MAX;
    unsigned pending=0;
    void emit(bool v) {
        bits.bit(v);
        while(pending>0){bits.bit(!v);--pending;}
    }
    void put(uint32_t before,uint32_t weight,uint32_t total) {
        if(!weight||!total||before+weight>total||total>=32768)fail("invalid arithmetic weights");
        uint64_t range=high-low+1;
        high=low+(range*(before+weight))/total-1;
        low=low+(range*before)/total;
        while(true) {
            if(high<0x80000000ULL)emit(false);
            else if(low>=0x80000000ULL){emit(true);low-=0x80000000ULL;high-=0x80000000ULL;}
            else if(low>=0x40000000ULL&&high<0xC0000000ULL){++pending;low-=0x40000000ULL;high-=0x40000000ULL;}
            else break;
            low<<=1;high=(high<<1)|1;
        }
    }
    EntropyPayload finish() {
        ++pending;
        emit(low>=0x40000000ULL);
        return {std::move(bits.data),bits.nbits};
    }
};
struct ArithmeticDecoder {
    BitSource bits;
    uint64_t low=0,high=UINT32_MAX,code=0;
    ArithmeticDecoder(const uint8_t* bytes,uint32_t nbits) : bits{bytes,nbits,0} {
        for(int i=0;i<32;++i)code=(code<<1)|bits.read(true);
    }
    uint32_t target(uint32_t total) const {
        if(!total||total>=32768)fail("invalid arithmetic total");
        uint64_t range=high-low+1;
        uint64_t x=((code-low+1)*total-1)/range;
        if(x>=total)fail("corrupt arithmetic payload");
        return uint32_t(x);
    }
    void take(uint32_t before,uint32_t weight,uint32_t total) {
        if(!weight||before+weight>total)fail("invalid arithmetic interval");
        uint64_t range=high-low+1;
        high=low+(range*(before+weight))/total-1;
        low=low+(range*before)/total;
        while(true) {
            if(high<0x80000000ULL){}
            else if(low>=0x80000000ULL){code-=0x80000000ULL;low-=0x80000000ULL;high-=0x80000000ULL;}
            else if(low>=0x40000000ULL&&high<0xC0000000ULL){code-=0x40000000ULL;low-=0x40000000ULL;high-=0x40000000ULL;}
            else break;
            low<<=1;high=(high<<1)|1;
            code=(code<<1)|bits.read(true);
        }
        if(code<low||code>high)fail("arithmetic decoder interval lost");
    }
};

// Candidate generation is deterministic. Pseudo-legal positions include pinned moves,
// and promotions are four distinct alphabet symbols. No heap allocations per ply.
struct Candidate { Move m; uint16_t w; uint8_t category; };
struct CandidateList {
    std::array<Candidate,256> a;
    unsigned size=0;
    uint32_t total=0;
    void add(Move m) {
        if(size==a.size())fail("too many pseudo moves");
        a[size++]={m,1,0};
    }
};

// Coarse structural type categories (11 counts) learn only from the current game.
enum Category : uint8_t { P_PUSH, P_DOUBLE, P_TAKE, KNIGHT, BISHOP, ROOK, QUEEN, KING, CASTLE, PROMOTION, RECAPTURE, COUNT };
struct Model {
    std::array<uint16_t,COUNT> freq{};
    Model(){freq.fill(1);}
    void update(uint8_t c) {
        if(freq[c]<48) ++freq[c];
    }
};

uint8_t group(const Position& p, const Move& m, const Move* prev) {
    int8_t piece=p.board[m.from];
    int kind=abs_piece(piece),df=int(m.to%8)-int(m.from%8);
    bool capture=p.board[m.to]!=0||(kind==1&&df!=0);
    if(m.promotion)return PROMOTION;
    if(kind==6&&std::abs(int(m.to)-int(m.from))==2)return CASTLE;
    if(prev && capture && m.to==prev->to)return RECAPTURE;
    if(kind==1)return capture?P_TAKE:((std::abs(int(m.to)-int(m.from))==16)?P_DOUBLE:P_PUSH);
    return uint8_t(kind==2?KNIGHT:kind==3?BISHOP:kind==4?ROOK:kind==5?QUEEN:KING);
}

// A heuristic, NOT a pretrained probability model. Explicitly fixed and testable.
unsigned positional_weight(const Position& p,Move m,const Move* previous,unsigned ply) {
    int kind=abs_piece(p.board[m.from]);
    int from_file=m.from&7,to_file=m.to&7,from_rank=m.from>>3,to_rank=m.to>>3;
    int own_rank=p.side?7-to_rank:to_rank;
    int start_rank=p.side?7-from_rank:from_rank;
    unsigned w=4;
    int central=6-(std::abs(to_file-3)+std::abs(to_file-4)+std::abs(to_rank-3)+std::abs(to_rank-4))/2;
    if(central<0)central=0;
    bool capture=p.board[m.to]!=0||(kind==1&&from_file!=to_file);
    if(capture)w+=9;
    if(previous && capture && m.to==previous->to)w+=15;
    if(kind==1) {
        w+=8;
        if(std::abs(int(m.to)-int(m.from))==16)w+=7;
        if(to_file==3||to_file==4)w+=8;
        if(own_rank==3||own_rank==4)w+=4;
    } else if(kind==2) {
        w+=8+central*2;
        if(start_rank==0 && (own_rank==2||own_rank==3))w+=8;
    } else if(kind==3){
        w+=5+central;
        if(start_rank==0 && own_rank>=1)w+=5;
    } else if(kind==4){
        w+=2;
    } else if(kind==5){
        w+=1;
        if(ply<24 && !capture)w=std::max(1u,w/2);
    } else if(kind==6){
        w+=1;
        if(std::abs(int(m.to)-int(m.from))==2)w+=30;
        else if(ply<30 && !capture)w=std::max(1u,w/2);
    }
    if(m.promotion) {
        w+=25;
        if(m.promotion!=1)w=std::max(1u,w/4);
    }
    return std::max(1u,w);
}


// v0.3 experimental features: fixed integer heuristics, no training or hidden parameters.
// Keeping separate modes prevents interpreting a lucky small-corpus result as a general improvement.
inline bool experimental_mode(Mode mode) {
    return mode==Mode::EntropyPositional || mode==Mode::EntropyTactical ||
           mode==Mode::EntropyRich || mode==Mode::EntropyCheckFiltered ||
           mode==Mode::EntropyTacticalCheck || mode==Mode::EntropyRichCheck ||
           mode==Mode::EntropyPhase || mode==Mode::EntropyPhaseTactical ||
           mode==Mode::EntropyPhaseCheck || mode==Mode::EntropyPhaseForcing ||
           mode==Mode::EntropyPhaseOpening || mode==Mode::EntropyPhaseMiddle ||
           mode==Mode::EntropyPhaseEndgame || mode==Mode::EntropyCheckSignal ||
           mode==Mode::EntropyCheckSignalLight;
}

// Cheap piece-square preference from rank progress and proximity to central 4 squares.
// This is not a full engine evaluation. It is deliberately capped and symmetrical.
int location_bonus(const Position& p, Move m) {
    int piece=abs_piece(p.board[m.from]);
    int ff=m.from&7, fr=m.from>>3, tf=m.to&7, tr=m.to>>3;
    int from_adv=p.side?7-fr:fr, to_adv=p.side?7-tr:tr;
    auto center=[](int file,int rank)->int {
        return (3-std::min(std::abs(file-3),std::abs(file-4)))+
               (3-std::min(std::abs(rank-3),std::abs(rank-4)));
    };
    int delta=center(tf,tr)-center(ff,fr);
    int score=0;
    switch(piece) {
        case 1: score=2*(to_adv-from_adv)+(tf>=2&&tf<=5?2:0);break;
        case 2: score=5*delta;break;
        case 3: score=3*delta;break;
        case 4: score=delta;break;
        case 5: score=2*delta;break;
        case 6: score=(from_adv<=1 && to_adv<=1)?0:-(delta*2);break;
    }
    return std::clamp(score,-20,20);
}

// Extra tactical cues. No search, no learned model, and no move-gen allocations.
// Capture values intentionally approximate material values; chess best-play isn't required.
// Attack squares of the opponent computed ONCE per board state, not O(candidates).
// All occupied squares are included as defended squares, unlike pseudo_destinations.
// Each ray stops at the first blocker (and includes that occupied square).
struct FastAttackTables {
    uint64_t ray[64][8]{},knight[64]{},king[64]{},pawn[2][64]{};
    constexpr FastAttackTables() {
        constexpr int d[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
        constexpr int n[8][2]={{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
        for(int sq=0;sq<64;++sq) {
            int f=sq&7,r=sq>>3;
            for(int k=0;k<8;++k) {
                int nf=f+d[k][0],nr=r+d[k][1];
                if(unsigned(nf)<8&&unsigned(nr)<8)king[sq]|=uint64_t(1)<<(nr*8+nf);
                while(unsigned(nf)<8&&unsigned(nr)<8){ray[sq][k]|=uint64_t(1)<<(nr*8+nf);nf+=d[k][0];nr+=d[k][1];}
                nf=f+n[k][0];nr=r+n[k][1];
                if(unsigned(nf)<8&&unsigned(nr)<8)knight[sq]|=uint64_t(1)<<(nr*8+nf);
            }
            for(int c=0;c<2;++c)for(int df:{-1,1}){
                int nf=f+df,nr=r+(c?-1:1);
                if(unsigned(nf)<8&&unsigned(nr)<8)pawn[c][sq]|=uint64_t(1)<<(nr*8+nf);
            }
        }
    }
};
inline constexpr FastAttackTables fast_attack_tables{};
struct AttackCache {
    uint64_t attacked_mask=0;
    explicit AttackCache(const Position& p) {
        auto& t=fast_attack_tables;int side=p.side^1;
        uint64_t pieces=p.occupied[side],occ=p.occupied[0]|p.occupied[1];
        while(pieces){
            int from=std::countr_zero(pieces);pieces&=pieces-1;
            int kind=abs_piece(p.board[from]);
            if(kind==1){attacked_mask|=t.pawn[side][from];continue;}
            if(kind==2){attacked_mask|=t.knight[from];continue;}
            if(kind==6){attacked_mask|=t.king[from];continue;}
            int lo=kind==3?4:0,hi=kind==4?4:8;
            for(int dir=lo;dir<hi;++dir){
                auto ray=t.ray[from][dir];auto blocks=ray&occ;
                if(blocks){
                    int near=(dir==0||dir==2||dir==4||dir==6)?std::countr_zero(blocks):63-std::countl_zero(blocks);
                    ray &= ~t.ray[near][dir];
                }
                attacked_mask|=ray;
            }
        }
#ifdef RMC_VERIFY_ATTACKS
        for(int sq=0;sq<64;++sq)if(((attacked_mask&bit(sq))!=0)!=p.square_attacked(sq,p.side^1))fail("fast attack mismatch");
#endif
    }
    bool attacked(int sq)const{return(attacked_mask&bit(sq))!=0;}
};
// Reuse unaffected own-piece attacks across legal candidate moves. Full keys
// and occupancy deltas make this exact for captures, EP, castling and promotion.
struct OwnAttackReuse {
    std::array<uint16_t,64> source_id{},ray_sources{};
    std::array<uint8_t,16> source_square{};
    std::array<std::array<uint64_t,16>,4> unions{};
    uint16_t all_ids=0;unsigned groups=0;bool fallback=false;
    static uint64_t piece_attack(const Position& p,int sq) {
        const auto& t=fast_attack_tables;const int kind=abs_piece(p.board[sq]);
        const int side=p.board[sq]>0?0:1;
        if(kind==1)return t.pawn[side][sq];
        if(kind==2)return t.knight[sq];
        if(kind==6)return t.king[sq];
        uint64_t out=0,occ=p.occupied[0]|p.occupied[1];
        for(int d=kind==3?4:0;d<(kind==4?4:8);++d){
            auto ray=t.ray[sq][d];auto blockers=ray&occ;
            if(blockers){int near=(d==0||d==2||d==4||d==6)?std::countr_zero(blockers):63-std::countl_zero(blockers);
                ray&=~t.ray[near][d];}
            out|=ray;
        }
        return out;
    }
    explicit OwnAttackReuse(const Position&p){
        const unsigned count=std::popcount(p.occupied[p.side]);
        if(count>16){fallback=true;return;}
        all_ids=count==16?uint16_t(65535):uint16_t((1u<<count)-1);groups=(count+3)/4;
        std::array<uint64_t,16> attacks{};unsigned id=0;
        for(uint64_t pieces=p.occupied[p.side];pieces;pieces&=pieces-1,++id){
            const int sq=std::countr_zero(pieces);const uint16_t flag=uint16_t(1u<<id);
            source_square[id]=uint8_t(sq);source_id[sq]=flag;attacks[id]=piece_attack(p,sq);
            const int kind=abs_piece(p.board[sq]);if(kind<3||kind>5)continue;
            uint64_t rays=0;for(int d=kind==3?4:0;d<(kind==4?4:8);++d)rays|=fast_attack_tables.ray[sq][d];
            while(rays){int target=std::countr_zero(rays);rays&=rays-1;ray_sources[target]|=flag;}
        }
        for(unsigned group=0;group<groups;++group)for(unsigned mask=1;mask<16;++mask){
            unsigned low=std::countr_zero(mask);unions[group][mask]=unions[group][mask&(mask-1)]|attacks[group*4+low];
        }
    }
    uint64_t after(const Position&before,const Position&next)const{
        const unsigned side=before.side;uint64_t out=0;
        if(fallback){for(uint64_t pieces=next.occupied[side];pieces;pieces&=pieces-1)out|=piece_attack(next,std::countr_zero(pieces));}
        else{
            uint16_t affected=0;
            uint64_t changed=(before.occupied[0]|before.occupied[1])^(next.occupied[0]|next.occupied[1]);
            while(changed){int sq=std::countr_zero(changed);changed&=changed-1;affected|=ray_sources[sq];}
            uint64_t removed=before.occupied[side]&~next.occupied[side];
            while(removed){int sq=std::countr_zero(removed);removed&=removed-1;affected|=source_id[sq];}
            const unsigned unchanged=all_ids&~affected;
            for(unsigned group=0;group<groups;++group)out|=unions[group][(unchanged>>(group*4))&15];
            unsigned pending=affected;
            while(pending){unsigned id=std::countr_zero(pending);pending&=pending-1;int sq=source_square[id];if(next.occupied[side]&bit(sq))out|=piece_attack(next,sq);}
            uint64_t added=next.occupied[side]&~before.occupied[side];
            while(added){int sq=std::countr_zero(added);added&=added-1;out|=piece_attack(next,sq);}
        }
#ifdef DMT_VERIFY_REUSE
        AttackCache oracle(next);if(out!=oracle.attacked_mask)fail("grouped attack reuse mismatch");
#endif
        return out;
    }
};
int tactical_bonus(const Position& p, Move m, const Move* previous, AttackCache& cache) {
    int kind=abs_piece(p.board[m.from]);
    int captured=abs_piece(p.board[m.to]);
    if(kind==1 && (m.to&7)!=(m.from&7) && !captured)captured=1; // en passant
    static constexpr int value[7]={0,1,3,3,5,9,20};
    int result=0;
    if(captured) {
        result+=std::min(22,5*value[captured]);
        result-=std::max(0,(value[kind]-value[captured])*2);
    }
    if(previous && m.to==previous->to && captured)result+=12;
    // Safety: only two queries per move. These are pre-move occupancy attacks, so
    // not a static-exchange evaluation; learned limits must be respected.
    const bool attacked_from=cache.attacked(m.from);
    const bool attacked_to=cache.attacked(m.to);
    if(attacked_from)result+=12;
    if(attacked_to && !captured)result-=std::min(20,4*value[kind]);
    if(attacked_to && captured && value[kind]>value[captured])result-=5;
    return std::clamp(result,-35,35);
}

// v0.5: stage-conditioned, hand-authored structural features (no trained tables).
// Phase uses the standard material-phase interpolation (N/B=1, R=2, Q=4 per side).
// The phase is recomputed from 12 64-bit piece masks so encoder and decoder agree.
unsigned material_phase(const Position& p) {
    unsigned phase=0;
    for(int c=0;c<2;++c) {
        phase+=std::popcount(p.piece_bits[c][1]); // knights
        phase+=std::popcount(p.piece_bits[c][2]); // bishops
        phase+=2u*std::popcount(p.piece_bits[c][3]); // rooks
        phase+=4u*std::popcount(p.piece_bits[c][4]); // queens
    }
    return std::min(24u,phase);
}

int stage_bonus(const Position& p, Move m, unsigned ply, unsigned phase) {
    const int piece=abs_piece(p.board[m.from]);
    const int fromfile=m.from&7, tofile=m.to&7;
    const int fromrank=m.from>>3, torank=m.to>>3;
    const int fromadv=p.side?7-fromrank:fromrank;
    const int toadv=p.side?7-torank:torank;
    const bool capture=p.board[m.to]!=0 || (piece==1 && fromfile!=tofile);
    int bonus=0;
    if(phase>=19 && ply<30) { // opening / developing pieces
        if(piece==1 && !capture) {
            if(tofile==3 || tofile==4) bonus+=10;
            else if(tofile==2) bonus+=4;
            if(toadv==3 && fromadv==1) bonus+=5;
        }
        if(piece==2 && fromadv==0 && (toadv==2||toadv==3)) bonus+=12;
        if(piece==3 && fromadv==0 && toadv>=1) bonus+=8;
        if(piece==6 && std::abs(int(m.to)-int(m.from))==2) bonus+=16;
        if(!capture && piece==5) bonus-=9;
        if(!capture && piece==4) bonus-=4;
    } else if(phase<=8) { // endgame: active king, advanced pawns
        if(piece==1 && !capture) {
            bonus+=std::max(0,toadv-fromadv)*4;
            if(toadv>=5) bonus+=9;
            if(toadv>=6) bonus+=12;
        }
        if(piece==6) {
            auto cent=[](int sq) {int f=sq&7,r=sq>>3;
                return std::min(f,7-f)+std::min(r,7-r);};
            bonus+=5*(cent(m.to)-cent(m.from));
        }
        if(m.promotion==1) bonus+=16;
    } else { // middlegame: modest extra pressure toward king side
        if(capture) bonus+=6;
        if(piece==1 && !capture && (tofile==3||tofile==4)) bonus+=4;
    }
    return std::clamp(bonus,-30,42);
}

// Check-delivery cue. Full post-move attacked-square calculation includes discovered
// checks. More expensive than tactical features, kept in an ablation-only mode.
int direct_check_bonus(const Position& p, Move m) {
    Position next=p;
    next.apply_unchecked(m);
    const int enemy_king=next.king_square(next.side);
    if(enemy_king<0)return 0;
    return next.square_attacked(enemy_king, p.side)?24:0;
}

// Candidate list may be trimmed only during check, where pseudo-legal alphabets
// can contain huge numbers of impossible moves. Deterministic both ways.
void remove_illegal_during_check(CandidateList& c,const Position& p) {
    int sq=p.king_square(p.side);
    if(sq<0 || !p.square_attacked(sq,p.side^1))return;
    unsigned write=0;
    for(unsigned i=0;i<c.size;++i) {
        Position trial=p;
        if(trial.apply_checked(c.a[i].m))c.a[write++]=c.a[i];
    }
    c.size=write;
    if(!write)fail("checked position has no legal continuation");
}
CandidateList candidates(const Position& p,Mode mode,const Move* previous,unsigned ply,const Model& model) {
    CandidateList c;
    uint64_t ours=p.occupied[p.side];
    while(ours) {
        int from=std::countr_zero(ours);ours&=ours-1;
        uint64_t mask=p.pseudo_destinations(from);
        while(mask) {
            int to=std::countr_zero(mask);mask&=mask-1;
            bool prom=abs_piece(p.board[from])==1 && ((to>>3)==0||(to>>3)==7);
            int max=prom?4:1;
            for(int i=0;i<max;++i)c.add(Move{uint8_t(from),uint8_t(to),uint8_t(prom?i+1:0)});
        }
    }
    if(!c.size)fail("position has no pseudo-legal moves");
    if(mode==Mode::EntropyCheckFiltered || mode==Mode::EntropyTacticalCheck ||
       mode==Mode::EntropyRichCheck || mode==Mode::EntropyPhaseCheck ||
       mode==Mode::EntropyPhaseForcing || mode==Mode::EntropyPhaseOpening ||
       mode==Mode::EntropyPhaseMiddle || mode==Mode::EntropyPhaseEndgame ||
       mode==Mode::EntropyCheckSignal || mode==Mode::EntropyCheckSignalLight)remove_illegal_during_check(c,p);
    if(mode==Mode::EntropyStatic||mode==Mode::EntropyAdaptive||experimental_mode(mode)) {
        uint32_t raw_total=0;
        AttackCache cache(p);
        // Phase is a property of the POSITION, not of each candidate.
        const unsigned stage=material_phase(p);
        const bool opening=stage>=19 && ply<30;
        const bool endgame=stage<=8;
        for(unsigned i=0;i<c.size;++i) {
            auto& x=c.a[i];
            x.category=group(p,x.m,previous);
            unsigned w=positional_weight(p,x.m,previous,ply);
            if(mode==Mode::EntropyPositional||mode==Mode::EntropyRich||mode==Mode::EntropyRichCheck)
                w=std::max(1,int(w)+location_bonus(p,x.m));
            if(mode==Mode::EntropyTactical||mode==Mode::EntropyRich||mode==Mode::EntropyTacticalCheck||mode==Mode::EntropyRichCheck || mode==Mode::EntropyPhaseTactical ||
               mode==Mode::EntropyPhaseCheck || mode==Mode::EntropyPhaseForcing ||
               mode==Mode::EntropyPhaseOpening || mode==Mode::EntropyPhaseMiddle ||
               mode==Mode::EntropyPhaseEndgame || mode==Mode::EntropyCheckSignal ||
               mode==Mode::EntropyCheckSignalLight)
                w=std::max(1,int(w)+tactical_bonus(p,x.m,previous,cache));
            if(mode==Mode::EntropyPhase || mode==Mode::EntropyPhaseTactical ||
               mode==Mode::EntropyPhaseCheck || mode==Mode::EntropyPhaseForcing ||
               (mode==Mode::EntropyPhaseOpening && opening) ||
               (mode==Mode::EntropyPhaseMiddle && !opening && !endgame) ||
               (mode==Mode::EntropyPhaseEndgame && endgame))
                w=std::max(1, int(w)+stage_bonus(p,x.m,ply,stage));
            if(mode==Mode::EntropyPhaseForcing || mode==Mode::EntropyCheckSignal ||
               mode==Mode::EntropyCheckSignalLight)
                w=std::max(1, int(w)+direct_check_bonus(p,x.m)*
                           (mode==Mode::EntropyCheckSignalLight ? 1 : 2)/2);
            if(mode==Mode::EntropyCheckFiltered) {
                // Same fixed structural model on smaller legal alphabet during check.
            }
            if(mode==Mode::EntropyAdaptive) {
                // Bounded within-game adaptation on move type. Strong prior prevents early overfitting.
                w=(w*(12u+std::min<unsigned>(model.freq[x.category],32u)))/12u;
            }
            x.w=uint16_t(std::max(1u,w));
            raw_total+=x.w;
        }
        // Arithmetic coder requires total < 32768; practical max on plausible chess boards
        // is well below this. If exceeded, scale the weights together deterministically.
        if(raw_total>=32768) {
            unsigned factor=(raw_total+16383)/16384;
            for(unsigned i=0;i<c.size;++i)c.a[i].w=uint16_t(std::max(1u,unsigned(c.a[i].w)/factor));
        }
    }
    for(unsigned i=0;i<c.size;++i)c.total+=c.a[i].w;
    if(c.total>=32768)fail("arithmetic total too large");
    return c;
}

unsigned index_of(const CandidateList& c,Move m) {
    for(unsigned i=0;i<c.size;++i)if(c.a[i].m==m)return i;
    fail("legal move missing from pseudo-move alphabet");
}
}

EntropyPayload entropy_encode(const std::vector<Move>& moves,Mode mode,Position p) {
    if(mode!=Mode::DMGTruncated&&mode!=Mode::EntropyUniform&&mode!=Mode::EntropyStatic&&mode!=Mode::EntropyAdaptive&&!experimental_mode(mode))fail("not an entropy mode");
    BitSink binary;
    ArithmeticEncoder arithmetic;
    Model model;
    Move previous{};
    bool has_previous=false;
    for(unsigned ply=0;ply<moves.size();++ply) {
        Move m=moves[ply];
        Position next=p;
        if(!next.apply_checked(m))fail("illegal move in entropy input");
        CandidateList c=candidates(p,mode,has_previous?&previous:nullptr,ply,model);
        unsigned i=index_of(c,m);
        if(mode==Mode::DMGTruncated)write_truncated(binary,i,c.size);
        else {
            uint32_t before=0;
            for(unsigned j=0;j<i;++j)before+=c.a[j].w;
            arithmetic.put(before,c.a[i].w,c.total);
        }
        model.update(c.a[i].category);
        previous=m;has_previous=true;
        p=std::move(next);
    }
    if(mode==Mode::DMGTruncated)return {std::move(binary.data),binary.nbits};
    // Empty games need no entropy payload, consistent with the baseline modes.
    if(moves.empty())return {};
    return arithmetic.finish();
}

std::vector<Move> entropy_decode(const uint8_t* bytes,uint32_t bits,uint32_t count,Mode mode,Position p) {
    if(mode!=Mode::DMGTruncated&&mode!=Mode::EntropyUniform&&mode!=Mode::EntropyStatic&&mode!=Mode::EntropyAdaptive&&!experimental_mode(mode))fail("not an entropy mode");
    BitSource binary{bytes,bits,0};
    ArithmeticDecoder arithmetic(bytes,bits);
    Model model;
    Move previous{};
    bool has_previous=false;
    std::vector<Move> out;
    out.reserve(count);
    for(unsigned ply=0;ply<count;++ply) {
        CandidateList c=candidates(p,mode,has_previous?&previous:nullptr,ply,model);
        unsigned index=0;
        if(mode==Mode::DMGTruncated)index=read_truncated(binary,c.size);
        else {
            uint32_t value=arithmetic.target(c.total),sum=0;
            for(;index<c.size;++index) {
                unsigned next=sum+c.a[index].w;
                if(value<next)break;
                sum=next;
            }
            if(index>=c.size)fail("arithmetic index invalid");
            arithmetic.take(sum,c.a[index].w,c.total);
        }
        Move m=c.a[index].m;
        if(!p.apply_checked(m))fail("corrupt entropy stream or illegal move");
        model.update(c.a[index].category);
        previous=m;has_previous=true;
        out.push_back(m);
    }
    if(mode==Mode::DMGTruncated&&binary.offset!=binary.nbits)fail("trailing DMG bits");
    return out;
}

namespace {
// v0.6 models operate on the SAME candidate ordering and stream format.
// All adaptation is reconstructed from earlier DECODED moves only.
constexpr uint32_t V6_SCALE=16000;
using WeightArray=std::array<uint16_t,256>;
struct V6Weights {WeightArray weights{};uint32_t total=0;};
using V6ModelArray=std::array<V6Weights,4>;
struct V6Context {
    std::array<uint32_t,COUNT> group_counts{};
    uint32_t seen=0;
    std::array<uint32_t,4> alpha{16384,16384,16384,16384};
};
struct V6Learn {
    // Exact state bytes (board, side, castling rights, en passant);
    // no hash-only matching, so collisions can't change the decoding model.
    std::unordered_map<std::string,std::vector<std::pair<Move,uint16_t>>> states;
    static std::string key(const Position& p) {
        std::string result(67,'\0');
        for(int i=0;i<64;++i)result[i]=char(p.board[i]);
        result[64]=char(p.side);
        result[65]=char(p.castling);
        result[66]=char(p.en_passant);
        return result;
    }
    uint32_t count(const std::string& key,Move move) const {
        auto it=states.find(key);
        if(it==states.end())return 0;
        for(auto& [m,n]:it->second)if(m==move)return n;
        return 0;
    }
    void observe(const std::string& key,Move move) {
        auto& a=states[key];
        for(auto& [m,n]:a)if(m==move){if(n<63)++n;return;}
        a.emplace_back(move,1);
    }
};
// Exact invariant division: reciprocal estimate plus remainder correction.
// The fallback preserves C++20 portability where 128-bit multiply is absent.
struct ExactDivisor {
    uint64_t d;
#ifdef __SIZEOF_INT128__
    uint64_t reciprocal;
    explicit ExactDivisor(uint64_t divisor):d(divisor),reciprocal(UINT64_MAX/divisor){}
    uint64_t operator()(uint64_t x)const{
        uint64_t q=uint64_t((__uint128_t(x)*reciprocal)>>64);
        return q+uint64_t(x-q*d>=d);
    }
#else
    explicit ExactDivisor(uint64_t divisor):d(divisor){}
    uint64_t operator()(uint64_t x)const{return x/d;}
#endif
};
V6Weights v6_quantize(const std::array<uint64_t,256>& raw,unsigned n) {
    if(!n || n>=V6_SCALE)fail("bad V6 alphabet");
    uint64_t sum=0;
    for(unsigned i=0;i<n;++i)sum+=raw[i];
    if(!sum)fail("zero model mass");
    V6Weights q;
    const ExactDivisor divide(sum);
    for(unsigned i=0;i<n;++i) {
        q.weights[i]=uint16_t(1 + divide(raw[i]*(V6_SCALE-n)));
        q.total+=q.weights[i];
    }
    if(q.total>=32768)fail("V6 weights too large");
    return q;
}
V6Weights v6_uniform(unsigned n) {
    V6Weights q;
    for(unsigned i=0;i<n;++i)q.weights[i]=1;
    q.total=n;
    return q;
}
V6Weights v6_copy(const CandidateList& c) {
    V6Weights q;
    q.total=c.total;
    for(unsigned i=0;i<c.size;++i)q.weights[i]=c.a[i].w;
    return q;
}
V6Weights v6_hier(const CandidateList& c,const V6Context& ctx) {
    std::array<uint32_t,COUNT> mass{};
    for(unsigned i=0;i<c.size;++i)mass[c.a[i].category]+=c.a[i].w;
    std::array<uint64_t,256> raw{};
    // A group-level adaptive prior, followed by within-group baseline distribution.
    for(unsigned i=0;i<c.size;++i) {
        const unsigned g=c.a[i].category;
        const uint32_t gm=mass[g];
        raw[i]=uint64_t(c.a[i].w)*(12u+(uint64_t(ctx.group_counts[g])*c.total*4u)/gm);
    }
    return v6_quantize(raw,c.size);
}
V6Weights v6_dict(const CandidateList& c, const V6Learn& memory,const std::string& key) {
    std::array<uint64_t,256> raw{};
    for(unsigned i=0;i<c.size;++i) {
        uint32_t hits=memory.count(key,c.a[i].m);
        raw[i]=uint64_t(c.a[i].w)*16 + uint64_t(hits)*c.total*4;
    }
    return v6_quantize(raw,c.size);
}
V6Weights v6_mix(const V6ModelArray& models,const V6Context& ctx,unsigned k,unsigned n) {
    std::array<uint64_t,256> raw{};
    for(unsigned i=0;i<n;++i) {
        for(unsigned j=0;j<k;++j)raw[i] += uint64_t(ctx.alpha[j])*models[j].weights[i]*(1048576u/models[j].total);
    }
    return v6_quantize(raw,n);
}
void v6_posterior(V6Context& ctx,const V6ModelArray& models,unsigned chosen,unsigned k) {
    std::array<uint64_t,4> likelihood{};
    uint64_t total=0;
    for(unsigned j=0;j<k;++j) {
        likelihood[j]=uint64_t(ctx.alpha[j])*models[j].weights[chosen]*1048576u/models[j].total;
        total+=likelihood[j];
    }
    if(!total)fail("posterior zero");
    for(unsigned j=0;j<k;++j)
        ctx.alpha[j]=std::max<uint32_t>(1, uint32_t((likelihood[j]*65536u)/total));
}
struct V6Step {
    CandidateList c;
    V6Weights coded;
    V6ModelArray models;
    unsigned model_count=0;
};
V6Step v6_step(const Position& p, const Move* previous, unsigned ply, V6Model mode,
               const V6Context& ctx,const V6Learn& memory,const std::string& key) {
    V6Step step;
    Model dummy;
    step.c=candidates(p,Mode::EntropyCheckSignalLight,previous,ply,dummy);
    if(mode==V6Model::Baseline) { step.coded=v6_copy(step.c);return step; }
    if(mode==V6Model::Hierarchical) {step.coded=v6_hier(step.c,ctx);return step;}
    if(mode==V6Model::Dictionary) {step.coded=v6_dict(step.c,memory,key);return step;}
    step.models[0]=v6_copy(step.c);
    step.models[1]=v6_uniform(step.c.size);
    step.models[2]=v6_hier(step.c,ctx);
    step.model_count=mode==V6Model::All?4:3;
    if(step.model_count==4)step.models[3]=v6_dict(step.c,memory,key);
    step.coded=v6_mix(step.models,ctx,step.model_count,step.c.size);
    return step;
}
uint32_t v6_before(const V6Weights& w,unsigned index) {
    uint32_t value=0;
    for(unsigned i=0;i<index;++i)value+=w.weights[i];
    return value;
}
unsigned v6_select(const V6Weights& w,unsigned n,uint32_t target,uint32_t& before) {
    before=0;
    for(unsigned i=0;i<n;++i) {
        if(target<before+w.weights[i])return i;
        before+=w.weights[i];
    }
    fail("invalid V6 arithmetic target");
}
void v6_advance(V6Context& ctx, const V6Step& step,unsigned index) {
    if(step.model_count)v6_posterior(ctx,step.models,index,step.model_count);
    auto cat=step.c.a[index].category;
    if(ctx.group_counts[cat]<4095)++ctx.group_counts[cat];
    ++ctx.seen;
}
void v6_varint(std::vector<uint8_t>& v,uint32_t x) {
    while(x>=128){v.push_back(uint8_t((x&127)|128));x>>=7;}
    v.push_back(uint8_t(x));
}
uint32_t v6_unvar(const std::vector<uint8_t>& v,size_t& pos) {
    uint64_t x=0;
    for(int i=0;i<5;++i) {
        if(pos>=v.size())fail("truncated V6 varint");
        uint8_t byte=v[pos++];
        x |= uint64_t(byte&127)<<(7*i);
        if(!(byte&128)) {
            if(x>UINT32_MAX)fail("V6 varint overflow");
            return uint32_t(x);
        }
    }
    fail("V6 varint too long");
}
uint32_t v6_crc(const uint8_t* p,size_t size) {
    uint32_t crc=0xFFFFFFFFu;
    for(size_t i=0;i<size;++i) {
        crc^=p[i];
        for(int j=0;j<8;++j)crc=(crc>>1)^((crc&1u)?0xEDB88320u:0);
    }
    return ~crc;
}
}
const char* v6_name(V6Model mode) {
    switch(mode) {
        case V6Model::Baseline:return "baseline";
        case V6Model::Hierarchical:return "hierarchical";
        case V6Model::Mixture:return "mixture";
        case V6Model::Dictionary:return "dictionary";
        case V6Model::All:return "all";
    }
    return "invalid";
}
V6Archive v6_encode(const std::vector<std::vector<Move>>& games,V6Model mode) {
    if(unsigned(mode)>unsigned(V6Model::All))fail("unknown V6 model");
    if(games.size()>100000)fail("too many games");
    ArithmeticEncoder ar;
    V6Learn memory;
    uint64_t total_plies=0;
    for(const auto& game:games) {
        if(game.size()>1000000)fail("game too long");
        total_plies+=game.size();
        if(total_plies>300000000ULL)fail("V6 archive too large");
        Position p=Position::standard();
        V6Context ctx;
        Move previous{};bool has_prev=false;
        for(unsigned ply=0;ply<game.size();++ply) {
            const Move m=game[ply];
            Position next=p;
            if(!next.apply_checked(m))fail("illegal input move");
            const std::string key=V6Learn::key(p);
            V6Step s=v6_step(p,has_prev?&previous:nullptr,ply,mode,ctx,memory,key);
            unsigned ix=index_of(s.c,m);
            ar.put(v6_before(s.coded,ix),s.coded.weights[ix],s.coded.total);
            v6_advance(ctx,s,ix);
            if(mode==V6Model::Dictionary||mode==V6Model::All)memory.observe(key,m);
            p=std::move(next);previous=m;has_prev=true;
        }
    }
    EntropyPayload payload=total_plies?ar.finish():EntropyPayload{};
    std::vector<uint8_t> output={'D','M','G','6',1,uint8_t(mode)};
    v6_varint(output,uint32_t(games.size()));
    for(auto& game:games)v6_varint(output,uint32_t(game.size()));
    v6_varint(output,payload.bit_count);
    const uint64_t frame=output.size()+4;
    output.insert(output.end(),payload.bytes.begin(),payload.bytes.end());
    uint32_t crc=v6_crc(output.data(),output.size());
    for(unsigned i=0;i<4;++i)output.push_back(uint8_t(crc>>(8*i)));
    return {std::move(output),payload.bit_count,frame};
}
std::vector<std::vector<Move>> v6_decode(const std::vector<uint8_t>& bytes) {
    if(bytes.size()<10 || bytes[0]!='D'||bytes[1]!='M'||bytes[2]!='G'||bytes[3]!='6'||bytes[4]!=1)
        fail("invalid V6 archive signature");
    if(bytes[5]>unsigned(V6Model::All))fail("unknown V6 mode");
    const auto mode=V6Model(bytes[5]);
    uint32_t crc=v6_crc(bytes.data(),bytes.size()-4),given=0;
    for(unsigned i=0;i<4;++i)given|=uint32_t(bytes[bytes.size()-4+i])<<(8*i);
    if(crc!=given)fail("V6 archive checksum mismatch");
    size_t pos=6;
    const uint32_t n=v6_unvar(bytes,pos);
    if(n>100000)fail("too many games");
    std::vector<uint32_t> sizes;
    sizes.reserve(n);
    uint64_t total=0;
    for(unsigned i=0;i<n;++i) {
        uint32_t count=v6_unvar(bytes,pos);
        if(count>1000000)fail("V6 game too long");
        total+=count;
        if(total>300000000ULL)fail("V6 archive too long");
        sizes.push_back(count);
    }
    const uint32_t bits=v6_unvar(bytes,pos);
    if((bits+7ULL)/8 !=bytes.size()-pos-4)fail("V6 payload length mismatch");
    if(total>0&&!bits)fail("missing V6 payload");
    ArithmeticDecoder ar(bytes.data()+pos,bits);
    V6Learn memory;
    std::vector<std::vector<Move>> result;
    result.reserve(n);
    for(uint32_t length:sizes) {
        Position p=Position::standard();
        V6Context ctx;
        Move previous{};bool has_prev=false;
        std::vector<Move> game;
        game.reserve(length);
        for(uint32_t ply=0;ply<length;++ply) {
            const std::string key=V6Learn::key(p);
            V6Step s=v6_step(p,has_prev?&previous:nullptr,ply,mode,ctx,memory,key);
            uint32_t before=0;
            unsigned ix=v6_select(s.coded,s.c.size,ar.target(s.coded.total),before);
            ar.take(before,s.coded.weights[ix],s.coded.total);
            Move m=s.c.a[ix].m;
            if(!p.apply_checked(m))fail("V6 decoded move illegal");
            v6_advance(ctx,s,ix);
            if(mode==V6Model::Dictionary||mode==V6Model::All)memory.observe(key,m);
            previous=m;has_prev=true;
            game.push_back(m);
        }
        result.push_back(std::move(game));
    }
    return result;
}

}

namespace rmc {
namespace {
// DMG7 pattern memories share move motifs between DIFFERENT positions and colors.
// All learning comes exclusively from previously decoded moves. Key packing is
// collision-free for the documented features; exact positions still use full state.
struct V7Patterns {
    std::unordered_map<uint32_t,uint16_t> geometry;
    std::unordered_map<uint32_t,uint16_t> squares;
    struct Opportunity {uint32_t expected=0,actual=0;};
    Goal10OpTable<Opportunity> geom_op;
    mutable Goal10OpTable<Opportunity> square_op;
    static unsigned phase(const Position& p,unsigned ply) {
        if(ply<24 && material_phase(p)>=18)return 0;
        if(material_phase(p)<=8)return 2;
        return 1;
    }
    static bool capture(const Position& p,Move m,int kind) {
        return p.board[m.to]!=0 || (kind==1 && (m.from&7)!=(m.to&7));
    }
    static uint32_t geometry_key(const Position& p,Move m,unsigned ply) {
        const unsigned kind=unsigned(abs_piece(p.board[m.from]));
        const unsigned dx=unsigned(std::abs(int(m.to&7)-int(m.from&7)));
        const int signed_delta=int(m.to>>3)-int(m.from>>3);
        const unsigned dy=unsigned((p.side ? -signed_delta : signed_delta)+7);
        const unsigned cap=capture(p,m,int(kind));
        // [2 phase|3 kind|3 abs(dx)|4 dy+7|1 capture|3 promotion]
        return (((((phase(p,ply)*8u+kind)*8u+dx)*16u+dy)*2u+cap)*8u+m.promotion);
    }
    static uint32_t square_key(const Position& p,Move m,unsigned ply) {
        const unsigned kind=unsigned(abs_piece(p.board[m.from]));
        const unsigned from=p.side?(m.from^56u):m.from;
        const unsigned to=p.side?(m.to^56u):m.to;
        const unsigned cap=capture(p,m,int(kind));
        // [2 phase|3 kind|6 from|6 to|1 capture|3 promotion]
        return (((((phase(p,ply)*8u+kind)*64u+from)*64u+to)*2u+cap)*8u+m.promotion);
    }
    static uint32_t hits(const std::unordered_map<uint32_t,uint16_t>& db,uint32_t key) {
        auto it=db.find(key);
        return it==db.end()?0u:unsigned(it->second);
    }
    static void record(std::unordered_map<uint32_t,uint16_t>& db,uint32_t key) {
        auto &v=db[key];
        if(v<255u)++v;
    }
    void observe(const Position& p,Move m,unsigned ply) {
        record(geometry,geometry_key(p,m,ply));
        record(squares,square_key(p,m,ply));
    }
    static void saturating_add(uint32_t& value,uint32_t x) {
        value=uint32_t(std::min<uint64_t>(uint64_t(value)+x,1000000000ULL));
    }
    void observe_opportunity(const Position& p,const CandidateList& c,unsigned ix,unsigned ply,
                             bool update_geom,bool update_square) {
        // Estimate the expected hit count under the original baseline model.
        // One opportunity contributes sum_i p_base(i) = 1 (4096 fixed-point units).
        for(unsigned i=0;i<c.size;++i) {
            unsigned e=unsigned((uint64_t(c.a[i].w)*4096u+c.total/2)/c.total);
            const auto m=c.a[i].m;
            if(update_geom) {
                auto& g=geom_op[geometry_key(p,m,ply)];
                saturating_add(g.expected,e);
            }
            if(update_square) {
                auto& q=square_op[square_key(p,m,ply)];
                saturating_add(q.expected,e);
            }
        }
        if(update_geom)saturating_add(geom_op[geometry_key(p,c.a[ix].m,ply)].actual,4096u);
        if(update_square)saturating_add(square_op[square_key(p,c.a[ix].m,ply)].actual,4096u);
    }
};
struct V7Context {
    V6Context base;
    std::array<uint32_t,8> posterior{10923u,10923u,10923u,10923u,10923u,10921u,0,0};
    std::array<uint32_t,8> opportunity_alpha{8192u,8192u,8192u,8192u,8192u,8192u,8192u,8192u};
    std::array<uint32_t,8> selective_alpha{16384u,16384u,16384u,16384u,0,0,0,0};
};
struct V7Step {
    CandidateList c;
    V6Weights coded;
    std::array<V6Weights,8> experts{};
    unsigned experts_n=0;
};
V6Weights v7_pattern(const CandidateList& c,const Position& p,unsigned ply,
                     const V7Patterns& db,bool geometry) {
    std::array<uint64_t,256> raw{};
    const auto& counts=geometry?db.geometry:db.squares;
    for(unsigned i=0;i<c.size;++i) {
        const Move m=c.a[i].m;
        unsigned h=V7Patterns::hits(counts,geometry?
                     V7Patterns::geometry_key(p,m,ply):V7Patterns::square_key(p,m,ply));
        // A structured prior keeps rare/unseen motifs decodable; prior mass is
        // proportional to the v0.5 move model. All arithmetic is integer-only.
        raw[i]=uint64_t(c.a[i].w)*16u + uint64_t(h)*c.total*(geometry?1u:3u);
    }
    return v6_quantize(raw,c.size);
}
V6Weights v7_opportunity(const CandidateList& c,const Position& p,unsigned ply,
                         const V7Patterns& db,bool geometry) {
    std::array<uint64_t,256> raw{};
    const auto& table=geometry?db.geom_op:db.square_op;
    constexpr uint64_t prior=4u*4096u;
    for(unsigned i=0;i<c.size;++i) {
        uint32_t key=geometry?V7Patterns::geometry_key(p,c.a[i].m,ply):
                              V7Patterns::square_key(p,c.a[i].m,ply);
        auto it=table.find(key);
        uint64_t hits=it==table.end()?0:it->second.actual;
        uint64_t exp=it==table.end()?0:it->second.expected;
        // Positive or negative correction against opportunity-adjusted baseline.
        // Clip extreme corrections to [1/3,3] to avoid memorising noise.
        uint64_t ratio=std::clamp((hits+prior)*1024u/(exp+prior),uint64_t(341),uint64_t(3072));
        raw[i]=std::max<uint64_t>(1,uint64_t(c.a[i].w)*ratio);
    }
    return v6_quantize(raw,c.size);
}
V6Weights v7_weighted_mix(const std::array<V6Weights,8>& models,
                          const std::array<uint32_t,8>& alpha,unsigned n_models,
                          unsigned n_moves) {
    std::array<uint64_t,256> raw{};
    for(unsigned i=0;i<n_moves;++i)
        for(unsigned j=0;j<n_models;++j)
            if(alpha[j])raw[i]+=uint64_t(alpha[j])*models[j].weights[i]*(1048576u/models[j].total);
    return v6_quantize(raw,n_moves);
}
V7Step v7_step(const Position& p, const Move* prev,unsigned ply,V7Model mode,
               const V7Context& ctx,const V6Learn& exact,const V7Patterns& pattern) {
    V7Step s;
    Model no_model;
    s.c=candidates(p,Mode::EntropyCheckSignalLight,prev,ply,no_model);
    s.experts[0]=v6_copy(s.c);
    if(mode==V7Model::Baseline) {s.coded=s.experts[0];return s;}
    if(mode==V7Model::ExactDictionary) {
        s.coded=v6_dict(s.c,exact,V6Learn::key(p));return s;
    }
    if(mode==V7Model::GeometryPattern) {
        s.coded=v7_pattern(s.c,p,ply,pattern,true);return s;
    }
    if(mode==V7Model::SquarePattern) {
        s.coded=v7_pattern(s.c,p,ply,pattern,false);return s;
    }
    if(mode==V7Model::OpportunityGeometry) {
        s.coded=v7_opportunity(s.c,p,ply,pattern,true);return s;
    }
    if(mode==V7Model::OpportunitySquare) {
        s.coded=v7_opportunity(s.c,p,ply,pattern,false);return s;
    }
    s.experts[1]=v6_uniform(s.c.size);
    if(mode==V7Model::SelectiveCompetition) {
        s.experts[2]=v6_dict(s.c,exact,V6Learn::key(p));
        s.experts[3]=v7_opportunity(s.c,p,ply,pattern,false);
        s.experts_n=4;
        s.coded=v7_weighted_mix(s.experts,ctx.selective_alpha,4,s.c.size);
        return s;
    }
    s.experts[2]=v6_hier(s.c,ctx.base);
    s.experts[3]=v6_dict(s.c,exact,V6Learn::key(p));
    s.experts[4]=v7_pattern(s.c,p,ply,pattern,true);
    s.experts[5]=v7_pattern(s.c,p,ply,pattern,false);
    if(mode==V7Model::CombinedDictionary) {
        // Fixed blend: baseline + exact + geometry + square. Competitive version
        // updates the mixture weights after observing each decoded move.
        s.coded=v7_weighted_mix(s.experts,{16384,0,0,16384,16384,16384},6,s.c.size);
        return s;
    }
    if(mode==V7Model::OpportunityCompetition) {
        s.experts[6]=v7_opportunity(s.c,p,ply,pattern,true);
        s.experts[7]=v7_opportunity(s.c,p,ply,pattern,false);
        s.experts_n=8;
        s.coded=v7_weighted_mix(s.experts,ctx.opportunity_alpha,8,s.c.size);
    } else {
        s.experts_n=6;
        s.coded=v7_weighted_mix(s.experts,ctx.posterior,6,s.c.size);
    }
    return s;
}
void v7_advance(V7Context& ctx,const V7Step& step,unsigned index,V7Model mode) {
    if(step.experts_n) {
        std::array<uint64_t,8> weighted{};
        auto& alpha=mode==V7Model::OpportunityCompetition?ctx.opportunity_alpha:
                    mode==V7Model::SelectiveCompetition?ctx.selective_alpha:ctx.posterior;
        uint64_t total=0;
        for(unsigned i=0;i<step.experts_n;++i) {
            weighted[i]=uint64_t(alpha[i])*step.experts[i].weights[index]*
                        (1048576u/step.experts[i].total);
            total+=weighted[i];
        }
        if(!total)fail("V7 posterior zero");
        for(unsigned i=0;i<step.experts_n;++i)
            alpha[i]=std::max<uint32_t>(1,uint32_t(weighted[i]*65536u/total));
        if(mode==V7Model::SwitchingCompetition || mode==V7Model::OpportunityCompetition ||
           mode==V7Model::SelectiveCompetition) {
            // Give previously disfavoured experts a chance to recover when the
            // game enters a different phase. Approximately 1/32 mass refreshed.
            for(unsigned i=0;i<step.experts_n;++i)
                alpha[i]=(alpha[i]*31u+(mode==V7Model::OpportunityCompetition?8192u:
                                                  mode==V7Model::SelectiveCompetition?16384u:10923u))/32u;
        }
    }
    uint8_t cat=step.c.a[index].category;
    if(ctx.base.group_counts[cat]<4095)++ctx.base.group_counts[cat];
    ++ctx.base.seen;
}
}
const char* v7_name(V7Model mode) {
    switch(mode) {
        case V7Model::Baseline:return "baseline";
        case V7Model::ExactDictionary:return "exact";
        case V7Model::GeometryPattern:return "geometry";
        case V7Model::SquarePattern:return "square";
        case V7Model::CombinedDictionary:return "combined";
        case V7Model::ModelCompetition:return "competition";
        case V7Model::SwitchingCompetition:return "switching";
        case V7Model::OpportunityGeometry:return "opportunity-geometry";
        case V7Model::OpportunitySquare:return "opportunity-square";
        case V7Model::OpportunityCompetition:return "opportunity-competition";
        case V7Model::Auto:return "auto";
        case V7Model::SelectiveCompetition:return "selective-competition";
    }
    return "invalid";
}
V6Archive v7_encode(const std::vector<std::vector<Move>>& games,V7Model mode) {
    if(mode==V7Model::Auto) {
        // Archive-level model competition. One choice in the container header;
        // decoding costs only the chosen model, but encoding is multi-pass.
        V6Archive best;
        bool first=true;
        for(unsigned k=0;k<=unsigned(V7Model::SelectiveCompetition);++k) {
            if(k==unsigned(V7Model::Auto))continue;
            V6Archive candidate=v7_encode(games,V7Model(k));
            if(first || candidate.bytes.size()<best.bytes.size() ||
                 (candidate.bytes.size()==best.bytes.size() && candidate.payload_bits<best.payload_bits)) {
                best=std::move(candidate);
                first=false;
            }
        }
        // The previous DMG6 mixture can be superior on near-uniform/random moves.
        // Auto is a multi-pass *archive* selector; its output carries the selected
        // native DMG6 or DMG7 magic and is handled by v7_decode().
        for(unsigned k=0;k<=unsigned(V6Model::All);++k) {
            V6Archive candidate=v6_encode(games,V6Model(k));
            if(candidate.bytes.size()<best.bytes.size() ||
               (candidate.bytes.size()==best.bytes.size() && candidate.payload_bits<best.payload_bits))
                best=std::move(candidate);
        }
        return best;
    }
    if(unsigned(mode)>unsigned(V7Model::SelectiveCompetition))fail("unknown V7 mode");
    if(games.size()>100000)fail("too many games");
    ArithmeticEncoder ar;
    V6Learn exact;
    V7Patterns pattern;
    uint64_t total_plies=0;
    for(const auto& game:games) {
        if(game.size()>1000000)fail("V7 game too long");
        total_plies+=game.size();
        if(total_plies>300000000ULL)fail("V7 archive too long");
        Position p=Position::standard();
        V7Context ctx;
        Move previous{};bool has_prev=false;
        for(unsigned ply=0;ply<game.size();++ply) {
            Move m=game[ply];
            Position next=p;
            if(!next.apply_checked(m))fail("illegal V7 input move");
            V7Step step=v7_step(p,has_prev?&previous:nullptr,ply,mode,ctx,exact,pattern);
            unsigned idx=index_of(step.c,m);
            ar.put(v6_before(step.coded,idx),step.coded.weights[idx],step.coded.total);
            v7_advance(ctx,step,idx,mode);
            if(mode==V7Model::ExactDictionary || mode==V7Model::CombinedDictionary ||
               mode==V7Model::ModelCompetition || mode==V7Model::SwitchingCompetition ||
               mode==V7Model::OpportunityCompetition || mode==V7Model::SelectiveCompetition)
                exact.observe(V6Learn::key(p),m);
            if(mode==V7Model::GeometryPattern || mode==V7Model::SquarePattern ||
               mode==V7Model::CombinedDictionary || mode==V7Model::ModelCompetition ||
               mode==V7Model::SwitchingCompetition || mode==V7Model::OpportunityCompetition)
                pattern.observe(p,m,ply);
            if(mode==V7Model::OpportunityGeometry || mode==V7Model::OpportunitySquare ||
               mode==V7Model::OpportunityCompetition || mode==V7Model::SelectiveCompetition)
                pattern.observe_opportunity(p,step.c,idx,ply,
                    mode!=V7Model::OpportunitySquare && mode!=V7Model::SelectiveCompetition,
                    mode!=V7Model::OpportunityGeometry);
            previous=m;has_prev=true;p=std::move(next);
        }
    }
    EntropyPayload payload=total_plies?ar.finish():EntropyPayload{};
    std::vector<uint8_t> output={'D','M','G','7',1,uint8_t(mode)};
    v6_varint(output,uint32_t(games.size()));
    for(const auto& game:games)v6_varint(output,uint32_t(game.size()));
    v6_varint(output,payload.bit_count);
    uint64_t framing=output.size()+4;
    output.insert(output.end(),payload.bytes.begin(),payload.bytes.end());
    uint32_t crc=v6_crc(output.data(),output.size());
    for(unsigned i=0;i<4;++i)output.push_back(uint8_t(crc>>(8*i)));
    return {std::move(output),payload.bit_count,framing};
}
std::vector<std::vector<Move>> v7_decode(const std::vector<uint8_t>& bytes) {
    if(bytes.size()>=4&&bytes[0]=='D'&&bytes[1]=='M'&&bytes[2]=='G'&&bytes[3]=='6')return v6_decode(bytes);
    if(bytes.size()<10||bytes[0]!='D'||bytes[1]!='M'||bytes[2]!='G'||bytes[3]!='7'||bytes[4]!=1)
        fail("invalid V7 archive signature");
    if(bytes[5]>unsigned(V7Model::SelectiveCompetition) || bytes[5]==unsigned(V7Model::Auto))fail("unknown V7 model");
    V7Model mode=V7Model(bytes[5]);
    uint32_t crc=v6_crc(bytes.data(),bytes.size()-4),given=0;
    for(unsigned i=0;i<4;++i)given|=uint32_t(bytes[bytes.size()-4+i])<<(8*i);
    if(crc!=given)fail("V7 checksum mismatch");
    size_t pos=6;
    uint32_t n=v6_unvar(bytes,pos);
    if(n>100000)fail("too many games");
    std::vector<uint32_t> sizes;sizes.reserve(n);
    uint64_t total=0;
    for(unsigned i=0;i<n;++i){
        uint32_t count=v6_unvar(bytes,pos);
        if(count>1000000)fail("V7 game too long");
        total+=count;
        if(total>300000000ULL)fail("V7 archive too long");
        sizes.push_back(count);
    }
    uint32_t bits=v6_unvar(bytes,pos);
    if((bits+7ULL)/8!=bytes.size()-pos-4)fail("V7 payload size mismatch");
    if(total>0&&!bits)fail("V7 payload missing");
    ArithmeticDecoder ar(bytes.data()+pos,bits);
    V6Learn exact;
    V7Patterns pattern;
    std::vector<std::vector<Move>> result;
    result.reserve(n);
    for(uint32_t count:sizes) {
        Position p=Position::standard();V7Context ctx;
        Move prev{};bool has_prev=false;
        std::vector<Move> game;game.reserve(count);
        for(unsigned ply=0;ply<count;++ply) {
            V7Step step=v7_step(p,has_prev?&prev:nullptr,ply,mode,ctx,exact,pattern);
            uint32_t before=0;
            unsigned idx=v6_select(step.coded,step.c.size,ar.target(step.coded.total),before);
            ar.take(before,step.coded.weights[idx],step.coded.total);
            Move m=step.c.a[idx].m;
            Position next=p;
            if(!next.apply_checked(m))fail("V7 decoded move illegal");
            v7_advance(ctx,step,idx,mode);
            if(mode==V7Model::ExactDictionary || mode==V7Model::CombinedDictionary ||
               mode==V7Model::ModelCompetition || mode==V7Model::SwitchingCompetition ||
               mode==V7Model::OpportunityCompetition || mode==V7Model::SelectiveCompetition)
                exact.observe(V6Learn::key(p),m);
            if(mode==V7Model::GeometryPattern || mode==V7Model::SquarePattern ||
               mode==V7Model::CombinedDictionary || mode==V7Model::ModelCompetition ||
               mode==V7Model::SwitchingCompetition || mode==V7Model::OpportunityCompetition)
                pattern.observe(p,m,ply);
            if(mode==V7Model::OpportunityGeometry || mode==V7Model::OpportunitySquare ||
               mode==V7Model::OpportunityCompetition || mode==V7Model::SelectiveCompetition)
                pattern.observe_opportunity(p,step.c,idx,ply,
                    mode!=V7Model::OpportunitySquare && mode!=V7Model::SelectiveCompetition,
                    mode!=V7Model::OpportunityGeometry);
            p=std::move(next);
            prev=m;has_prev=true;game.push_back(m);
        }
        result.push_back(std::move(game));
    }
    return result;
}
}

// DMG8 experiments: all-legal candidates, online relation-feature log-linear
// choice modeling, A/E relation ablation, and online expert competition.
// This is an independent DMG8 bitstream, never silently modifies DMG7.
namespace rmc {
namespace {
constexpr unsigned V8_F=24;
constexpr unsigned V8_K=5;
constexpr int V8_LOG=64; // 64 log2 score ticks / bit
// LUT fixed at build time, not computed by target-platform libm.
static constexpr uint16_t v8_exp2_lut[257] = {1024,1035,1046,1058,1069,1081,1093,1105,1117,1129,1141,1154,1166,1179,1192,1205,1218,1231,1244,1258,1272,1286,1300,1314,1328,1342,1357,1372,1387,1402,1417,1433,1448,1464,1480,1496,1512,1529,1545,1562,1579,1596,1614,1631,1649,1667,1685,1704,1722,1741,1760,1779,1798,1818,1838,1858,1878,1898,1919,1940,1961,1983,2004,2026,2048,2070,2093,2116,2139,2162,2186,2209,2233,2258,2282,2307,2332,2358,2383,2409,2435,2462,2489,2516,2543,2571,2599,2627,2656,2685,2714,2744,2774,2804,2834,2865,2896,2928,2960,2992,3025,3057,3091,3124,3158,3193,3228,3263,3298,3334,3371,3407,3444,3482,3520,3558,3597,3636,3676,3716,3756,3797,3838,3880,3922,3965,4008,4052,4096,4141,4186,4231,4277,4324,4371,4419,4467,4515,4565,4614,4664,4715,4767,4819,4871,4924,4978,5032,5087,5142,5198,5255,5312,5370,5428,5487,5547,5607,5668,5730,5793,5856,5919,5984,6049,6115,6182,6249,6317,6386,6455,6526,6597,6668,6741,6814,6889,6964,7039,7116,7194,7272,7351,7431,7512,7594,7677,7760,7845,7930,8016,8104,8192,8281,8371,8463,8555,8648,8742,8837,8933,9031,9129,9228,9329,9431,9533,9637,9742,9848,9955,10064,10173,10284,10396,10509,10624,10739,10856,10975,11094,11215,11337,11460,11585,11711,11839,11968,12098,12230,12363,12498,12634,12771,12910,13051,13193,13337,13482,13629,13777,13927,14079,14232,14387,14544,14702,14862,15024,15188,15353,15520,15689,15860,16033,16208,16384};
inline unsigned v8_kind(const Position& p,int sq){return unsigned(abs_piece(p.board[sq]));}
static uint32_t v8_relations(const Position& p,const Position& after,Move m,
                             const Move* previous,const AttackCache& enemy_pre,unsigned ply,const OwnAttackReuse* reuse=nullptr) {
    const int kind=int(v8_kind(p,m.from));
    const int victim=abs_piece(p.board[m.to]);
    const bool ep=(kind==1 && (m.from&7)!=(m.to&7) && victim==0);
    const int actual_victim=ep?1:victim;
    const bool cap=actual_victim!=0;
    const bool danger_from=enemy_pre.attacked(m.from);
    // Aggregate both attack maps once rather than repeatedly scanning every piece
    // for each relation. These maps include defended occupied squares.
    // The reference path is retained for independent byte comparisons.
    const uint64_t our_attacks=reuse?reuse->after(p,after):AttackCache(after).attacked_mask;
    const bool danger_to=after.square_attacked(m.to,after.side);
    const bool supported=(our_attacks&bit(m.to))!=0;
    uint32_t flags=0;
    auto add=[&](int n,bool ok){if(ok)flags|=(1u<<n);};
    add(0,danger_from);
    add(1,danger_from && !danger_to);
    add(2,danger_to);
    add(3,supported);
    add(4,cap);
    add(5,cap && previous && m.to==previous->to);
    constexpr int val[7]={0,1,3,3,5,9,100};
    add(6,cap && val[actual_victim]>=val[kind]);
    add(7,cap && enemy_pre.attacked(m.to));
    int enemy_king=after.king_square(after.side);
    bool checking=enemy_king>=0 && (our_attacks&bit(enemy_king));
    add(8,checking);
    add(9,checking && kind!=6 && kind!=2); // allows discovered checks
    uint64_t enemy_targets=(after.piece_bits[after.side][1]|
        after.piece_bits[after.side][2]|after.piece_bits[after.side][3]|
        after.piece_bits[after.side][4]) & our_attacks;
    const int attacked_valuable=enemy_targets?((enemy_targets&(enemy_targets-1))?2:1):0;
    bool queen=(enemy_targets & after.piece_bits[after.side][4])!=0;
    bool rook=(enemy_targets & after.piece_bits[after.side][3])!=0;
    bool minor=(enemy_targets & (after.piece_bits[after.side][1]|
                                 after.piece_bits[after.side][2]))!=0;
    add(10,queen);
    add(11,rook);
    add(12,minor);
    uint64_t supported_own=our_attacks & after.occupied[p.side];
    bool own_queen=(supported_own & after.piece_bits[p.side][4])!=0;
    bool own_rook=(supported_own & after.piece_bits[p.side][3])!=0;
    bool own_minor=(supported_own & (after.piece_bits[p.side][1]|
                                    after.piece_bits[p.side][2]))!=0;
    add(13,own_queen);
    add(14,own_rook);
    add(15,own_minor);
    int dest_rank=p.side?7-int(m.to>>3):int(m.to>>3);
    int start_rank=p.side?7-int(m.from>>3):int(m.from>>3);
    add(16,kind==1 && dest_rank>=5);
    add(17,kind==1 && std::abs(int(m.to)-int(m.from))==16);
    add(18,(kind==2||kind==3) && start_rank==0 && dest_rank>0);
    add(19,kind==5 && ply<16);
    add(20,kind==6 && std::abs(int(m.to)-int(m.from))==2);
    add(21,m.promotion!=0);
    add(22,attacked_valuable>0 && (kind==3||kind==4||kind==5));
    add(23,attacked_valuable>=2);
    return flags;
}
struct V8Learn {
    std::array<int16_t,V8_F> theta{};
    std::array<int32_t,V8_F> residual{};
    std::array<uint64_t,V8_F> actual{},expected{};
    uint64_t seen=0;
    void observe(const std::array<uint32_t,256>& flags,const V6Weights& joint,
                 const V6Weights& base,unsigned n,unsigned chosen) {
        std::array<uint32_t,V8_F> pj{};
        std::array<uint32_t,V8_F> pb{};
        for(unsigned j=0;j<n;++j){
            auto f=flags[j];
            while(f){int bit=std::countr_zero(f);f&=f-1;
                pj[bit]+=joint.weights[j];pb[bit]+=base.weights[j];
            }
        }
        const ExactDivisor divide_joint(joint.total),divide_base(base.total);
        for(unsigned f=0;f<V8_F;++f){
            int32_t observed=(flags[chosen]&(1u<<f))?4096:0;
            int32_t expectation=int32_t(divide_joint(uint64_t(pj[f])*4096u+joint.total/2));
            residual[f]+=6*(observed-expectation);
            int32_t update=residual[f]/4096; // truncation toward zero
            residual[f]-=update*4096;
            theta[f]=int16_t(std::clamp(int32_t(theta[f])+update,-48,48));
            expected[f]=std::min<uint64_t>(expected[f]+divide_base(uint64_t(pb[f])*4096u+base.total/2),1000000000ULL);
            if(observed)actual[f]=std::min<uint64_t>(actual[f]+4096u,1000000000ULL);
        }
        ++seen;
        if((seen&63u)==0) for(auto& t:theta) {
            if(t>0)--t;
            if(t<0)++t;
        }
    }
};
struct V8Context {
    V8Learn relation;
    V7Patterns patterns;
    V6Learn exact;
    std::array<uint32_t,V8_K> alpha{13108,13107,13107,13107,13107};
};
struct V8Step {
#ifdef DMT_CACHE_OPPORTUNITY
    std::array<V7Patterns::Opportunity*,256> square_entries{};
#endif
    CandidateList moves;
    std::array<uint32_t,256> flags{};
    std::array<uint32_t,256> square_keys{};
    std::array<V6Weights,V8_K> experts{};
    V6Weights coded;
};
template<class Step> auto* v8_entry_cache(Step& s){
#ifdef DMT_CACHE_OPPORTUNITY
    return &s.square_entries;
#else
    (void)s;return static_cast<std::array<V7Patterns::Opportunity*,256>*>(nullptr);
#endif
}
uint64_t v09_pinned(const Position& p) {
    int king=p.king_square(p.side);if(king<0)return ~uint64_t(0);
    constexpr int d[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    uint64_t pinned=0;
    for(int dir=0;dir<8;++dir){
        int f=(king&7)+d[dir][0],r=(king>>3)+d[dir][1],block=-1;
        while(unsigned(f)<8&&unsigned(r)<8){
            int sq=r*8+f,piece=p.board[sq];
            if(piece){
                bool ours=(piece>0)==(p.side==0);
                if(block<0){if(!ours)break;block=sq;}
                else {int k=abs_piece(piece);if(!ours&&(k==5||k==(dir<4?4:3)))pinned|=bit(block);break;}
            }
            f+=d[dir][0];r+=d[dir][1];
        }
    }
    return pinned;
}
bool v09_known_pseudo(const Position& p, Move m, Position& next,uint64_t pinned,bool in_check) {
    int kind=abs_piece(p.board[m.from]);
    bool ep=kind==1&&(m.from%8!=m.to%8)&&!p.board[m.to];
    bool castle=kind==6&&std::abs(int(m.to)-int(m.from))==2;
    bool ok;
    if(castle){ok=next.apply_checked(m);}
    else if(abs_piece(p.board[m.to])==6){ok=false;}
    else {
        next.apply_unchecked(m);
        // The candidate builder already fully filtered checked positions.
        if(in_check) ok=true;
        else if(kind!=6&&!ep&&!(pinned&bit(m.from)))ok=true;
        else {int king=next.king_square(p.side);ok=king>=0&&!next.square_attacked(king,p.side^1);}
    }
#ifdef RMC_VERIFY_LEGAL_FAST
    Position oracle=p;bool expected=oracle.apply_checked(m);
    if(ok!=expected || (ok&&(V6Learn::key(oracle)!=V6Learn::key(next)||oracle.occupied!=next.occupied||oracle.piece_bits!=next.piece_bits)))fail("fast legality mismatch");
#endif
    return ok;
}
V6Weights v09_cached_opportunity(const CandidateList& c,const Position& p,unsigned ply,const V7Patterns& db,std::array<uint32_t,256>& keys,std::array<V7Patterns::Opportunity*,256>* entries=nullptr) {
    const unsigned phase=V7Patterns::phase(p,ply);std::array<uint64_t,256> raw{};
    constexpr uint64_t prior=4u*4096u;
    for(unsigned i=0;i<c.size;++i){
        Move m=c.a[i].m;unsigned kind=abs_piece(p.board[m.from]);
        unsigned from=p.side?(m.from^56u):m.from,to=p.side?(m.to^56u):m.to;
        unsigned cap=V7Patterns::capture(p,m,int(kind));
        uint32_t key=(((((phase*8u+kind)*64u+from)*64u+to)*2u+cap)*8u+m.promotion);keys[i]=key;
        const V7Patterns::Opportunity* record=nullptr;
        if(entries){
            // unordered_map references remain valid across rehash. These zero
            // entries would be inserted by observe for every candidate anyway.
            auto* entry=&db.square_op[key];(*entries)[i]=entry;record=entry;
        }else {auto it=db.square_op.find(key);if(it!=db.square_op.end())record=&it->second;}
        uint64_t hits=record?record->actual:0,exp=record?record->expected:0;
        uint64_t ratio=std::clamp((hits+prior)*1024u/(exp+prior),uint64_t(341),uint64_t(3072));
        raw[i]=std::max<uint64_t>(1,uint64_t(c.a[i].w)*ratio);
    }
    return v6_quantize(raw,c.size);
}
void v09_cached_observe(V7Patterns& db,const CandidateList& c,const std::array<uint32_t,256>& keys,unsigned ix,
                        const std::array<V7Patterns::Opportunity*,256>* entries=nullptr){
    const ExactDivisor divide(c.total);
    for(unsigned i=0;i<c.size;++i){unsigned e=unsigned(divide(uint64_t(c.a[i].w)*4096u+c.total/2));
        auto& record=entries?*(*entries)[i]:db.square_op[keys[i]];V7Patterns::saturating_add(record.expected,e);}
    auto& chosen=entries?*(*entries)[ix]:db.square_op[keys[ix]];
    V7Patterns::saturating_add(chosen.actual,4096u);
}
// Compute check delivery once for the relational-joint path. The reference
// candidate builder used it once for weights and again for relation flags.
void v8_joint_candidates(const Position& p,const Move* prev,unsigned ply,V8Step& s){
    Model dummy;auto pseudo=candidates(p,Mode::EntropyUniform,prev,ply,dummy);
    const bool in_check=p.square_attacked(p.king_square(p.side),p.side^1);
    if(in_check)remove_illegal_during_check(pseudo,p);
    const uint64_t pinned=in_check?0:v09_pinned(p);
    AttackCache pre_enemy(p);const OwnAttackReuse reuse(p);
    unsigned raw_total=0;
    for(unsigned i=0;i<pseudo.size;++i){
        auto item=pseudo.a[i];Position next=p;
        const bool legal=v09_known_pseudo(p,item.m,next,pinned,in_check);
        uint32_t flags=legal?v8_relations(p,next,item.m,prev,pre_enemy,ply,&reuse):0;
        const bool check=legal?bool(flags&(1u<<8)):direct_check_bonus(p,item.m)!=0;
        unsigned w=positional_weight(p,item.m,prev,ply);
        w=unsigned(std::max(1,int(w)+tactical_bonus(p,item.m,prev,pre_enemy)));
        w=unsigned(std::max(1,int(w)+(check?12:0)));
        raw_total+=w;
        if(legal){item.category=group(p,item.m,prev);item.w=uint16_t(w);
            s.flags[s.moves.size]=flags;s.moves.a[s.moves.size++]=item;}
    }
    if(!s.moves.size)fail("DMG8 move requested from terminal position");
    const unsigned factor=raw_total>=32768?(raw_total+16383)/16384:1;
    for(unsigned i=0;i<s.moves.size;++i){s.moves.a[i].w=uint16_t(std::max(1u,unsigned(s.moves.a[i].w)/factor));s.moves.total+=s.moves.a[i].w;}
#ifdef DMT_VERIFY_JOINT_CANDIDATES
    auto original=candidates(p,Mode::EntropyCheckSignalLight,prev,ply,dummy);unsigned count=0,total=0;
    for(unsigned i=0;i<original.size;++i){Position next=p;
        if(!v09_known_pseudo(p,original.a[i].m,next,pinned,in_check))continue;
        const auto& got=s.moves.a[count];const auto& ref=original.a[i];
        if(got.m!=ref.m||got.w!=ref.w||got.category!=ref.category||s.flags[count]!=v8_relations(p,next,ref.m,prev,pre_enemy,ply))fail("joint candidate oracle mismatch");
        total+=ref.w;++count;
    }
    if(count!=s.moves.size||total!=s.moves.total)fail("joint candidate total mismatch");
#endif
}
V8Step v8_step(const Position& p,const Move* prev,unsigned ply,
               V8Model mode,const V8Context& ctx,
               const std::array<int16_t,V8_F>* phase_delta=nullptr){
    V8Step s;
#ifndef DMT_REFERENCE_JOINT_CANDIDATES
    if(mode==V8Model::RelationalJoint)v8_joint_candidates(p,prev,ply,s);
    else {
#endif
    Model dummy;
    const bool in_check=p.square_attacked(p.king_square(p.side),p.side^1);
    const uint64_t pinned=in_check?0:v09_pinned(p);
    CandidateList pseudo=candidates(p,Mode::EntropyCheckSignalLight,prev,ply,dummy);
    // Reference mode only needs legal filtering, not expensive relation maps.
    if(mode==V8Model::LegalOpportunity){
        for(unsigned i=0;i<pseudo.size;++i){
            Position next=p;
            if(!v09_known_pseudo(p,pseudo.a[i].m,next,pinned,in_check))continue;
            s.moves.a[s.moves.size++]=pseudo.a[i];
            s.moves.total+=pseudo.a[i].w;
        }
        if(!s.moves.size)fail("DMG8 terminal position");
        s.experts[0]=v09_cached_opportunity(s.moves,p,ply,ctx.patterns,s.square_keys,v8_entry_cache(s));
        s.coded=s.experts[0];
        return s;
    }
    AttackCache pre_enemy(p);
#ifndef DMT_REFERENCE_RELATIONS
    const OwnAttackReuse own_reuse(p);
    const OwnAttackReuse* reuse=&own_reuse;
#else
    const OwnAttackReuse* reuse=nullptr;
#endif
    uint32_t running=0;
    for(unsigned i=0;i<pseudo.size;++i){
        Position next=p;
        if(!v09_known_pseudo(p,pseudo.a[i].m,next,pinned,in_check))continue;
        auto item=pseudo.a[i];
        s.moves.a[s.moves.size]=item;
        s.flags[s.moves.size]=v8_relations(p,next,item.m,prev,pre_enemy,ply,reuse);
        ++s.moves.size;
        running+=item.w;
    }
    if(!s.moves.size)fail("DMG8 move requested from a terminal position");
    s.moves.total=running;
#ifndef DMT_REFERENCE_JOINT_CANDIDATES
    }
#endif
    // A/B reference: same v0.7 opportunity model on a fully legal alphabet.
    s.experts[0]=v09_cached_opportunity(s.moves,p,ply,ctx.patterns,s.square_keys,v8_entry_cache(s));
    std::array<uint64_t,256> ae{},joint{};
    constexpr uint64_t prior=4096u*12u;
    const bool need_ae = mode != V8Model::RelationalJoint;
    const bool need_mix = mode==V8Model::ExpertMixture || mode==V8Model::ExpertMixtureReset;
    std::array<int32_t,V8_F> ae_factor{};
    if(need_ae) for(unsigned f=0;f<V8_F;++f) {
        auto ratio=std::clamp<uint64_t>(((ctx.relation.actual[f]+prior)*1024u)/(ctx.relation.expected[f]+prior),384u,2048u);
        ae_factor[f]=int32_t(ratio)-1024;
    }
    for(unsigned i=0;i<s.moves.size;++i){
        int32_t log_score=0,ae_add=0;
        uint32_t f=s.flags[i];
        while(f){unsigned ix=std::countr_zero(f);f&=f-1;
            log_score+=ctx.relation.theta[ix];
            if(phase_delta) log_score+=(*phase_delta)[ix];
            if(need_ae) ae_add+=ae_factor[ix];
        }
        log_score=std::clamp(log_score,-128,128);
        ae_add=std::clamp(ae_add/3,-768,2048);
        joint[i]=uint64_t(s.experts[0].weights[i])*v8_exp2_lut[log_score+128];
        ae[i]=uint64_t(s.experts[0].weights[i])*unsigned(1024+ae_add);
    }
    if(need_ae) s.experts[1]=v6_quantize(ae,s.moves.size);
    s.experts[2]=v6_quantize(joint,s.moves.size);
    if(need_mix) {
        s.experts[3]=v6_uniform(s.moves.size);
        s.experts[4]=v6_dict(s.moves,ctx.exact,V6Learn::key(p));
    }
    if(mode==V8Model::LegalOpportunity)s.coded=s.experts[0];
    else if(mode==V8Model::RelationalAE)s.coded=s.experts[1];
    else if(mode==V8Model::RelationalJoint)s.coded=s.experts[2];
    else {
        std::array<V6Weights,8> models{};
        std::array<uint32_t,8> alpha{};
        for(unsigned k=0;k<V8_K;++k){models[k]=s.experts[k];alpha[k]=ctx.alpha[k];}
        s.coded=v7_weighted_mix(models,alpha,V8_K,s.moves.size);
    }
    return s;
}
void v8_observe(V8Context& ctx,const Position& p,const V8Step& s,
                unsigned ix,unsigned ply,V8Model mode){
    if(mode==V8Model::LegalOpportunity){
        v09_cached_observe(ctx.patterns,s.moves,s.square_keys,ix,v8_entry_cache(s));
        return;
    }
    ctx.relation.observe(s.flags,s.experts[2],s.experts[0],s.moves.size,ix);
    if(mode==V8Model::ExpertMixture || mode==V8Model::ExpertMixtureReset){
        uint64_t norm=0;
        std::array<uint64_t,V8_K> joint{};
        for(unsigned k=0;k<V8_K;++k){
            joint[k]=uint64_t(ctx.alpha[k])*s.experts[k].weights[ix]*65536u/s.experts[k].total;
            norm+=joint[k];
        }
        if(!norm)fail("DMG8 mixture posterior zero");
        for(unsigned k=0;k<V8_K;++k){
            ctx.alpha[k]=std::max(1u,uint32_t(joint[k]*65536u/norm));
            // Share 1/64 of probability to make underweighted experts recover.
            ctx.alpha[k]=(ctx.alpha[k]*63u+13107u)/64u;
        }
    }
    v09_cached_observe(ctx.patterns,s.moves,s.square_keys,ix,v8_entry_cache(s));
    if(mode==V8Model::ExpertMixture || mode==V8Model::ExpertMixtureReset) ctx.exact.observe(V6Learn::key(p),s.moves.a[ix].m);
}
} // anonymous
const char* v8_name(V8Model m) {
    switch(m){
    case V8Model::LegalOpportunity:return "legal-opportunity";
    case V8Model::RelationalAE:return "relation-ae";
    case V8Model::RelationalJoint:return "relation-joint";
    case V8Model::ExpertMixture:return "relation-mixture";
    case V8Model::ExpertMixtureReset:return "relation-mixture-reset";
    case V8Model::Auto:return "auto";
    }
    return "invalid";
}
V6Archive v8_encode(const std::vector<std::vector<Move>>& games,V8Model mode){
    if(mode==V8Model::Auto){
        V6Archive best=v7_encode(games,V7Model::Auto);
        for(unsigned k=0;k<unsigned(V8Model::Auto);++k){
            auto candidate=v8_encode(games,V8Model(k));
            if(candidate.bytes.size()<best.bytes.size() ||
                (candidate.bytes.size()==best.bytes.size()&&candidate.payload_bits<best.payload_bits))
                best=std::move(candidate);
        }
        return best;
    }
    if(unsigned(mode)>=unsigned(V8Model::Auto))fail("unknown DMG8 encoder mode");
    if(games.size()>100000)fail("too many games");
    ArithmeticEncoder ar;
    V8Context ctx;
    uint64_t total=0;
    for(const auto& game:games){
        if(game.size()>1000000 || (total+=game.size())>300000000ULL)fail("invalid DMG8 game size");
        Position p=Position::standard();
        Move prev{};bool had=false;
        if(mode==V8Model::ExpertMixtureReset)ctx.alpha.fill(13107);
        for(unsigned ply=0;ply<game.size();++ply){
            Move m=game[ply];
            V8Step s=v8_step(p,had?&prev:nullptr,ply,mode,ctx);
            unsigned ix=index_of(s.moves,m); // rejects invalid UCI input
            ar.put(v6_before(s.coded,ix),s.coded.weights[ix],s.coded.total);
            v8_observe(ctx,p,s,ix,ply,mode);
            if(!p.apply_checked(m))fail("illegal DMG8 input move");
            prev=m;had=true;
        }
    }
    auto payload=total?ar.finish():EntropyPayload{};
    std::vector<uint8_t> out={'D','M','G','8',1,uint8_t(mode)};
    v6_varint(out,uint32_t(games.size()));
    for(const auto& g:games)v6_varint(out,uint32_t(g.size()));
    v6_varint(out,payload.bit_count);
    uint64_t framing=out.size()+4;
    out.insert(out.end(),payload.bytes.begin(),payload.bytes.end());
    uint32_t crc=v6_crc(out.data(),out.size());
    for(unsigned i=0;i<4;++i)out.push_back(uint8_t(crc>>(8*i)));
    return {std::move(out),payload.bit_count,framing};
}
std::vector<std::vector<Move>> v8_decode(const std::vector<uint8_t>& bytes){
    if(bytes.size()>=4 && bytes[0]=='D' && bytes[1]=='M' && bytes[2]=='G' &&
        (bytes[3]=='6'||bytes[3]=='7'))return v7_decode(bytes);
    if(bytes.size()<10 || bytes[0]!='D'||bytes[1]!='M'||bytes[2]!='G'||bytes[3]!='8'||bytes[4]!=1)
        fail("invalid DMG8 signature");
    if(bytes[5]>=unsigned(V8Model::Auto))fail("invalid DMG8 mode");
    V8Model mode=V8Model(bytes[5]);
    uint32_t crc=v6_crc(bytes.data(),bytes.size()-4),given=0;
    for(unsigned i=0;i<4;++i)given|=uint32_t(bytes[bytes.size()-4+i])<<(8*i);
    if(crc!=given)fail("DMG8 CRC32 mismatch");
    size_t pos=6;
    uint32_t n=v6_unvar(bytes,pos);
    if(n>100000)fail("too many DMG8 games");
    std::vector<uint32_t> lengths;lengths.reserve(n);
    uint64_t total=0;
    for(unsigned i=0;i<n;++i){
        uint32_t length=v6_unvar(bytes,pos);
        if(length>1000000 || (total+=length)>300000000ULL)fail("invalid DMG8 game size");
        lengths.push_back(length);
    }
    uint32_t bits=v6_unvar(bytes,pos);
    if((uint64_t(bits)+7u)/8u!=bytes.size()-pos-4)fail("DMG8 payload length mismatch");
    if(total && !bits)fail("missing DMG8 payload");
    ArithmeticDecoder ar(bytes.data()+pos,bits);
    V8Context ctx;
    std::vector<std::vector<Move>> games;games.reserve(n);
    for(unsigned length:lengths){
        Position p=Position::standard();Move prev{};bool had=false;
        if(mode==V8Model::ExpertMixtureReset)ctx.alpha.fill(13107);
        std::vector<Move> game;game.reserve(length);
        for(unsigned ply=0;ply<length;++ply){
            V8Step s=v8_step(p,had?&prev:nullptr,ply,mode,ctx);
            uint32_t before=0;
            unsigned ix=v6_select(s.coded,s.moves.size,ar.target(s.coded.total),before);
            ar.take(before,s.coded.weights[ix],s.coded.total);
            Move m=s.moves.a[ix].m;
            v8_observe(ctx,p,s,ix,ply,mode);
            if(!p.apply_checked(m))fail("illegal DMG8 decoded move");
            prev=m;had=true;game.push_back(m);
        }
        games.push_back(std::move(game));
    }
    return games;
}
} // rmc

// DMG9-R1 experimental bitstream. Legacy decoders and probability models are
// frozen. The new model is deliberately independent of DMG8 feature indices.
namespace rmc {
namespace {
constexpr unsigned V9_F=16, V9_SCALE=16384;
struct V9Learn {
    std::array<int16_t,V9_F> theta{};
    std::array<int32_t,V9_F> residual{};
    uint64_t seen=0;
};
struct V9Context {
    V9Learn rel;
    V7Patterns patterns;
};
struct V9Step {
    CandidateList legal;
    std::array<uint16_t,256> plus{},minus{};
    std::array<uint32_t,256> keys{};
    V6Weights coded;
};
// A count of geometrically attacked/supporting pieces in a fixed target set.
// Compare the very SAME surviving piece targets before and after a candidate.
int v9_count(uint64_t attacks,uint64_t alive,int kind,const Position& p,int color) {
    uint64_t type=kind==5?p.piece_bits[color][4]:kind==4?p.piece_bits[color][3]:
        (p.piece_bits[color][1]|p.piece_bits[color][2]);
    return std::popcount(attacks&alive&type);
}
#ifdef DMG9_INCREMENTAL_ATTACKS
// Experimental kernel: currently slower on the development corpus.
// Reuse the unmodified geometrical attack sets for every legal candidate.
// Any unchanged slider whose ray touches a modified occupancy square must be
// marked dirty. This intentionally uses a conservative superset.
struct V9SharedAttacks {
    std::array<uint64_t,64> old_piece{};
    uint64_t our_union=0;
    uint64_t current_own=0;
    explicit V9SharedAttacks(const Position& p) {
        current_own=p.occupied[p.side];
        uint64_t bits=current_own;
        while(bits){
            int sq=std::countr_zero(bits);bits&=bits-1;
            old_piece[sq]=piece_attack(p,sq);
            our_union|=old_piece[sq];
        }
    }
    static uint64_t piece_attack(const Position& p,int sq){
        const int kind=abs_piece(p.board[sq]);
        const auto& t=fast_attack_tables;
        if(kind==1)return t.pawn[p.board[sq]<0][sq];
        if(kind==2)return t.knight[sq];
        if(kind==6)return t.king[sq];
        if(kind<3||kind>5)return 0;
        int lo=kind==3?4:0,hi=kind==4?4:8;
        uint64_t result=0,occ=p.occupied[0]|p.occupied[1];
        for(int dir=lo;dir<hi;++dir){
            auto ray=t.ray[sq][dir];auto blockers=ray&occ;
            if(blockers){
                int nearest=(dir==0||dir==2||dir==4||dir==6)?
                    std::countr_zero(blockers):63-std::countl_zero(blockers);
                ray &= ~t.ray[nearest][dir];
            }
            result|=ray;
        }
        return result;
    }
    static bool affected_slider(int sq,int kind,uint64_t changed){
        int f=sq&7,r=sq>>3;
        while(changed){
            int to=std::countr_zero(changed);changed&=changed-1;
            int df=std::abs(f-(to&7)),dr=std::abs(r-(to>>3));
            if((kind==4||kind==5)&&(df==0||dr==0))return true;
            if((kind==3||kind==5)&&df==dr)return true;
        }
        return false;
    }
    uint64_t after_union(const Position& p,const Position& after,Move m) const {
        uint64_t changed=bit(m.from)|bit(m.to);
        int kind=abs_piece(p.board[m.from]);
        if(kind==1&&(m.from&7)!=(m.to&7)&&!p.board[m.to]){
            int pawn_square=p.side?m.to+8:m.to-8;
            changed|=bit(pawn_square);
        }
        if(kind==6&&std::abs(int(m.to)-int(m.from))==2){
            int rank=m.from>>3;
            if(m.to>m.from)changed|=bit(rank*8+7)|bit(rank*8+5);
            else changed|=bit(rank*8)|bit(rank*8+3);
        }
        uint64_t new_union=0;
        uint64_t bits=current_own;
        while(bits){
            int sq=std::countr_zero(bits);bits&=bits-1;
            int type=abs_piece(p.board[sq]);
            if(sq==m.from||
               ((type==3||type==4||type==5)&&affected_slider(sq,type,changed))){
                if(after.board[sq]&&((after.board[sq]>0)==(p.side==0)))
                    new_union |= piece_attack(after,sq);
            } else new_union |= old_piece[sq];
        }
        // New square occupied by moved piece; also castled rook's destination.
        new_union |= piece_attack(after,m.to);
        if(kind==6&&std::abs(int(m.to)-int(m.from))==2){
            int rank=m.from>>3;
            new_union |= piece_attack(after,rank*8+(m.to>m.from?5:3));
        }
#ifdef RMC_VERIFY_ATTACKS
        AttackCache oracle(after);
        if(new_union!=oracle.attacked_mask)fail("DMG9 incremental attack union mismatch");
#endif
        return new_union;
    }
};
#endif // DMG9_INCREMENTAL_ATTACKS
void v9_feature(const Position& p,const Position& after,Move m,
                const Move* prev,const Move* prev_own,
                uint64_t our_before,uint64_t our_after,uint64_t enemy_before,
                uint16_t& plus,uint16_t& minus) {
    plus=0;minus=0;
    auto b=[&](unsigned f,bool v){if(v)plus|=uint16_t(1u<<f);};
    const unsigned kind=unsigned(abs_piece(p.board[m.from]));
    const unsigned victim=unsigned(abs_piece(p.board[m.to]));
    const bool ep=kind==1&&((m.from&7)!=(m.to&7))&&!victim;
    const bool cap=victim||ep;
    unsigned actual_victim=ep?1:victim;
    b(0,(enemy_before&bit(m.from))!=0);
    b(1,after.square_attacked(m.to,after.side));
    b(2,(our_after&bit(m.to))!=0);
    b(3,cap);
    b(4,cap&&prev&&m.to==prev->to);
    static constexpr unsigned val[7]={0,1,3,3,5,9,100};
    b(5,cap&&val[actual_victim]>=val[kind]);
    const int other_king=after.king_square(after.side);
    b(6,other_king>=0&&(our_after&bit(other_king))!=0);
    // A captured target must not be misinterpreted as a lost threat.
    const uint64_t surviving_enemies=after.occupied[after.side];
    const uint64_t surviving_own=after.occupied[p.side];
    for(int group=0;group<3;++group) {
        unsigned k=group==0?5:group==1?4:3;
        int before=v9_count(our_before,surviving_enemies,k,p,after.side);
        int after_n=v9_count(our_after,surviving_enemies,k,after,after.side);
        if(after_n>before)plus|=uint16_t(1u<<(7+group));
        if(after_n<before)minus|=uint16_t(1u<<(7+group));
        before=v9_count(our_before,surviving_own,k,p,p.side);
        after_n=v9_count(our_after,surviving_own,k,after,p.side);
        if(after_n>before)plus|=uint16_t(1u<<(10+group));
        if(after_n<before)minus|=uint16_t(1u<<(10+group));
    }
    b(13,m.promotion!=0);
    b(14,kind==6&&std::abs(int(m.to)-int(m.from))==2);
    // Cheap same-piece continuation proxy; no persistent IDs are encoded.
    b(15,prev_own&&m.from==prev_own->to);
}
// Fixed 16,384 positive-frequency CDF. Largest-remainder tie breaking in
// canonical move order; all arithmetic is exact integer arithmetic.
V6Weights v9_quantize(const std::array<uint64_t,256>& raw,unsigned n){
    if(!n||n>=V9_SCALE)fail("invalid DMG9 alphabet");
    uint64_t sum=0;for(unsigned i=0;i<n;++i)sum+=raw[i];
    if(!sum)fail("DMG9 zero probability mass");
    V6Weights out;
    std::array<uint64_t,256> remainder{};
    uint32_t assigned=0;
    for(unsigned i=0;i<n;++i){
        // Bounding every raw score is part of the DMG9 profile.
        if(raw[i]>(UINT64_MAX/(V9_SCALE-n)))fail("DMG9 raw mass overflow");
        uint64_t mult=raw[i]*(V9_SCALE-n);
        unsigned q=unsigned(mult/sum);
        remainder[i]=mult%sum;
        out.weights[i]=uint16_t(q+1);
        assigned+=q+1;
    }
    if(assigned>V9_SCALE)fail("DMG9 CDF overflow");
    unsigned left=V9_SCALE-assigned;
    while(left--){
        unsigned best=0;for(unsigned i=1;i<n;++i)
            if(remainder[i]>remainder[best])best=i;
        ++out.weights[best];remainder[best]=0;
    }
    out.total=V9_SCALE;
    return out;
}
std::array<uint64_t,256> v9_pattern_raw(const CandidateList& c,const Position& p,
                                unsigned ply,const V7Patterns& db,
                                std::array<uint32_t,256>& keys,bool use_pattern){
    const unsigned phase=V7Patterns::phase(p,ply);
    std::array<uint64_t,256> result{};
    constexpr uint64_t prior=4u*4096u;
    for(unsigned i=0;i<c.size;++i){
        Move m=c.a[i].m;unsigned kind=abs_piece(p.board[m.from]);
        unsigned from=p.side?(m.from^56u):m.from,to=p.side?(m.to^56u):m.to;
        unsigned cap=V7Patterns::capture(p,m,int(kind));
        uint32_t key=(((((phase*8u+kind)*64u+from)*64u+to)*2u+cap)*8u+m.promotion);
        keys[i]=key;
        uint64_t ratio=1024;
        if(use_pattern){
            auto it=db.square_op.find(key);
            uint64_t hits=it==db.square_op.end()?0:it->second.actual;
            uint64_t exp=it==db.square_op.end()?0:it->second.expected;
            ratio=std::clamp((hits+prior)*1024u/(exp+prior),uint64_t(341),uint64_t(3072));
        }
        result[i]=uint64_t(c.a[i].w)*ratio;
    }
    return result;
}
V9Step v9_step(const Position& p,const Move* prev,const Move* prev_own,
               unsigned ply,const V9Context& ctx,V9Model mode){
    V9Step s;
    Model dummy;
    CandidateList pseudo=candidates(p,Mode::EntropyCheckSignalLight,prev,ply,dummy);
    const bool in_check=p.square_attacked(p.king_square(p.side),p.side^1);
    const uint64_t pinned=in_check?0:v09_pinned(p);
    const uint64_t enemy_before=AttackCache(p).attacked_mask;
#ifdef DMG9_INCREMENTAL_ATTACKS
    V9SharedAttacks shared(p);
    const uint64_t our_before=shared.our_union;
#else
    Position reversed=p;reversed.side^=1;
    const uint64_t our_before=AttackCache(reversed).attacked_mask;
#endif
    for(unsigned i=0;i<pseudo.size;++i){
        Position after=p;
        if(!v09_known_pseudo(p,pseudo.a[i].m,after,pinned,in_check))continue;
        unsigned dst=s.legal.size++;
        s.legal.a[dst]=pseudo.a[i];
        s.legal.total+=pseudo.a[i].w;
#ifdef DMG9_INCREMENTAL_ATTACKS
        const uint64_t our_after=shared.after_union(p,after,pseudo.a[i].m);
#else
        const uint64_t our_after=AttackCache(after).attacked_mask;
#endif
        v9_feature(p,after,pseudo.a[i].m,prev,prev_own,our_before,our_after,enemy_before,
                   s.plus[dst],s.minus[dst]);
    }
    if(!s.legal.size)fail("DMG9 terminal position");
    auto prior=v9_pattern_raw(s.legal,p,ply,ctx.patterns,s.keys,mode==V9Model::DeltaOpportunity);
    std::array<int,256> score{};int lowest=std::numeric_limits<int>::max();
    for(unsigned i=0;i<s.legal.size;++i){
        int v=0;uint16_t mask=s.plus[i];
        while(mask){int f=std::countr_zero(mask);mask&=mask-1;v+=ctx.rel.theta[f];}
        mask=s.minus[i];
        while(mask){int f=std::countr_zero(mask);mask&=mask-1;v-=ctx.rel.theta[f];}
        score[i]=v;lowest=std::min(lowest,v);
    }
    std::array<uint64_t,256> raw{};
    for(unsigned i=0;i<s.legal.size;++i){
        unsigned d=unsigned(score[i]-lowest);
        if(d>1024)fail("DMG9 score bound exceeded");
        // 64 ticks per bit, LUT 1024..2048 for fractional 2^(d/64).
        raw[i]=prior[i]*v8_exp2_lut[d%64] * (uint64_t(1)<<(d/64));
    }
    s.coded=v9_quantize(raw,s.legal.size);
    return s;
}
void v9_observe(V9Context& ctx,const V9Step& s,unsigned ix,V9Model mode){
    std::array<int32_t,V9_F> masses{};
    for(unsigned i=0;i<s.legal.size;++i){
        auto p=s.plus[i],n=s.minus[i];int w=int(s.coded.weights[i]);
        while(p){int f=std::countr_zero(p);p&=p-1;masses[f]+=w;}
        while(n){int f=std::countr_zero(n);n&=n-1;masses[f]-=w;}
    }
    auto round_signed=[](int64_t n,int64_t den)->int32_t{
        return int32_t(n>=0?(n+den/2)/den:-((-n+den/2)/den));
    };
    for(unsigned f=0;f<V9_F;++f){
        int chosen=int((s.plus[ix]>>f)&1)-int((s.minus[ix]>>f)&1);
        int32_t expected=round_signed(int64_t(masses[f])*4096,V9_SCALE);
        ctx.rel.residual[f]+=6*(chosen*4096-expected);
        int32_t u=ctx.rel.residual[f]/4096;
        ctx.rel.residual[f]-=u*4096;
        ctx.rel.theta[f]=int16_t(std::clamp(int(ctx.rel.theta[f])+int(u),-32,32));
    }
    ++ctx.rel.seen;
    if((ctx.rel.seen&63u)==0)for(auto& theta:ctx.rel.theta){
        if(theta>0)--theta;else if(theta<0)++theta;
    }
    if(mode==V9Model::DeltaOpportunity)
        v09_cached_observe(ctx.patterns,s.legal,s.keys,ix);
}
} // anonymous
const char* v9_name(V9Model model){
    switch(model){
        case V9Model::DeltaOpportunity:return "delta-opportunity";
        case V9Model::DeltaNoPattern:return "delta-no-pattern";
        case V9Model::Auto:return "auto";
    }
    return "unknown";
}
V6Archive v9_encode(const std::vector<std::vector<Move>>& games,V9Model mode){
    if(mode==V9Model::Auto){
        V6Archive best=v8_encode(games,V8Model::Auto);
        for(auto m:{V9Model::DeltaOpportunity,V9Model::DeltaNoPattern}){
            auto a=v9_encode(games,m);
            if(a.bytes.size()<best.bytes.size() ||
               (a.bytes.size()==best.bytes.size()&&a.payload_bits<best.payload_bits))best=std::move(a);
        }
        return best;
    }
    if(unsigned(mode)>=unsigned(V9Model::Auto))fail("invalid DMG9 model");
    if(games.size()>100000)fail("DMG9 too many games");
    ArithmeticEncoder ar;
    V9Context ctx;
    uint64_t total=0;
    for(const auto& game:games){
        if(game.size()>1000000||(total+=game.size())>300000000ULL)fail("DMG9 game length invalid");
        Position p=Position::standard();Move prev{},last_own[2]{};
        bool has_prev=false,has_own[2]={false,false};
        for(unsigned ply=0;ply<game.size();++ply){
            Move m=game[ply];unsigned color=p.side;
            V9Step s=v9_step(p,has_prev?&prev:nullptr,has_own[color]?&last_own[color]:nullptr,ply,ctx,mode);
            unsigned ix=index_of(s.legal,m);
            ar.put(v6_before(s.coded,ix),s.coded.weights[ix],s.coded.total);
            v9_observe(ctx,s,ix,mode);
            if(!p.apply_checked(m))fail("DMG9 illegal input");
            last_own[color]=m;has_own[color]=true;prev=m;has_prev=true;
        }
    }
    auto payload=total?ar.finish():EntropyPayload{};
    std::vector<uint8_t> out={'D','M','G','9',1,uint8_t(mode)};
    v6_varint(out,uint32_t(games.size()));
    for(const auto& g:games)v6_varint(out,uint32_t(g.size()));
    v6_varint(out,payload.bit_count);
    uint64_t framing=out.size()+4;
    out.insert(out.end(),payload.bytes.begin(),payload.bytes.end());
    uint32_t crc=v6_crc(out.data(),out.size());
    for(unsigned i=0;i<4;++i)out.push_back(uint8_t(crc>>(8*i)));
    return {std::move(out),payload.bit_count,framing};
}
std::vector<std::vector<Move>> v9_decode(const std::vector<uint8_t>& bytes){
    if(bytes.size()>=4 && bytes[0]=='D'&&bytes[1]=='M'&&bytes[2]=='G' &&
       bytes[3]>='6'&&bytes[3]<='8')return v8_decode(bytes);
    if(bytes.size()<10 || bytes[0]!='D'||bytes[1]!='M'||bytes[2]!='G'||bytes[3]!='9'||bytes[4]!=1)
        fail("invalid DMG9 signature");
    if(bytes[5]>=unsigned(V9Model::Auto))fail("invalid DMG9 profile");
    V9Model mode=V9Model(bytes[5]);
    uint32_t crc=v6_crc(bytes.data(),bytes.size()-4),given=0;
    for(unsigned i=0;i<4;++i)given|=uint32_t(bytes[bytes.size()-4+i])<<(8*i);
    if(crc!=given)fail("DMG9 checksum mismatch");
    size_t pos=6;uint32_t count=v6_unvar(bytes,pos);
    if(count>100000)fail("DMG9 too many games");
    std::vector<uint32_t> lengths;lengths.reserve(count);
    uint64_t total=0;
    for(unsigned i=0;i<count;++i){
        auto n=v6_unvar(bytes,pos);
        if(n>1000000||(total+=n)>300000000ULL)fail("DMG9 invalid game length");
        lengths.push_back(n);
    }
    uint32_t bits=v6_unvar(bytes,pos);
    if((uint64_t(bits)+7u)/8u!=bytes.size()-pos-4)fail("DMG9 invalid payload length");
    if(total&&!bits)fail("DMG9 missing payload");
    ArithmeticDecoder ar(bytes.data()+pos,bits);
    V9Context ctx;
    std::vector<std::vector<Move>> games;games.reserve(count);
    for(uint32_t length:lengths){
        Position p=Position::standard();Move prev{},last_own[2]{};
        bool has_prev=false,has_own[2]={false,false};
        std::vector<Move> game;game.reserve(length);
        for(unsigned ply=0;ply<length;++ply){
            unsigned color=p.side;
            V9Step s=v9_step(p,has_prev?&prev:nullptr,has_own[color]?&last_own[color]:nullptr,ply,ctx,mode);
            uint32_t before=0;
            unsigned ix=v6_select(s.coded,s.legal.size,ar.target(s.coded.total),before);
            ar.take(before,s.coded.weights[ix],s.coded.total);
            Move m=s.legal.a[ix].m;
            v9_observe(ctx,s,ix,mode);
            if(!p.apply_checked(m))fail("DMG9 illegal decoded move");
            last_own[color]=m;has_own[color]=true;prev=m;has_prev=true;
            game.push_back(m);
        }
        games.push_back(std::move(game));
    }
    return games;
}
} // namespace rmc



// Phase-specific optional residuals on top of the frozen DMG8-Fast joint.
// DMP1 is a distinct format; DMG8 output and decode semantics do not change.
namespace rmc {
namespace {
struct PhaseDelta {
    std::array<std::array<int16_t,V8_F>,3> theta{};
    std::array<std::array<int32_t,V8_F>,3> residual{};
    std::array<uint64_t,3> seen{};
    void observe(unsigned stage,const V8Step& step,unsigned selected,unsigned rate) {
        if(!rate)return;
        std::array<uint32_t,V8_F> mass{};
        for(unsigned j=0;j<step.moves.size;++j){
            uint32_t flag=step.flags[j];
            while(flag){unsigned f=std::countr_zero(flag);flag&=flag-1;
                mass[f]+=step.coded.weights[j];
            }
        }
        for(unsigned f=0;f<V8_F;++f){
            int32_t selected_q12=(step.flags[selected]&(1u<<f))?4096:0;
            int32_t expected_q12=int32_t((uint64_t(mass[f])*4096u+step.coded.total/2)/step.coded.total);
            residual[stage][f]+=int32_t(rate)*(selected_q12-expected_q12);
            int32_t update=residual[stage][f]/4096;
            residual[stage][f]-=update*4096;
            theta[stage][f]=int16_t(std::clamp(int32_t(theta[stage][f])+update,-24,24));
        }
        ++seen[stage];
        if(seen[stage]%64==0)for(auto& v:theta[stage]){
            if(v>0)--v;
            if(v<0)++v;
        }
    }
};
unsigned phase_rate(PhaseProfile m){
    switch(m){
        case PhaseProfile::Shared: return 0;
        case PhaseProfile::Residual1: return 1;
        case PhaseProfile::Residual2: return 2;
        case PhaseProfile::Residual4: return 4;
        case PhaseProfile::Residual6: return 6;
        case PhaseProfile::EndgameOnly: return 6;
        case PhaseProfile::OpeningMixtureEndgame: return 6;
    }
    fail("invalid DMP1 profile");
}
void phase_record(PhaseDiagnostics* out,unsigned stage,const V8Step& frame,unsigned ix){
    if(!out)return;
    auto& row=out->stage[stage];
    ++row.plies;
    row.ideal_bits+=std::log2(double(frame.coded.total)/double(frame.coded.weights[ix]));
    if(uint64_t(frame.coded.weights[ix])*64u<frame.coded.total){
        ++row.long_tail_64;
        row.long_tail_bits+=std::log2(double(frame.coded.total)/double(frame.coded.weights[ix]));
    }
    if(uint64_t(frame.coded.weights[ix])*256u<frame.coded.total)++row.long_tail_256;
    unsigned top=0;
    for(unsigned k=1;k<frame.moves.size;++k)
        if(frame.coded.weights[k]>frame.coded.weights[top])top=k;
    if(ix==top)++row.top1;
}
} // anonymous
const char* phase_name(PhaseProfile profile){
    switch(profile){
        case PhaseProfile::Shared:return "shared";
        case PhaseProfile::Residual1:return "residual-1";
        case PhaseProfile::Residual2:return "residual-2";
        case PhaseProfile::Residual4:return "residual-4";
        case PhaseProfile::Residual6:return "residual-6";
        case PhaseProfile::EndgameOnly:return "endgame-only";
        case PhaseProfile::OpeningMixtureEndgame:return "opening-mixture-endgame";
    }
    return "invalid";
}
V6Archive phase_encode(const std::vector<std::vector<Move>>& games,PhaseProfile profile,PhaseDiagnostics* out){
    if(unsigned(profile)>unsigned(PhaseProfile::OpeningMixtureEndgame))fail("invalid DMP1 profile");
    if(games.size()>100000)fail("too many DMP1 games");
    if(out)*out={};
    uint64_t total=0;
    ArithmeticEncoder ar;
    V8Context ctx;
    PhaseDelta delta;
    unsigned rate=phase_rate(profile);
    for(const auto& game:games){
        if(game.size()>1000000||(total+=game.size())>300000000ULL)fail("invalid DMP1 game size");
        Position pos=Position::standard(); Move prev{};bool had=false;
        for(unsigned ply=0;ply<game.size();++ply){
            unsigned stage=V7Patterns::phase(pos,ply);
            const bool hybrid=profile==PhaseProfile::OpeningMixtureEndgame;
            const bool end_only=(hybrid||profile==PhaseProfile::EndgameOnly);
            const unsigned local_rate=(end_only&&stage!=2)?0:rate;
            const auto* residual=local_rate?&delta.theta[stage]:nullptr;
            const auto mode=(hybrid&&stage==0)?V8Model::ExpertMixture:V8Model::RelationalJoint;
            auto frame=v8_step(pos,had?&prev:nullptr,ply,mode,ctx,residual);
            unsigned index=index_of(frame.moves,game[ply]);
            phase_record(out,stage,frame,index);
            ar.put(v6_before(frame.coded,index),frame.coded.weights[index],frame.coded.total);
            v8_observe(ctx,pos,frame,index,ply,mode);
            delta.observe(stage,frame,index,local_rate);
            if(!pos.apply_checked(game[ply]))fail("illegal DMP1 move");
            prev=game[ply];had=true;
        }
    }
    auto payload=total?ar.finish():EntropyPayload{};
    std::vector<uint8_t> bytes={'D','M','P','1',1,uint8_t(profile)};
    v6_varint(bytes,uint32_t(games.size()));
    for(const auto& g:games)v6_varint(bytes,uint32_t(g.size()));
    v6_varint(bytes,payload.bit_count);
    uint64_t framing=bytes.size()+4;
    bytes.insert(bytes.end(),payload.bytes.begin(),payload.bytes.end());
    uint32_t crc=v6_crc(bytes.data(),bytes.size());
    for(unsigned j=0;j<4;++j)bytes.push_back(uint8_t(crc>>(8*j)));
    return {std::move(bytes),payload.bit_count,framing};
}
std::vector<std::vector<Move>> phase_decode(const std::vector<uint8_t>& bytes){
    if(bytes.size()<10 || bytes[0]!='D'||bytes[1]!='M'||bytes[2]!='P'||bytes[3]!='1'||bytes[4]!=1||
       bytes[5]>unsigned(PhaseProfile::OpeningMixtureEndgame))fail("invalid DMP1 header/profile");
    PhaseProfile profile=PhaseProfile(bytes[5]);
    uint32_t crc=v6_crc(bytes.data(),bytes.size()-4),given=0;
    for(unsigned k=0;k<4;++k)given|=uint32_t(bytes[bytes.size()-4+k])<<(8*k);
    if(crc!=given)fail("DMP1 CRC32 mismatch");
    size_t index=6;
    uint32_t n=v6_unvar(bytes,index);
    if(n>100000)fail("invalid DMP1 game count");
    std::vector<uint32_t> lengths;lengths.reserve(n);
    uint64_t total=0;
    for(unsigned k=0;k<n;++k){
        uint32_t len=v6_unvar(bytes,index);
        if(len>1000000||(total+=len)>300000000ULL)fail("invalid DMP1 game length");
        lengths.push_back(len);
    }
    uint32_t bits=v6_unvar(bytes,index);
    if((uint64_t(bits)+7u)/8u!=bytes.size()-index-4)fail("invalid DMP1 payload length");
    if(total&&!bits)fail("missing DMP1 payload");
    ArithmeticDecoder ar(bytes.data()+index,bits);
    V8Context ctx;
    PhaseDelta delta;
    unsigned rate=phase_rate(profile);
    std::vector<std::vector<Move>> games;games.reserve(n);
    for(unsigned len:lengths){
        Position pos=Position::standard();Move prev{};bool had=false;
        std::vector<Move> game;game.reserve(len);
        for(unsigned ply=0;ply<len;++ply){
            unsigned stage=V7Patterns::phase(pos,ply);
            const bool hybrid=profile==PhaseProfile::OpeningMixtureEndgame;
            const bool end_only=(hybrid||profile==PhaseProfile::EndgameOnly);
            const unsigned local_rate=(end_only&&stage!=2)?0:rate;
            const auto* residual=local_rate?&delta.theta[stage]:nullptr;
            const auto mode=(hybrid&&stage==0)?V8Model::ExpertMixture:V8Model::RelationalJoint;
            auto frame=v8_step(pos,had?&prev:nullptr,ply,mode,ctx,residual);
            uint32_t before=0;
            unsigned ix=v6_select(frame.coded,frame.moves.size,ar.target(frame.coded.total),before);
            ar.take(before,frame.coded.weights[ix],frame.coded.total);
            Move move=frame.moves.a[ix].m;
            v8_observe(ctx,pos,frame,ix,ply,mode);
            delta.observe(stage,frame,ix,local_rate);
            if(!pos.apply_checked(move))fail("illegal DMP1 decoded move");
            prev=move;had=true;game.push_back(move);
        }
        games.push_back(std::move(game));
    }
    return games;
}
PhaseDiagnostics phase_v8_diagnostics(const std::vector<std::vector<Move>>& games,V8Model mode){
    if(mode==V8Model::Auto)fail("diagnostics requires a concrete model");
    PhaseDiagnostics output{};
    V8Context ctx;
    for(const auto& game:games){
        Position p=Position::standard(); Move prev{};bool had=false;
        for(unsigned ply=0;ply<game.size();++ply){
            unsigned stage=V7Patterns::phase(p,ply);
            auto frame=v8_step(p,had?&prev:nullptr,ply,mode,ctx);
            unsigned ix=index_of(frame.moves,game[ply]);
            phase_record(&output,stage,frame,ix);
            v8_observe(ctx,p,frame,ix,ply,mode);
            if(!p.apply_checked(game[ply]))fail("illegal move in phase diagnostics");
            prev=game[ply];had=true;
        }
    }
    return output;
}
PhaseDiagnostics phase_baseline_diagnostics(const std::vector<std::vector<Move>>& games){
    return phase_v8_diagnostics(games,V8Model::RelationalJoint);
}
} // rmc

// DMT1: independent midgame model ablations. Frozen DMG8/DMG9/DMP1 remain unchanged.
namespace rmc {
namespace {
constexpr unsigned MID_F = 10;
constexpr unsigned MID_V1_COUNT=17;
constexpr unsigned MID_V2_COUNT=33;
inline bool mid_r3_profile(MidProfile profile){return unsigned(profile)>=33 && unsigned(profile)<45;}
inline bool mid_r4_profile(MidProfile profile){return unsigned(profile)>=45 && unsigned(profile)<54;}
inline bool mid_r5_profile(MidProfile profile){return unsigned(profile)>=54 && unsigned(profile)<68;}
inline bool mid_r6_profile(MidProfile profile){return unsigned(profile)>=68 && unsigned(profile)<80;}
inline bool mid_r7_profile(MidProfile profile){return unsigned(profile)>=80 && unsigned(profile)<98;}
inline bool mid_r8_profile(MidProfile profile){return unsigned(profile)>=98 && unsigned(profile)<125;}
inline bool mid_r9_profile(MidProfile profile){return unsigned(profile)>=125 && unsigned(profile)<189;}
inline bool mid_r10_profile(MidProfile profile){return unsigned(profile)>=189 && unsigned(profile)<243;}
// DMT7 builds on the EXACT DMT6 profile 75 pipeline, leaving every old profile untouched.
inline MidProfile r7_base(MidProfile profile){return (mid_r7_profile(profile)||mid_r8_profile(profile)||mid_r9_profile(profile))?MidProfile::R6QueenPawnQuietRate4:profile;}
inline bool r7_quiet(MidProfile p){return p==MidProfile::R7IntentQuiet||p==MidProfile::R7ContextQuiet;}
inline bool r7_context(MidProfile p){return p==MidProfile::R7Context||p==MidProfile::R7ContextQuiet||
    p==MidProfile::R7AdaptiveContext||p==MidProfile::R7ContextStrong;}
inline bool r7_adaptive(MidProfile p){return p==MidProfile::R7Adaptive||p==MidProfile::R7AdaptiveContext;}

inline bool mid_r3_open_position(MidProfile profile){return profile==MidProfile::R3OpeningPosition || profile==MidProfile::R3Both || profile==MidProfile::R3OpeningPositionWeak || profile==MidProfile::R3BothRate4 || unsigned(profile)>=40;}
inline bool mid_r3_plan(MidProfile profile){return mid_r4_profile(profile) || mid_r5_profile(profile) || mid_r6_profile(profile) || profile==MidProfile::R3MiddlegamePlan || profile==MidProfile::R3Both || profile==MidProfile::R3PlanRate4 || profile==MidProfile::R3BothRate4 || profile==MidProfile::R3BothStrong || profile==MidProfile::R3BothStrongRate4 || profile==MidProfile::R3BothAggressive;}
inline bool mid_new_profile(MidProfile profile){return unsigned(profile)>=MID_V1_COUNT;}
inline bool mid_opening_enabled(MidProfile profile){return (unsigned(profile)>=24 && unsigned(profile)<=29) || unsigned(profile)>=31;}
inline bool mid_mid_enabled(MidProfile profile){return !mid_opening_enabled(profile) || unsigned(profile)>=26;}
inline bool mid_guard_open(MidProfile profile){return profile==MidProfile::OpeningGuard||profile==MidProfile::OpeningMidGuard||profile==MidProfile::OpeningMidGuardRate3||profile==MidProfile::OpeningMidGuardRate4||unsigned(profile)>=31;}
inline unsigned mid_guard_percent(MidProfile profile){
    if(profile==MidProfile::AllGuard2)return 2;
    if(profile==MidProfile::AllGuard10)return 10;
    return 5;
}
inline unsigned mid_fixed_rate(MidProfile profile){
    if(profile==MidProfile::AllGuardRate1)return 1;
    if(profile==MidProfile::AllGuardRate3||profile==MidProfile::OpeningMidGuardRate3)return 3;
    if(profile==MidProfile::AllGuardRate4||profile==MidProfile::OpeningMidGuardRate4||profile==MidProfile::OpeningMidSEEValueRate4||mid_r3_profile(profile)||mid_r4_profile(profile)||mid_r5_profile(profile)||mid_r6_profile(profile))return 4;
    if(profile==MidProfile::AllGuardRate6)return 6;
    return 0;
}

constexpr int MID_VAL[7] = {0,100,320,330,500,900,20000};
uint64_t mid_isqrt(uint64_t x){
    uint64_t r=0,bit=uint64_t(1)<<62;
    while(bit>x)bit>>=2;
    while(bit){if(x>=r+bit){x-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}
    return r;
}
V6Weights mid_temper(const V6Weights& base,unsigned n,MidProfile profile){
    std::array<uint64_t,256> raw{};
    if(profile==MidProfile::FlatMild){
        for(unsigned i=0;i<n;++i){uint64_t w=base.weights[i];raw[i]=mid_isqrt(w*mid_isqrt(w<<20));}
    }else if(profile==MidProfile::FlatStrong){
        for(unsigned i=0;i<n;++i)raw[i]=mid_isqrt(uint64_t(base.weights[i])<<20);
    }else if(profile==MidProfile::Sharp){
        for(unsigned i=0;i<n;++i){uint64_t w=base.weights[i];raw[i]=w*mid_isqrt(mid_isqrt(w<<20));}
    }else fail("unknown temperature model");
    return v6_quantize(raw,n);
}
V6Weights mid_guard(const V6Weights& base,unsigned n,unsigned percent){
    std::array<uint64_t,256> raw{};
    for(unsigned i=0;i<n;++i)raw[i]=uint64_t(base.weights[i])*(100-percent)*n+uint64_t(percent)*base.total;
    return v6_quantize(raw,n);
}
int mid_piece_value(int piece){return MID_VAL[abs_piece(piece)];}

// DMT1 byte-preserving acceleration: these queries must match the original
// pseudo_destinations() target test.  They do not reject pinned attackers:
// legal-capture testing remains in the bounded exchange routine.
#ifdef DMT_FAST_TACTICAL
uint64_t mid_target_attackers(const Position& board,int target){
    const int side=board.side;
    const auto& t=fast_attack_tables;
    uint64_t attackers=(t.pawn[side^1][target]&board.piece_bits[side][0])|
        (t.knight[target]&board.piece_bits[side][1])|
        (t.king[target]&board.piece_bits[side][5]);
    uint64_t occupied=board.occupied[0]|board.occupied[1];
    for(int direction=0;direction<8;++direction){
        uint64_t blockers=t.ray[target][direction]&occupied;
        if(!blockers)continue;
        int nearest=(direction==0||direction==2||direction==4||direction==6)?
            std::countr_zero(blockers):63-std::countl_zero(blockers);
        uint64_t sqbit=bit(nearest);
        if(!(board.occupied[side]&sqbit))continue;
        unsigned kind=abs_piece(board.board[nearest]);
        if(kind==5 || (direction<4 ? kind==4:kind==3))attackers|=sqbit;
    }
#ifdef DMT_VERIFY_FAST
    uint64_t slow=0,actors=board.occupied[side];
    while(actors){int from=std::countr_zero(actors);actors&=actors-1;
        if(board.pseudo_destinations(from)&bit(target))slow|=bit(from);}
    if(attackers!=slow)fail("DMT1 optimized SEE attackers do not match oracle");
#endif
    return attackers;
}
// The additional DMT1 signals use the moved piece's attacks ONLY where they
// intersect opponent-occupied targets. Pawn pushes, castling, and defended own
// pieces are therefore irrelevant. Compute occupancy after a legal candidate
// without copying/applying the full Position for every candidate.
uint64_t mid_landing_capture_targets(const Position& p,Move m){
    const auto& t=fast_attack_tables;
    const int side=p.side;
    int kind=abs_piece(p.board[m.from]);
    if(m.promotion)kind=m.promotion==1?5:m.promotion==2?4:m.promotion==3?3:2;
    uint64_t occ=(p.occupied[0]|p.occupied[1])&~bit(m.from);
    occ|=bit(m.to);
    if(abs_piece(p.board[m.from])==1 && p.en_passant==int(m.to) &&
        (m.from&7)!=(m.to&7) && p.board[m.to]==0)
        occ&=~bit(int(m.to)+(side==0?-8:8));
    if(kind==1)return t.pawn[side][m.to];
    if(kind==2)return t.knight[m.to];
    if(kind==6)return t.king[m.to];
    uint64_t attacks=0;
    for(int direction=(kind==3?4:0);direction<(kind==4?4:8);++direction){
        uint64_t ray=t.ray[m.to][direction];
        uint64_t blockers=ray&occ;
        if(blockers){
            int nearest=(direction==0||direction==2||direction==4||direction==6)?
                std::countr_zero(blockers):63-std::countl_zero(blockers);
            ray &= ~t.ray[nearest][direction];
        }
        attacks|=ray;
    }
    return attacks;
}
#endif
// Bounded legal least-value capture continuation, allowing either side to stop.
// This is a cheap 8-capture SEE approximation, not a perfect search oracle.
int mid_see(const Position& p,Move first){
    const int victim=p.board[first.to] ? mid_piece_value(p.board[first.to]):
        (abs_piece(p.board[first.from])==1 && (first.from&7)!=(first.to&7) ? MID_VAL[1]:0);
    if(!victim)return 0;
    Position board=p;
    if(!board.apply_checked(first))return 0;
#ifndef DMT_FAST_TACTICAL
    if(!board.square_attacked(first.to,board.side))return victim;
#endif
    std::array<int,9> gains{};
    gains[0]=victim;
    unsigned count=1;
    while(count<9){
        int sq=first.to;
        int best_sq=-1,best_val=999999;
#ifdef DMT_FAST_TACTICAL
        Position best_after{};
#endif
        const int side=board.side;
#ifdef DMT_FAST_TACTICAL
        uint64_t actors=mid_target_attackers(board,sq);
#else
        uint64_t actors=board.occupied[side];
#endif
        while(actors){
            int from=std::countr_zero(actors);actors&=actors-1;
#ifndef DMT_FAST_TACTICAL
            if(!(board.pseudo_destinations(from)&bit(sq)))continue;
#endif
            // Promotions and kings are legal-filtered; no speculative illegal exchanges.
            int v=mid_piece_value(board.board[from]);
            if(v>=best_val)continue;
            Move m{uint8_t(from),uint8_t(sq),0};
            if(abs_piece(board.board[from])==1 && ((sq>>3)==0 || (sq>>3)==7))m.promotion=1;
            Position test=board;
            if(!test.apply_checked(m))continue;
            best_val=v;best_sq=from;
#ifdef DMT_FAST_TACTICAL
            best_after=std::move(test);
#endif
        }
        if(best_sq<0)break;
        gains[count++]=mid_piece_value(board.board[sq]);
#ifdef DMT_FAST_TACTICAL
        board=std::move(best_after);
#else
        Move m{uint8_t(best_sq),uint8_t(sq),0};
        if(abs_piece(board.board[best_sq])==1&&((sq>>3)==0||(sq>>3)==7))m.promotion=1;
        if(!board.apply_checked(m))fail("SEE continuation failed");
#endif
    }
    int future=0;
    for(int i=int(count)-1;i>=1;--i)future=std::max(0,gains[i]-future);
    return gains[0]-future;
}
// Optional extra information absent from DMG8's 24-bit relation feature vector.
std::array<uint16_t,256> mid_features(const Position& p,const Move* previous,
                                    const Move* own_previous,const V8Step& frame,
                                    MidProfile profile){
    std::array<uint16_t,256> out{};
    const bool see = profile==MidProfile::SEE||profile==MidProfile::AllTactics||profile==MidProfile::AllTacticsGuard||profile==MidProfile::AdaptiveResidual||profile==MidProfile::SEEProxy||profile==MidProfile::AllFast||mid_new_profile(profile)&&mid_mid_enabled(profile);
    const bool threat = profile==MidProfile::NewThreat||profile==MidProfile::AllTactics||profile==MidProfile::AllTacticsGuard||profile==MidProfile::AdaptiveResidual||profile==MidProfile::AllFast||mid_new_profile(profile)&&mid_mid_enabled(profile);
    const bool history = profile==MidProfile::OwnHistory||profile==MidProfile::AllTactics||profile==MidProfile::AllTacticsGuard||profile==MidProfile::AdaptiveResidual||profile==MidProfile::AllFast||mid_new_profile(profile)&&mid_mid_enabled(profile);
    const bool forced = profile==MidProfile::ForcedResponse||profile==MidProfile::AllTactics||profile==MidProfile::AllTacticsGuard||profile==MidProfile::AdaptiveResidual||profile==MidProfile::AllFast||mid_new_profile(profile)&&mid_mid_enabled(profile);
    if(!see&&!threat&&!history&&!forced)return out;
    const uint64_t valuable=p.piece_bits[p.side^1][1]|p.piece_bits[p.side^1][2]|
                            p.piece_bits[p.side^1][3]|p.piece_bits[p.side^1][4];
    uint64_t before_attack=0;
    if(threat){Position viewer=p;viewer.side^=1;AttackCache a(viewer);before_attack=a.attacked_mask;}
    uint64_t last_own_targets=0;
    if(forced && previous && p.board[previous->to]!=0){
        int attacker=p.board[previous->to];
        if((attacker>0)!=(p.side==0)){Position enemy_view=p;enemy_view.side^=1;last_own_targets=enemy_view.pseudo_destinations(previous->to)&p.occupied[p.side];}
    }
    for(unsigned i=0;i<frame.moves.size;++i){
        auto m=frame.moves.a[i].m;
        uint16_t flags=0;
        const bool capture=bool(frame.flags[i]&(1u<<4));
        if(see&&capture){
            int gain;
            if(profile==MidProfile::SEEProxy||profile==MidProfile::AllFast){
                int victim=p.board[m.to]?mid_piece_value(p.board[m.to]):MID_VAL[1];
                Position after=p;after.apply_unchecked(m);
                gain=victim-(after.square_attacked(m.to,after.side)?mid_piece_value(p.board[m.from]):0);
            }else gain=mid_see(p,m);
            if(gain>=100)flags|=1u<<0;
            if(gain<=-100)flags|=1u<<1;
            if(profile==MidProfile::MidSEEValue || profile==MidProfile::OpeningMidSEEValue ||
               profile==MidProfile::OpeningMidSEEValueRate4 || mid_r3_profile(profile) || mid_r4_profile(profile) || mid_r5_profile(profile) || mid_r6_profile(profile)){
                if(gain>=300)flags|=1u<<8;
                if(gain<=-300)flags|=1u<<9;
            }
        }
        const bool need_post=threat||(forced&&previous&&!capture&&p.board[previous->to]!=0);
        uint64_t moved_attacks=0;
        if(need_post){
#ifdef DMT_FAST_TACTICAL
            moved_attacks=mid_landing_capture_targets(p,m);
#ifdef DMT_VERIFY_FAST
            Position after=p;after.apply_unchecked(m);after.side=p.side;
            uint64_t oracle=after.pseudo_destinations(m.to);
            if((moved_attacks&(valuable|bit(previous?previous->to:0)))!=
               (oracle&(valuable|bit(previous?previous->to:0))))
                fail("DMT1 optimized moved capture threats do not match oracle");
#endif
#else
            Position after=p;after.apply_unchecked(m);after.side=p.side;
            moved_attacks=after.pseudo_destinations(m.to);
#endif
        }
        if(threat){
            const uint64_t targets=moved_attacks&valuable&~before_attack;
            if(targets)flags|=1u<<2;
            if((targets&(targets-1))!=0)flags|=1u<<3;
        }
        if(history&&own_previous){
            if(m.from==own_previous->to)flags|=1u<<4;
            if((m.to&7)==(own_previous->to&7) && m.from!=own_previous->to)flags|=1u<<5;
        }
        if(forced){
            if(last_own_targets&bit(m.from))flags|=1u<<6;
            // Favor an active response to the last opponent move, distinct from capture flag5:
            // covering its landing square without directly capturing it.
            if(previous&&!capture && p.board[previous->to]!=0 && (moved_attacks&bit(previous->to)))flags|=1u<<7;
        }
        out[i]=flags;
    }
    return out;
}
// New DMT2 opening signals. These are *features*, not external opening-book weights.
// All orientations are from the side to move. Stable across encoder and decoder.
std::array<uint16_t,256> mid_opening_features(const Position& p,const Move* own_previous,const V8Step& frame){
    std::array<uint16_t,256> out{};
    for(unsigned i=0;i<frame.moves.size;++i){
        auto m=frame.moves.a[i].m;
        unsigned kind=unsigned(abs_piece(p.board[m.from]));
        int fr=p.side?7-int(m.from>>3):int(m.from>>3);
        int tr=p.side?7-int(m.to>>3):int(m.to>>3);
        int file=m.to&7;
        bool pawn=kind==1;
        uint16_t f=0;
        if((kind==2||kind==3)&&fr==0&&tr>0)f|=1u<<0;
        if(kind==2&&fr==0&&tr>0)f|=1u<<1;
        if(pawn&&abs(int(m.to)-int(m.from))==16&&file>=2&&file<=5)f|=1u<<2;
        if(pawn&&(file==3||file==4)&&tr>fr)f|=1u<<3;
        if(kind==6&&std::abs(int(m.to)-int(m.from))==2)f|=1u<<4;
        if(pawn&&(m.from&7)!=(m.to&7)&&(file==3||file==4))f|=1u<<5;
        if((kind==2||kind==3)&&(file==3||file==4)&&(tr==3||tr==4))f|=1u<<6;
        if(own_previous&&m.from==own_previous->to)f|=1u<<7;
        out[i]=f;
    }
    return out;
}
struct MidResidual {
    std::array<int16_t,MID_F> theta{};
    std::array<int32_t,MID_F> rest{};
    std::array<uint32_t,MID_F> seen{};
    uint32_t steps=0;
    void observe_quiet(const std::array<uint16_t,256>& f,const V6Weights& model,
                       const std::array<uint8_t,256>& is_quiet,unsigned n,unsigned chosen,unsigned rate){
        if(!is_quiet[chosen])return; // conditional distribution given quiet
        uint32_t quiet_total=0;
        std::array<uint32_t,MID_F> sums{};
        for(unsigned i=0;i<n;++i)if(is_quiet[i]){
            quiet_total+=model.weights[i];
            unsigned fs=f[i];while(fs){unsigned k=std::countr_zero(fs);fs&=fs-1;sums[k]+=model.weights[i];}
        }
        if(quiet_total<=1)return;
        const ExactDivisor divide(quiet_total);
        for(unsigned k=0;k<MID_F;++k){
            int32_t expected=int32_t(divide(uint64_t(sums[k])*4096+quiet_total/2));
            int32_t actual=(f[chosen]&(1u<<k))?4096:0;
            rest[k]+=int32_t(rate)*(actual-expected);
            int32_t update=rest[k]/4096;
            rest[k]-=update*4096;
            theta[k]=int16_t(std::clamp(int(theta[k])+update,-24,24));
            if(expected>100&&expected<3996)++seen[k];
        }
        ++steps;
        if((steps&63u)==0)for(auto& t:theta){if(t>0)--t;else if(t<0)++t;}
    }
    void observe(const std::array<uint16_t,256>& f,const V6Weights& model,unsigned n,unsigned chosen,bool adaptive,unsigned explicit_rate=0){
        std::array<uint32_t,MID_F> sums{};
        for(unsigned i=0;i<n;++i){unsigned fs=f[i];while(fs){unsigned k=std::countr_zero(fs);fs&=fs-1;sums[k]+=model.weights[i];}}
        const ExactDivisor divide(model.total);
        for(unsigned k=0;k<MID_F;++k){
            int32_t expected=int32_t(divide(uint64_t(sums[k])*4096+model.total/2));
            int32_t actual=(f[chosen]&(1u<<k))?4096:0;
            unsigned rate=explicit_rate?explicit_rate:(adaptive?(seen[k]>24 ? 3:1):2);
            rest[k]+=int32_t(rate)*(actual-expected);
            int32_t update=rest[k]/4096;
            rest[k]-=update*4096;
            theta[k]=int16_t(std::clamp(int(theta[k])+update,-24,24));
            if(expected>100&&expected<3996)++seen[k];
        }
        ++steps;
        if((steps&63u)==0)for(auto& t:theta){if(t>0)--t;else if(t<0)++t;}
    }
};
// DMT3: exact position (including side, rights and EP) transposition context.
// This is an ONLINE archive-local dictionary, not an external opening book.
// Keys are compared in full, not by hash alone, and eviction follows insertion order.
struct R3OpenKey {
    std::array<int8_t,64> board{};
    uint8_t side=0, castling=0;
    int8_t en_passant=-1;
    auto operator<=>(const R3OpenKey&) const = default;
};
R3OpenKey r3_open_key(const Position& p){
    int8_t ep=p.en_passant;
    if(ep>=0){
        // EP target only changes the legal choice set if the side to move has
        // an adjacent pawn that could geometrically capture on it. Keep EP
        // whenever such a pawn exists (even if pinned); otherwise normalize.
        uint64_t adjacent=fast_attack_tables.pawn[p.side^1][unsigned(ep)]&p.piece_bits[p.side][0];
        if(adjacent==0)ep=-1;
    }
    return {p.board,p.side,p.castling,ep};
}
uint16_t r3_move_code(Move m){return uint16_t(m.from | (uint16_t(m.to)<<6) | (uint16_t(m.promotion)<<12));}
struct R3OpenHash {
    size_t operator()(const R3OpenKey& key)const noexcept{
        uint64_t h=0x9e3779b97f4a7c15ULL;
        for(unsigned i=0;i<64;i+=8){uint64_t word;std::memcpy(&word,key.board.data()+i,8);
            h=(h^word)*0x100000001b3ULL;}
        return size_t((h^uint64_t(key.side|(key.castling<<8)|(uint32_t(uint8_t(key.en_passant))<<16)))*0x100000001b3ULL);
    }
};
struct R3OpenHistory {
    std::vector<std::pair<uint16_t,uint16_t>> moves;
    uint16_t n=0;
};
struct R3OpenStore {
    static constexpr size_t limit=32768;
    std::unordered_map<R3OpenKey,R3OpenHistory,R3OpenHash> table;
    std::deque<R3OpenKey> insertion_order;
    V6Weights predict(const Position& p,const CandidateList& cand,const V6Weights& baseline,unsigned prior,unsigned strength) const {
        auto found=table.find(r3_open_key(p));
        if(found==table.end() || found->second.n==0)return baseline;
        const auto& hist=found->second;
        const unsigned n=hist.n;
        std::array<uint64_t,256> raw{};
        const unsigned beta=(n+prior)*100-strength*n;
        for(unsigned i=0;i<cand.size;++i){
            uint16_t code=r3_move_code(cand.a[i].m);
            unsigned c=0;
            for(const auto& x:hist.moves)if(x.first==code){c=x.second;break;}
            raw[i]=uint64_t(baseline.weights[i])*beta + uint64_t(c)*baseline.total*strength;
        }
        return v6_quantize(raw,cand.size);
    }
    void observe(const Position& p,Move chosen){
        auto key=r3_open_key(p);
        auto it=table.find(key);
        if(it==table.end()){
            if(table.size()>=limit){table.erase(insertion_order.front());insertion_order.pop_front();}
            it=table.emplace(key,R3OpenHistory{}).first;
            insertion_order.push_back(std::move(key));
        }
        auto& h=it->second;
        if(h.n>=1000){
            // Deterministic halving keeps the total bounded, preserving positivity of observed moves.
            h.n=0;
            for(auto& [code,c]:h.moves){c=uint16_t((c+1)/2);h.n=uint16_t(h.n+c);}
        }
        uint16_t code=r3_move_code(chosen);
        auto jt=std::find_if(h.moves.begin(),h.moves.end(),[&](auto& v){return v.first==code;});
        if(jt==h.moves.end())h.moves.push_back({code,1});
        else if(jt->second<1000)++jt->second;
        ++h.n;
    }
};

// Geometric single-piece attacks include its first blocker, even if friendly.
// pseudo_destinations() is deliberately NOT used here: it removes friendly squares
// and treats pawn advances as moves rather than defended targets.
uint64_t r3_piece_attacks(const Position& p,int square){
    const auto& t=fast_attack_tables;
    int piece=p.board[square], kind=abs_piece(piece);
    if(!kind)return 0;
    if(kind==1)return t.pawn[piece<0?1:0][square];
    if(kind==2)return t.knight[square];
    if(kind==6)return t.king[square];
    uint64_t occupied=p.occupied[0]|p.occupied[1], out=0;
    for(int direction=kind==3?4:0; direction<(kind==4?4:8);++direction){
        uint64_t ray=t.ray[square][direction], blockers=ray&occupied;
        if(blockers){
            int first=(direction==0||direction==2||direction==4||direction==6)?
                std::countr_zero(blockers):63-std::countl_zero(blockers);
            ray&=~t.ray[first][direction];
        }
        out|=ray;
    }
    return out;
}
// Cheap action-continuity features that are absent from DMT2's 10 tactical flags.
// Side of the historical own move is verified: its square may now hold a capturer.
std::array<uint16_t,256> r3_plan_features(const Position& p,const Move* previous,
                                          const Move* own_previous,const V8Step& frame){
    std::array<uint16_t,256> out{};
    if(!own_previous)return out;
    uint64_t own_attacks=0;
    int last_sq=int(own_previous->to);
    bool own_piece=p.board[last_sq]!=0 && ((p.board[last_sq]>0)==(p.side==0));
    if(own_piece)own_attacks=r3_piece_attacks(p,last_sq);
    uint64_t opponent_targets=0;
    if(previous && p.board[previous->to]!=0 && ((p.board[previous->to]>0)!=(p.side==0))){
        opponent_targets=r3_piece_attacks(p,previous->to);
    }
    for(unsigned i=0;i<frame.moves.size;++i){
        auto m=frame.moves.a[i].m;
        int piece=p.board[m.from];
        uint16_t f=0;
        bool captures=p.board[m.to]!=0 ||
            (abs_piece(piece)==1 && (m.to&7)!=(m.from&7));
        // Move a different piece which was supported by the prior own moved piece.
        if(own_piece && m.from!=last_sq && (own_attacks&bit(m.from)))f|=1u<<0;
        // Newly chosen landing square was supported by the previous own moved piece.
        if(own_piece && m.from!=last_sq && (own_attacks&bit(m.to)))f|=1u<<1;
        // Capture an opposing piece which the previous own move was attacking.
        if(own_piece && captures && (own_attacks&bit(m.to)))f|=1u<<2;
        // Move a piece directly threatened by the opponent's immediately preceding move.
        if(opponent_targets&bit(m.from))f|=1u<<3;
        // Reply through the opponent's vacated square rather than capture its landing square.
        if(previous && m.to==previous->from)f|=1u<<4;
        // A second own action immediately adjacent to the prior own destination.
        int df=std::abs(int(m.to&7)-int(last_sq&7));
        int dr=std::abs(int(m.to>>3)-int(last_sq>>3));
        if(m.from!=last_sq && df<=1 && dr<=1 && (df+dr)>0)f|=1u<<5;
        out[i]=f;
    }
    return out;
}
struct R12Model {
    std::array<int16_t,168> theta{};
    std::array<int32_t,168> residual{};
    uint32_t steps=0;
};
constexpr Dmt12Config R12_FIXED{31,4,100,7,0,10,32};
void r12_validate(const Dmt12Config& c){
    if(!c.families||c.families>31||!c.rate||c.rate>16||c.alpha>100||!c.phase_mask||c.phase_mask>7||c.base>1||c.decay_log2<4||c.decay_log2>12||!c.cap||c.cap>64)fail("invalid DMT12 config");
}
struct MidContext {
    uint8_t r13_mask=127;
    Dmt14Config r14{};
    bool r14_active=false;
    bool r15_active=false;
    Dmt15Config r15{};
    bool r16_active=false;
    Dmt16Config r16{};
    Dmt12Config config{};
    std::array<R12Model,3> r12{};
    V8Context baseline; MidResidual mid,opening,plan;
    std::array<MidResidual,3> single{},compact{};
    R3OpenStore open_store;
    std::array<MidResidual,4> context_residual{};
    MidResidual rank_residual, positional_residual, r6_residual;
    MidResidual r7_intent;
    std::array<MidResidual,4> r7_intent_context{};
    std::array<int16_t,4> r7_calibration{};
    std::array<uint16_t,4> r7_calibration_seen{};
    std::array<uint32_t,4> uncertainty_total{},uncertainty_tail{};
    uint32_t alt=32768;
    // DMT8: five online probability experts, four observable contexts.
    // Counters are integer, capped; decoder can replay them without side data.
    std::array<std::array<uint16_t,5>,4> r8_gate{};
    std::array<uint32_t,4> r8_seen{};
    std::array<std::array<int16_t,5>,4> r8_advantage{};
    std::array<std::array<MidResidual,3>,4> r8_specialists{};
    MidResidual r9_open_residual{},r9_end_residual{};
    MidResidual r10_target_residual{};
    std::array<MidResidual,3> r10_phase_residual{};
    int32_t r10_advantage=0;
    uint32_t r10_seen=0;
};
struct MidStep {
    std::array<std::array<uint8_t,5>,256> r12_features{};
    V8Step base; V6Weights coded;
    std::array<uint16_t,256> additional{},r3_plan{},r4_rank{},positional{},r6_features{};
    std::array<uint8_t,256> pos_quiet{};
    std::array<uint16_t,256> r7_intent{};
    V6Weights r7_before{},r7_expert{};
    uint8_t r7_calibration_context=0;
    V6Weights flat{};
    // DMT8 expert snapshots of the *existing* online models. No new board search.
    std::array<V6Weights,5> r8_experts{};
    uint8_t r8_ctx=0;
    bool r8_active=false;
    std::array<std::array<uint16_t,256>,3> r8_signal{};
    std::array<uint16_t,256> r9_phase_signal{};
    std::array<uint16_t,256> r10_target_signal{};
    V6Weights r10_before{},r10_expert{};
    unsigned context=0;
};
struct FeatureScore10 {
    std::array<int16_t,32> low{},high{};
    explicit FeatureScore10(const std::array<int16_t,10>& theta){
        for(unsigned i=1;i<32;++i){const unsigned b=std::countr_zero(i),prior=i&(i-1);
            low[i]=int16_t(low[prior]+theta[b]);high[i]=int16_t(high[prior]+theta[b+5]);}
    }
    int operator()(unsigned mask)const{return low[mask&31]+high[(mask>>5)&31];}
};
V6Weights mid_residual_cdf(const V6Weights& original,unsigned n,
                             const std::array<uint16_t,256>& extra,const MidResidual& model){
    bool active=false;for(auto weight:model.theta)if(weight){active=true;break;}
    if(!active)return original;
#ifdef DMT_FEATURE_LUT
    const FeatureScore10 score_table(model.theta);
#endif
    std::array<uint64_t,256> raw; // All entries [0,n) are assigned before quantization.
    for(unsigned i=0;i<n;++i){
#ifdef DMT_FEATURE_LUT
        int score=score_table(extra[i]);
#else
        int score=0;unsigned features=extra[i];
        while(features){unsigned k=std::countr_zero(features);features&=features-1;score+=model.theta[k];}
#endif
        score=std::clamp(score,-128,128);
        raw[i]=uint64_t(original.weights[i])*v8_exp2_lut[score+128];
    }
    return v6_quantize(raw,n);
}
// DMT4: contexts depend ONLY on known candidate metadata / pre-move board.
// No class label, Elo, or future move is available to the decoder.
unsigned r4_context(const Position& p,const V8Step& base) {
    if(p.square_attacked(p.king_square(p.side),p.side^1))return 3;
    unsigned captures=0;
    for(unsigned i=0;i<base.moves.size;++i)
        captures+=unsigned((base.flags[i]&(1u<<4))!=0);
    if(captures==0)return 0;
    return captures<=2?1:2;
}
// Relative ranks are calculated across the ACTUAL candidate set.
// Cheap geometry and already computed tactical flags, no added board simulation.
#ifdef DMT_VERIFY_COUNT
std::array<uint16_t,256> r4_rank_features_reference(const Position& p,const V8Step& base,
                                           const std::array<uint16_t,256>& tactics) {
    std::array<uint16_t,256> flags{};
    std::array<int,256> score{};
    unsigned n=base.moves.size;
    for(unsigned i=0;i<n;++i){
        Move m=base.moves.a[i].m;
        bool capture=bool(base.flags[i]&(1u<<4));
        int kind=abs_piece(p.board[m.from]);
        int from_c=std::abs(2*int(m.from&7)-7)+std::abs(2*int(m.from>>3)-7);
        int to_c=std::abs(2*int(m.to&7)-7)+std::abs(2*int(m.to>>3)-7);
        int v=(from_c-to_c)/2;
        if(kind==6)v=0; // king centralization unsafe in midgame
        if(kind==1)v/=2;
        if(capture)v+=3;
        if(tactics[i]&(1u<<0))v+=6; // SEE profitable
        if(tactics[i]&(1u<<1))v-=6;
        if(tactics[i]&(1u<<8))v+=3;
        if(tactics[i]&(1u<<9))v-=3;
        if(tactics[i]&(1u<<2))v+=4; // new threat
        if(tactics[i]&(1u<<3))v+=2;
        if(tactics[i]&(1u<<6))v+=2;
        score[i]=v;
    }
    // Lookup tables replace candidate x candidate ranking comparisons.
    // Scores are bounded by the additions above to (-64,64).
    // Exact strict-rank calculation using only *observed* scores.  The old
    // implementation cleared and scanned 12*129 bins for every position.
    // Sorting <=256 candidates allows all equal scores to share an identical
    // "strictly better" count; class totals are unchanged.
    struct RankItem { int score; unsigned index; uint8_t cls; };
    std::array<RankItem,256> order{};
    std::array<unsigned,12> class_totals{},class_seen{};
    for(unsigned i=0;i<n;++i){
        const auto m=base.moves.a[i].m;
        const auto cls=unsigned(2*(abs_piece(p.board[m.from])-1)+bool(base.flags[i]&(1u<<4)));
        if(score[i]<-64||score[i]>64)fail("relative rank score outside proven range");
        order[i]={score[i],i,uint8_t(cls)};
        ++class_totals[cls];
    }
    std::sort(order.begin(),order.begin()+n,[](const RankItem&a,const RankItem&b){
        if(a.score!=b.score)return a.score>b.score;
        return a.index<b.index;
    });
    unsigned processed=0;
    for(unsigned begin=0;begin<n;){
        unsigned end=begin+1;
        while(end<n && order[end].score==order[begin].score)++end;
        // `processed` and `class_seen` only include strictly higher scores.
        for(unsigned j=begin;j<end;++j){
            const auto item=order[j];
            const auto m=base.moves.a[item.index].m;
            const bool capture=bool(base.flags[item.index]&(1u<<4));
            const unsigned better=processed, same_class=class_totals[item.cls],
                           better_class=class_seen[item.cls];
            uint16_t bits=0;
            if(better==0)bits|=1u<<0;
            if(4*better<=n)bits|=1u<<1;
            if(better_class==0 && same_class>=2)bits|=1u<<2;
            if(same_class>=3 && 3*better_class<=same_class)bits|=1u<<3;
            if(!capture && item.score>=2)bits|=1u<<4;
            if(!capture && item.score<=-2)bits|=1u<<5;
            if(capture && item.score>=6)bits|=1u<<6;
            if(capture && item.score<=-4)bits|=1u<<7;
            flags[item.index]=bits;
        }
        for(unsigned j=begin;j<end;++j)++class_seen[order[j].cls];
        processed=end;
        begin=end;
    }
    return flags;
}

#endif
// Portable, fixed feature tables: no processor-specific instructions.
static constexpr auto rank_center=[](){
    std::array<int8_t,64> a{};
    for(int i=0;i<64;++i){int x=2*(i&7)-7,y=2*(i>>3)-7;a[i]=int8_t(((x<0?-x:x)+(y<0?-y:y))/2);}
    return a;
}();
static constexpr auto rank_tactical=[](){
    std::array<int8_t,1024> a{};
    for(unsigned f=0;f<a.size();++f)a[f]=int8_t(6*bool(f&1)-6*bool(f&2)+3*bool(f&256)-3*bool(f&512)+4*bool(f&4)+2*bool(f&8)+2*bool(f&64));
    return a;
}();
std::array<uint16_t,256> r4_rank_features_count(const Position& p,const V8Step& base,
                                           const std::array<uint16_t,256>& tactics) {
    std::array<uint16_t,256> flags{};
    std::array<int8_t,256> score;
    std::array<uint8_t,256> cls,order;
    std::array<uint16_t,42> count{},offset{};
    std::array<unsigned,12> class_totals{},class_seen{};
    const unsigned n=base.moves.size;
    for(unsigned i=0;i<n;++i){
        const auto m=base.moves.a[i].m;
        const unsigned kind=unsigned(abs_piece(p.board[m.from]));
        const bool capture=bool(base.flags[i]&(1u<<4));
        int v=int(rank_center[m.from])-rank_center[m.to];
        if(kind==6)v=0;
        if(kind==1)v/=2;
        v+=3*int(capture)+int(rank_tactical[tactics[i]&1023u]);
        if(v<-15||v>26)fail("relative rank score outside proven range");
        score[i]=int8_t(v);cls[i]=uint8_t(2*(kind-1)+capture);
        ++class_totals[cls[i]];++count[unsigned(v+15)];
    }
    unsigned cursor=0;
    for(int k=41;k>=0;--k){offset[k]=uint16_t(cursor);cursor+=count[k];}
    for(unsigned i=0;i<n;++i)order[offset[unsigned(score[i]+15)]++]=uint8_t(i);
    for(unsigned begin=0;begin<n;){
        unsigned end=begin+1;
        while(end<n && score[order[end]]==score[order[begin]])++end;
        for(unsigned j=begin;j<end;++j){
            const unsigned i=order[j],same_class=class_totals[cls[i]],better_class=class_seen[cls[i]];
            const bool capture=bool(base.flags[i]&(1u<<4));
            uint16_t bits=0;
            if(begin==0)bits|=1u<<0;
            if(4*begin<=n)bits|=1u<<1;
            if(better_class==0 && same_class>=2)bits|=1u<<2;
            if(same_class>=3 && 3*better_class<=same_class)bits|=1u<<3;
            if(!capture && score[i]>=2)bits|=1u<<4;
            if(!capture && score[i]<=-2)bits|=1u<<5;
            if(capture && score[i]>=6)bits|=1u<<6;
            if(capture && score[i]<=-4)bits|=1u<<7;
            flags[i]=bits;
        }
        for(unsigned j=begin;j<end;++j)++class_seen[cls[order[j]]];
        begin=end;
    }
    return flags;
}

std::array<uint16_t,256> r4_rank_features(const Position& p,const V8Step& base,const std::array<uint16_t,256>& tactics){
    auto result=r4_rank_features_count(p,base,tactics);
#ifdef DMT_VERIFY_COUNT
    if(result!=r4_rank_features_reference(p,base,tactics))fail("counted rank mismatch");
#endif
    return result;
}

// DMT5: candidate-local positional signals, NEVER use the encoded/selected move.
// They are deliberately different from the existing centralization rank score:
// pawn structure, outposts, slider mobility and open/semi-open files.
// All features are heuristic hints, not claims of objectively optimal moves.
unsigned r5_pos_mask(MidProfile profile){
    if(profile==MidProfile::R5Pawn)return 0x00f;
    if(profile==MidProfile::R5Minor)return 0x070;
    if(profile==MidProfile::R5Rook||profile==MidProfile::R5RookQuiet||profile==MidProfile::R5RookPiece||profile==MidProfile::R5RookQuietFast)return 0x380;
    if(profile==MidProfile::R5AllQuietNoPawn)return 0x3f0;
    if(profile==MidProfile::R5AllQuietRookPawn)return 0x38f;
    return 0x3ff;
}
inline bool r5_pawn_attacks(const Position& p,int side,int target){
    return (fast_attack_tables.pawn[side^1][target]&p.piece_bits[side][0])!=0;
}
inline bool r5_pawn_attack_after(const Position& p,int side,int target,Move m,bool moved_pawn){
    uint64_t own=p.piece_bits[side][0];
    if(moved_pawn){own&=~bit(m.from);own|=bit(m.to);}
    return (fast_attack_tables.pawn[side^1][target]&own)!=0;
}
inline bool r5_pawn_can_chase(const Position& p,int enemy,int sq){
    // Pawn in adjacent file ahead of the square may eventually challenge it.
    int f=sq&7,r=sq>>3;
    uint64_t others=p.piece_bits[enemy][0];
    while(others){int x=std::countr_zero(others);others&=others-1;
        if(std::abs((x&7)-f)!=1)continue;
        if(enemy==0 ? (x>>3)<r : (x>>3)>r)return true;
    }
    return false;
}
inline bool r5_is_passed(const Position& p,int side,int square){
    int f=square&7,r=square>>3;
    uint64_t others=p.piece_bits[side^1][0];
    while(others){int x=std::countr_zero(others);others&=others-1;
        if(std::abs((x&7)-f)>1)continue;
        if(side==0 ? (x>>3)>r : (x>>3)<r)return false;
    }
    return true;
}
inline int r5_slider_mobility(const Position& p,int sq,int side,int kind,uint64_t occupied){
    static constexpr int df[8]={1,-1,0,0,1,1,-1,-1};
    static constexpr int dr[8]={0,0,1,-1,1,-1,1,-1};
    int count=0;
    for(int d=0;d<8;++d){
        if(kind==3 && d<4)continue;
        if(kind==4 && d>=4)continue;
        int f=(sq&7)+df[d],r=(sq>>3)+dr[d];
        while(f>=0&&f<8&&r>=0&&r<8){
            int x=r*8+f;
            if(x!=sq && !(p.occupied[side]&bit(x)))++count;
            if(occupied&bit(x))break;
            f+=df[d];r+=dr[d];
        }
    }
    return count;
}
std::array<uint16_t,256> r5_positional_features(const Position& p,const V8Step& base,
             std::array<uint8_t,256>& quiet,MidProfile profile){
    std::array<uint16_t,256> features{};
    const unsigned mask=r5_pos_mask(profile);
    const int us=p.side,them=us^1,enemy_king=p.king_square(them);
    const uint64_t enemy_pawns=p.piece_bits[them][0],my_pawns=p.piece_bits[us][0];
    // Static origin mobility and pawn file occupancy depend on the position,
    // not on individual quiet destination candidates.
    std::array<int8_t,64> bishop_origin_mobility{};
    bishop_origin_mobility.fill(-1);
    uint8_t own_pawn_files=0,enemy_pawn_files=0;
    for(uint64_t pp=my_pawns;pp;pp&=pp-1)own_pawn_files|=uint8_t(1u<<(std::countr_zero(pp)&7));
    for(uint64_t pp=enemy_pawns;pp;pp&=pp-1)enemy_pawn_files|=uint8_t(1u<<(std::countr_zero(pp)&7));
    for(unsigned i=0;i<base.moves.size;++i){
        Move m=base.moves.a[i].m;
        // DMT4 capture flag includes en passant, which we intentionally exclude.
        if(base.flags[i]&(1u<<4))continue;
        int kind=abs_piece(p.board[m.from]);
        quiet[i]=(profile!=MidProfile::R5RookPiece || kind==4)?1:0;
        int rel=us==0?int(m.to>>3):7-int(m.to>>3);
        int f=m.to&7;
        uint16_t x=0;
        if(kind==1 && (mask&0x00f)){
            if((mask&1u) && r5_pawn_attacks(p,us,m.to))x|=1u<<0;
            // A pawn push that attacks an opposing pawn or minor is a potential break.
            uint64_t targets=fast_attack_tables.pawn[us][m.to];
            if((mask&2u) && (targets&(enemy_pawns|p.piece_bits[them][1]|p.piece_bits[them][2])))x|=1u<<1;
            if((mask&4u) && rel>=3&&rel<=5 && !r5_pawn_attacks(p,them,m.to))x|=1u<<2;
            if((mask&8u) && r5_is_passed(p,us,m.to))x|=1u<<3;
        }
        if((kind==2||kind==3) && (mask&0x070)){
            if(kind==2){
                if((mask&0x10u)&&rel>=3&&rel<=5 && r5_pawn_attacks(p,us,m.to)
                       && !r5_pawn_can_chase(p,them,m.to))x|=1u<<4;
                // Knight attack onto the opponent king's ring.
                if((mask&0x20u) && (fast_attack_tables.knight[m.to]&fast_attack_tables.king[enemy_king]))x|=1u<<5;
            }
            if(kind==3 && (mask&0x40u)){
                uint64_t occ=p.occupied[0]|p.occupied[1];
                uint64_t after=(occ&~bit(m.from))|bit(m.to);
                // Count blockers after the quiet move; own occupancy must reflect delta.
                // This feature is deliberately an approximation of safe bishop activity,
                // not an exact legal-move count. Use ray available-square counts.
                auto& old_cached=bishop_origin_mobility[m.from];
                if(old_cached<0)old_cached=int8_t(r5_slider_mobility(p,m.from,us,3,occ));
                int old_mob=old_cached;
                int new_mob=0;
                static constexpr int df[4]={1,1,-1,-1},dr[4]={1,-1,1,-1};
                for(int d=0;d<4;++d){int ff=f+df[d],rr=int(m.to>>3)+dr[d];
                    while(ff>=0&&ff<8&&rr>=0&&rr<8){
                        int sq=rr*8+ff;
                        if(!(p.occupied[us]&bit(sq)) || sq==m.from)++new_mob;
                        if(after&bit(sq))break;
                        ff+=df[d];rr+=dr[d];
                    }
                }
                if(new_mob>=old_mob+2)x|=1u<<6;
            }
        }
        if(kind==4 && (mask&0x380)){
            bool own_file=(own_pawn_files&(1u<<f))!=0;
            bool enemy_file=(enemy_pawn_files&(1u<<f))!=0;
            if((mask&0x80u) && !own_file)x|=1u<<7;
            if((mask&0x100u)&&!own_file&&!enemy_file)x|=1u<<8;
            if((mask&0x200u)&&(rel==6 || f==(enemy_king&7)))x|=1u<<9;
        }
        features[i]=x;
    }
    return features;
}
V6Weights r5_quiet_cdf(const V6Weights& original,unsigned n,
                        const std::array<uint16_t,256>& signals,
                        const std::array<uint8_t,256>& is_quiet,
                        const MidResidual& model){
    bool active=false;for(int t:model.theta)if(t)active=true;
    if(!active)return original;
    std::array<uint64_t,256> weighted{},raw{};
    uint64_t old_quiet=0,new_quiet=0;unsigned quiet_count=0;
    for(unsigned i=0;i<n;++i)if(is_quiet[i]){
        ++quiet_count;old_quiet+=original.weights[i];
        int score=0,fs=signals[i];
        while(fs){int k=std::countr_zero(unsigned(fs));fs&=fs-1;score+=model.theta[k];}
        score=std::clamp(score,-128,128);
        weighted[i]=uint64_t(original.weights[i])*v8_exp2_lut[score+128];
        new_quiet+=weighted[i];
    }
    if(quiet_count<=1 || !new_quiet)return original;
    if(old_quiet==original.total)return mid_residual_cdf(original,n,signals,model);
    for(unsigned i=0;i<n;++i)
        raw[i]=is_quiet[i]?weighted[i]*old_quiet:uint64_t(original.weights[i])*new_quiet;
    return v6_quantize(raw,n);
}

// DMT6 exploratory quiet queen/pawn features; only pre-move board and candidate move.
// Queen attack rays share the bitboard occupancy; approximate positional signals are
// intentionally distinct from strict legality and must be reproducible on decode.
unsigned r6_mask(MidProfile profile){
    if(profile==MidProfile::R6Queen || profile==MidProfile::R6QueenQuiet)return 0x01f;
    if(profile==MidProfile::R6Pawn || profile==MidProfile::R6PawnQuiet)return 0x3e0;
    if(profile==MidProfile::R6QueenPiece)return 0x01f;
    if(profile==MidProfile::R6PawnPiece)return 0x3e0;
    if(profile==MidProfile::R6QueenSafety)return 0x003;
    if(profile==MidProfile::R6PawnPressure)return 0x2e0;
    return 0x3ff;
}
uint64_t r6_queen_rays(const Position& p,int square,uint64_t occupancy,bool rook_only=false){
    (void)p;
    uint64_t result=0;
    const auto& t=fast_attack_tables;
    for(int d=0;d<(rook_only?4:8);++d){
        uint64_t ray=t.ray[square][d];
        uint64_t blockers=ray&occupancy;
        if(blockers){
            int nearest=(d==0||d==2||d==4||d==6)?
                std::countr_zero(blockers):63-std::countl_zero(blockers);
            ray&=~t.ray[nearest][d];
        }
        result|=ray;
    }
    return result;
}
inline bool r6_attacked(const Position& p,int target,int side){
    const auto& t=fast_attack_tables;
    uint64_t nonsliders=(t.pawn[side^1][target]&p.piece_bits[side][0])|
        (t.knight[target]&p.piece_bits[side][1])|
        (t.king[target]&p.piece_bits[side][5]);
    if(nonsliders)return true;
    uint64_t occupied=p.occupied[0]|p.occupied[1];
    for(int d=0;d<8;++d){
        uint64_t blockers=t.ray[target][d]&occupied;
        if(!blockers)continue;
        int sq=(d==0||d==2||d==4||d==6)?
           std::countr_zero(blockers):63-std::countl_zero(blockers);
        if(!(p.occupied[side]&bit(sq)))continue;
        int kind=abs_piece(p.board[sq]);
        if(kind==5||(d<4?kind==4:kind==3))return true;
    }
    return false;
}
std::array<uint16_t,256> r6_positional_features(const Position& p,
        const V8Step& base,std::array<uint8_t,256>& is_quiet,MidProfile profile){
    std::array<uint16_t,256> result{};
    unsigned mask=r6_mask(profile);int us=p.side,them=us^1;
    uint64_t occ=p.occupied[0]|p.occupied[1];
    uint64_t highvalue=p.piece_bits[them][3]|p.piece_bits[them][4];
    uint64_t own_minor=p.piece_bits[us][1]|p.piece_bits[us][2];
    uint64_t own_pawns=p.piece_bits[us][0];
    uint64_t opponent_pawns=p.piece_bits[them][0];
    int ek=p.king_square(them);
    uint64_t king_zone=fast_attack_tables.king[ek]|bit(ek);
    // Per-position memoization.  Neither the starting-board danger query nor
    // its old rays depends on the destination of a *quiet* queen move.
    std::array<int8_t,64> danger_cache{}; danger_cache.fill(-1);
    std::array<uint64_t,64> source_rays{};
    uint64_t source_rays_valid=0;
    for(unsigned i=0;i<base.moves.size;++i){
        Move m=base.moves.a[i].m;
        if(base.flags[i]&(1u<<4))continue;
        int kind=abs_piece(p.board[m.from]);
        is_quiet[i]=(profile==MidProfile::R6QueenPiece?kind==5:
                     (profile==MidProfile::R6PawnPiece?kind==1:true));
        unsigned bits=0;
        if(kind==5 && (mask&0x1fu)){
            // Retreating to an unattacked square is a common queen preservation pattern.
            auto danger=[&](int sq)->bool {
                auto &v=danger_cache[sq];
                if(v<0)v=int8_t(r6_attacked(p,sq,them));
                return v!=0;
            };
            bool old_attacked=danger(m.from);
            bool new_attacked=danger(m.to);
            if((mask&1) && old_attacked && !new_attacked)bits|=1u<<0;
            if((mask&2) && !new_attacked && old_attacked==false)bits|=1u<<1;
            bool want_rays=bool(mask&0x1cu);
            if(want_rays){
                uint64_t after=(occ&~bit(m.from))|bit(m.to);
                if(!(source_rays_valid&bit(m.from))){
                    source_rays[m.from]=r6_queen_rays(p,m.from,occ);
                    source_rays_valid|=bit(m.from);
                }
                uint64_t old_rays=source_rays[m.from];
                uint64_t new_rays=r6_queen_rays(p,m.to,after);
                if((mask&4) && (new_rays&highvalue&~old_rays))bits|=1u<<2;
                if((mask&8) && std::popcount(new_rays&king_zone)>std::popcount(old_rays&king_zone))bits|=1u<<3;
                if((mask&16) && std::popcount(new_rays&~p.occupied[us])>=std::popcount(old_rays&~p.occupied[us])+3)bits|=1u<<4;
            }
        }else if(kind==1 && (mask&0x3e0u)){
            uint64_t before=fast_attack_tables.pawn[us][m.from];
            uint64_t after=fast_attack_tables.pawn[us][m.to];
            uint64_t new_attacks=after&~before;
            // Pawn move newly offers support to a friendly bishop/knight.
            if((mask&32) && (new_attacks&own_minor))bits|=1u<<5;
            // Newly threatens a heavy piece rather than a pawn/minor already in R5.
            if((mask&64) && (new_attacks&highvalue))bits|=1u<<6;
            // Pawn advance newly challenges another pawn or minor piece.
            if((mask&128) && (new_attacks&(opponent_pawns|p.piece_bits[them][1]|p.piece_bits[them][2])))bits|=1u<<7;
            // The pushed pawn gains support where it previously lacked pawn support.
            bool was_supported=(fast_attack_tables.pawn[them][m.from]&own_pawns)!=0;
            uint64_t after_pawns=(own_pawns&~bit(m.from))|bit(m.to);
            bool supported=(fast_attack_tables.pawn[them][m.to]&after_pawns)!=0;
            if((mask&256) && supported&&!was_supported)bits|=1u<<8;
            // Pushing a pawn that exposes a rook/queen on its file (before/after ray).
            if((mask&512)){
                uint64_t own_sliders=p.piece_bits[us][3]|p.piece_bits[us][4];
                uint64_t candidates=own_sliders;
                while(candidates){
                    int sq=std::countr_zero(candidates);candidates&=candidates-1;
                    if((sq&7)!=(m.from&7))continue;
                    bool rook_only=abs_piece(p.board[sq])==4;
                    uint64_t old_rays=r6_queen_rays(p,sq,occ,rook_only);
                    uint64_t new_rays=r6_queen_rays(p,sq,(occ&~bit(m.from))|bit(m.to),rook_only);
                    if((new_rays&~old_rays) & (p.piece_bits[them][0]|p.piece_bits[them][1]|p.piece_bits[them][2]|p.piece_bits[them][3]|p.piece_bits[them][4])){
                        bits|=1u<<9;break;
                    }
                }
            }
        }
        result[i]=uint16_t(bits);
    }
    return result;
}
inline bool r4_use_context(MidProfile p){return (mid_r5_profile(p)||mid_r6_profile(p))||p==MidProfile::R4Context||p==MidProfile::R4ContextRank||p==MidProfile::R4ContextGuard||p==MidProfile::R4All||p==MidProfile::R4AllMild;}
inline bool r4_use_rank(MidProfile p){return (mid_r5_profile(p)||mid_r6_profile(p))||p==MidProfile::R4Rank||p==MidProfile::R4ContextRank||p==MidProfile::R4RankGuard||p==MidProfile::R4All||p==MidProfile::R4AllMild;}
inline bool r4_use_guard(MidProfile p){return p==MidProfile::R4Guard||p==MidProfile::R4ContextGuard||p==MidProfile::R4RankGuard||p==MidProfile::R4All||p==MidProfile::R4AllMild;}
// DMT7: cheap intention cues using only the PRE-MOVE board and legal candidates.
// Avoid candidate board copies/SEE: these are conditional human-choice indicators,
// not objective move-evaluation heuristics.
std::array<uint16_t,256> r7_intent_features(const Position& p,const V8Step& base,
                                    const Move* previous,const Move* own_previous){
    std::array<uint16_t,256> out{};
    const int side=p.side, enemy=side^1, dir=side==0?1:-1;
    const auto& t=fast_attack_tables;
    // Reuse DMG8 tactical candidate relations: bit 1 means escaping danger,
    // bits 2/3 are post-move risk/support. Avoid redundant attack scans.
    for(unsigned i=0;i<base.moves.size;++i){
        Move m=base.moves.a[i].m;
        const int piece=abs_piece(p.board[m.from]);
        const int from_file=m.from&7, to_file=m.to&7;
        const int from_rank=m.from>>3,to_rank=m.to>>3;
        const bool capture=(base.flags[i]&(1u<<4))!=0;
        if(capture)continue; // model focuses on quiet positional intent
        unsigned f=0;
        if((piece==2 || piece==3) && from_rank==(side==0?0:7))f|=1u<<0; // develop minor
        if(piece==1 && (to_rank-from_rank)==2*dir)f|=1u<<1; // pawn space gain
        if(piece==1 && (to_file==3||to_file==4))f|=1u<<2; // central pawn push
        if(piece==1 && to_rank*dir > from_rank*dir &&
            (to_rank==(side==0?4:3)||to_rank==(side==0?5:2)))f|=1u<<3; // deep pawn advance
        if(piece!=1 && (base.flags[i]&(1u<<1)))f|=1u<<4; // flee attack
        if(piece!=1 && !(base.flags[i]&(1u<<2)) && (base.flags[i]&(1u<<3)))f|=1u<<5; // consolidate
        if(piece!=1 && (to_rank*dir>from_rank*dir) && (to_rank==(side==0?4:3)||to_rank==(side==0?5:2)))f|=1u<<6;
        if(piece==1 && (t.pawn[side][m.to]&p.occupied[enemy]))f|=1u<<7; // pawn gains threat
        if(previous && m.to==previous->from)f|=1u<<8; // opponent-vacated square
        if(own_previous && m.from==own_previous->to)f|=1u<<9; // relocate same piece
        // No clock, Elo, external weights, or future move are used.
        out[i]=uint16_t(f);
    }
    return out;
}
V6Weights r7_mix(const V6Weights& base,const V6Weights& expert,unsigned n,unsigned alpha){
    if(alpha==0)return base;
    if(alpha==100)return expert;
    std::array<uint64_t,256> raw; // All entries [0,n) are assigned before quantization.
    for(unsigned i=0;i<n;++i)raw[i]=uint64_t(100-alpha)*base.weights[i]*expert.total +
        uint64_t(alpha)*expert.weights[i]*base.total;
    return v6_quantize(raw,n);
}
unsigned r7_alpha(MidProfile profile,const MidContext& ctx,unsigned context){
    if(profile==MidProfile::R7Mix50)return 50;
    if(profile==MidProfile::R7Mix75)return 75;
    if(!r7_adaptive(profile))return 100;
    // Smoothed signed preference for expert over baseline. Bounded integer state.
    if(ctx.r7_calibration_seen[context]<12)return 50;
    return unsigned(std::clamp(50+int(ctx.r7_calibration[context]),15,85));
}
// Five expert heads: initial relation model, tactical, context/rank,
// positional, and complete DMT6. They all predict the same legal move list.
// Online mixture only uses *previous* decoded moves; integer-only updates.
constexpr unsigned R8_GATES=4096;
inline bool r8_special(MidProfile p){return unsigned(p)>=112 && unsigned(p)<MID_PROFILE_COUNT;}
inline bool r8_fused(MidProfile p){return unsigned(p)>=119 && unsigned(p)<MID_PROFILE_COUNT;}
std::array<uint16_t,5> r8_prior(MidProfile profile){
    switch(profile){
    case MidProfile::R9Base20:case MidProfile::R9Rate3Base20:
    case MidProfile::R9Rate1Base20:case MidProfile::R9Rate4Base20:
        return {0,1092,1092,1093,819};
    case MidProfile::R9Base40:return {0,819,819,820,1638};
    case MidProfile::R9Tactical50:return {0,2048,1024,1024,0};
    case MidProfile::R9Piece50:case MidProfile::R9Rate3Piece50:return {0,1024,2048,1024,0};
    case MidProfile::R9Plan50:return {0,1024,1024,2048,0};
    case MidProfile::R9TacticalPiece:return {0,2048,2048,0,0};
    case MidProfile::R9TPRate1:case MidProfile::R9TPRate3:case MidProfile::R9TPRate4:
    case MidProfile::R9TPSharp:case MidProfile::R9TPDamp:case MidProfile::R9TPSelf2:
    case MidProfile::R9TPSelf3:case MidProfile::R9TPSelf4:case MidProfile::R9TPSelfBlend3:
    case MidProfile::R9TPBetterTactics:case MidProfile::R9TPFeatureMix:
    case MidProfile::R9TPBetterTacticsRate3:case MidProfile::R9TPSelfBetterTactics:
    case MidProfile::R9TPAlternativePiece:case MidProfile::R9TPAllNewFeatures:
    case MidProfile::R9PhaseOpen1:case MidProfile::R9PhaseOpen2:case MidProfile::R9PhaseOpen4:
    case MidProfile::R9PhaseEnd1:case MidProfile::R9PhaseEnd2:case MidProfile::R9PhaseEnd4:
    case MidProfile::R9PhaseBoth1:case MidProfile::R9PhaseBoth2:case MidProfile::R9PhaseBoth4:
    case MidProfile::R9PhaseOpenMix:case MidProfile::R9PhaseBothMix:
    case MidProfile::R9BothRate6:case MidProfile::R9BothRate8:
    case MidProfile::R9Open8End4:case MidProfile::R9Open4End8:
    case MidProfile::R9BothAlt2:case MidProfile::R9BothAlt4:
    case MidProfile::R9BothAlt6:case MidProfile::R9BothAlt8:
    case MidProfile::R9BothSelf4:case MidProfile::R9BothSelf6:
    case MidProfile::R9MixRate1:case MidProfile::R9MixRate3:
    case MidProfile::R9MixRate4:case MidProfile::R9MixRate6:
    case MidProfile::R9MixSharp:case MidProfile::R9MixDamp:
    case MidProfile::R9MixSelf3:case MidProfile::R9MixSelf4:
        return {0,2048,2048,0,0};
    case MidProfile::R9MixTact65:return {0,2662,1434,0,0};
    case MidProfile::R9MixPiece65:return {0,1434,2662,0,0};
    case MidProfile::R9MixTact35:return {0,1434,2662,0,0};
    case MidProfile::R9BasePiece:return {0,0,2048,0,2048};
    default:break;
    }
    if(mid_r9_profile(profile))return {0,1365,1365,1366,0};
    if(profile==MidProfile::R8FusedTacticalPiece)return {0,2048,2048,0,0};
    if(profile==MidProfile::R8FusedTacticalPlan)return {0,2048,0,2048,0};
    if(profile==MidProfile::R8FusedPiecePlan)return {0,0,2048,2048,0};
    if(profile==MidProfile::R8SpecialUniform || profile==MidProfile::R8FusedUniform)return {0,1365,1365,1366,0};
    if(profile==MidProfile::R8SpecialPiece)return {0,0,1024,0,3072};
    if(profile==MidProfile::R8SpecialTactical)return {0,1024,0,0,3072};
    if(profile==MidProfile::R8SpecialPlan)return {0,0,0,1024,3072};
    if(r8_special(profile))return {0,256,256,256,3328};
    if(profile==MidProfile::R8Uniform)return {819,819,819,819,820};
    if(profile==MidProfile::R8ContextHeavyFinal)return {128,256,512,768,2432};
    if(profile==MidProfile::R8ContextTactical)return {128,1408,768,384,1408};
    if(profile==MidProfile::R8ContextTwo)return {0,0,0,1024,3072};
    return {205,410,615,819,2047};
}
inline bool r8_switch_profile(MidProfile p){return unsigned(p)>=107 && unsigned(p)<112;}
unsigned r8_context_index(MidProfile p,unsigned ctx){
    return p==MidProfile::R8Global || p==MidProfile::R8Uniform || p==MidProfile::R8SwitchGlobal || p==MidProfile::R8SpecialGlobal || p==MidProfile::R8SpecialUniform || p==MidProfile::R8FusedUniform || p==MidProfile::R8FusedGlobal || mid_r9_profile(p)?0:ctx;
}
V6Weights r8_combine(const MidStep& step,const MidContext& ctx,MidProfile p){
    const unsigned n=step.base.moves.size;
    if(p==MidProfile::R8Control || p==MidProfile::R8SpecialControl)return step.r8_experts[4];
    const unsigned context=r8_context_index(p,step.r8_ctx);
    if(r8_switch_profile(p)){
        if(ctx.r8_seen[context]<12)return step.r8_experts[4];
        const auto& scores=ctx.r8_advantage[context];
        unsigned winner=4;
        int threshold=p==MidProfile::R8SwitchConservative?170:
            p==MidProfile::R8SwitchQuick?25:80;
        for(unsigned k=0;k<4;++k){
            if(p==MidProfile::R8SwitchTwo && k!=3)continue;
            if(scores[k]>threshold){winner=k;threshold=scores[k];}
        }
        return step.r8_experts[winner];
    }
    const auto prior=r8_prior(p);
    const auto& gate=ctx.r8_gate[context];
    std::array<uint64_t,256> raw{};
    for(unsigned k=0;k<5;++k){
        unsigned g=(ctx.r8_seen[context]==0 || p==MidProfile::R8Uniform)?prior[k]:gate[k];
        if(!g)continue;
        const auto& ex=step.r8_experts[k];
        // Fixed-point denominator compensation. 2^20 / sum avoids FP and
        // changing probabilities with host SIMD/rounding mode.
        uint64_t multiplier=(uint64_t(g)<<24)/ex.total;
        for(unsigned j=0;j<n;++j)raw[j]+=multiplier*ex.weights[j];
    }
    return v6_quantize(raw,n);
}
void r8_update(MidContext& ctx,const MidStep&step,unsigned index,MidProfile p){
    if(p==MidProfile::R8Control||p==MidProfile::R8Uniform||p==MidProfile::R8SpecialControl||p==MidProfile::R8SpecialUniform||p==MidProfile::R8FusedUniform||unsigned(p)>=122)return;
    const unsigned context=r8_context_index(p,step.r8_ctx);
    const auto prior=r8_prior(p);
    auto& gate=ctx.r8_gate[context];
    uint32_t& seen=ctx.r8_seen[context];
    if(seen==0)gate=prior;
    ++seen;
    if(r8_switch_profile(p)){
        auto& scores=ctx.r8_advantage[context];
        const auto& baseline=step.r8_experts[4];
        for(unsigned k=0;k<4;++k){
            const auto& expert=step.r8_experts[k];
            int64_t left=int64_t(expert.weights[index])*baseline.total;
            int64_t right=int64_t(baseline.weights[index])*expert.total;
            int delta=int(std::clamp<int64_t>((left-right)*128/std::max<int64_t>(right,1),-64,64));
            const int leak=(p==MidProfile::R8SwitchQuick?7:31);
            scores[k]=int16_t(std::clamp((int(scores[k])*leak)/(leak+1)+delta,-1500,1500));
        }
        return;
    }
    // Bayesian-type expert posterior: gate[k] *= P_k(chosen). Exact integer,
    // with minimum mass and forgetting toward a prior to recover after shifts.
    std::array<uint64_t,5> raw{};
    uint64_t sum=0;
    for(unsigned k=0;k<5;++k){
        const auto& ex=step.r8_experts[k];
        uint64_t value=uint64_t(gate[k])*ex.weights[index]*65536/ex.total;
        raw[k]=value;sum+=value;
    }
    if(!sum) return;
    // Uniform/context/global speeds and forgetting strength are ablations.
    const unsigned forget=p==MidProfile::R8ContextFast?32:
       p==MidProfile::R8ContextSlow?2:8; // /1024 per move
    uint32_t total=0;
    for(unsigned k=0;k<5;++k){
        uint32_t posterior=uint32_t(raw[k]*R8_GATES/sum);
        uint32_t v=(posterior*(1024-forget)+uint32_t(prior[k])*forget+512)/1024;
        if(prior[k]>0)v=std::max(1u,v);
        gate[k]=uint16_t(v);total+=v;
    }
    // Sum need not equal 4096: mixture renormalizes weights regardless.
}
// Three separate ONLINE specialists sharing the already-computed DMT6 features:
// tactical classification, quiet piece-type choice, and plan continuity.
// Every feature is known from the PRE-MOVE board and candidate moves.
void r8_special_features(const Position& p,MidStep& s,MidProfile profile){
    auto n=s.base.moves.size;
    for(unsigned i=0;i<n;++i){
        const auto move=s.base.moves.a[i].m;
        const unsigned piece=unsigned(abs_piece(p.board[move.from]));
        const bool capture=(s.base.flags[i]&(1u<<4))!=0;
        unsigned tac=0,types=0,plan=0;
        if(capture)tac|=1u<<0;
        if(s.additional[i]&(1u<<0))tac|=1u<<1;  // winning exchange
        if(s.additional[i]&(1u<<1))tac|=1u<<2;  // losing exchange
        if(s.r4_rank[i]&(1u<<0))tac|=1u<<3; // highest rank
        if(s.r4_rank[i]&(1u<<6))tac|=1u<<4; // good tactical capture
        if(s.additional[i]&(1u<<2))tac|=1u<<5; // new threat
        if(!capture){
            if(piece>=1 && piece<=6)types|=1u<<(piece-1);
            if(s.positional[i])types|=1u<<6;
            if(s.r6_features[i])types|=1u<<7;
            if(s.r4_rank[i]&(1u<<1))types|=1u<<8;
        }else types|=1u<<9;
        plan=s.r3_plan[i]&0x3f;
        if(!capture && (s.additional[i]&(1u<<4)))plan|=1u<<6;
        if(!capture && (s.r4_rank[i]&(1u<<1)))plan|=1u<<7;
        if(!capture && (s.positional[i]&(1u<<0)))plan|=1u<<8;
        if(profile==MidProfile::R9TPBetterTactics || profile==MidProfile::R9TPBetterTacticsRate3 ||
           profile==MidProfile::R9TPSelfBetterTactics || profile==MidProfile::R9TPAllNewFeatures){
            if(s.additional[i]&(1u<<8))tac|=1u<<6;
            if(s.additional[i]&(1u<<9))tac|=1u<<7;
            if(s.base.flags[i]&(1u<<5))tac|=1u<<8;
            if(s.base.flags[i]&(1u<<8))tac|=1u<<9;
        }else if(profile==MidProfile::R9TPFeatureMix || unsigned(profile)>=157){
            tac=0;
            if(capture)tac|=1u<<0;
            if(s.additional[i]&(1u<<0))tac|=1u<<1;
            if(s.additional[i]&(1u<<1))tac|=1u<<2;
            if(s.base.flags[i]&(1u<<5))tac|=1u<<3;
            if(s.base.flags[i]&(1u<<8))tac|=1u<<4;
            if(s.additional[i]&(1u<<2))tac|=1u<<5;
            if(s.base.flags[i]&(1u<<1))tac|=1u<<6;
            if(s.base.flags[i]&(1u<<3))tac|=1u<<7;
            if(s.r4_rank[i]&(1u<<0))tac|=1u<<8;
            if(s.r4_rank[i]&(1u<<6))tac|=1u<<9;
        }
        if(profile==MidProfile::R9TPAlternativePiece || profile==MidProfile::R9TPAllNewFeatures){
            types=0;
            if(!capture){
                if(piece==1)types|=1u<<0;
                if(piece==2 || piece==3)types|=1u<<1;
                if(piece==4)types|=1u<<2;
                if(piece==5)types|=1u<<3;
                if(piece==6)types|=1u<<4;
                if(s.positional[i])types|=1u<<5;
                if(s.r6_features[i])types|=1u<<6;
                if(s.r4_rank[i]&(1u<<1))types|=1u<<7;
                if(s.base.flags[i]&(1u<<1))types|=1u<<8;
            }else types|=1u<<9;
        }
        s.r8_signal[0][i]=uint16_t(tac);
        s.r8_signal[1][i]=uint16_t(types);
        s.r8_signal[2][i]=uint16_t(plan);
    }
}
// Experimental additional opening/endgame residual: no extra board search,
// flags and piece identities are strictly pre-move candidate metadata.
inline bool r9_phase_open(MidProfile p){if(unsigned(p)>=168)return true;
 return p==MidProfile::R9PhaseOpen1 ||
 p==MidProfile::R9PhaseOpen2 || p==MidProfile::R9PhaseOpen4 ||
 p==MidProfile::R9PhaseBoth1 || p==MidProfile::R9PhaseBoth2 ||
 p==MidProfile::R9PhaseBoth4 || p==MidProfile::R9PhaseOpenMix ||
 p==MidProfile::R9PhaseBothMix;}
inline bool r9_phase_end(MidProfile p){if(unsigned(p)>=168)return true;
 return p==MidProfile::R9PhaseEnd1 ||
 p==MidProfile::R9PhaseEnd2 || p==MidProfile::R9PhaseEnd4 ||
 p==MidProfile::R9PhaseBoth1 || p==MidProfile::R9PhaseBoth2 ||
 p==MidProfile::R9PhaseBoth4 || p==MidProfile::R9PhaseBothMix;}
inline unsigned r9_phase_rate(MidProfile p,bool opening){
 if(p==MidProfile::R9BothRate6 || p==MidProfile::R9BothAlt6 || p==MidProfile::R9BothSelf6 || unsigned(p)>=178)return 6;
 if(p==MidProfile::R9BothRate8 || p==MidProfile::R9BothAlt8)return 8;
 if(p==MidProfile::R9Open8End4)return opening?8:4;
 if(p==MidProfile::R9Open4End8)return opening?4:8;
 if(p==MidProfile::R9BothAlt2)return 2;
 if(unsigned(p)>=168)return 4;
 if(p==MidProfile::R9PhaseOpen1 || p==MidProfile::R9PhaseEnd1 || p==MidProfile::R9PhaseBoth1)return 1;
 if(p==MidProfile::R9PhaseOpen4 || p==MidProfile::R9PhaseEnd4 || p==MidProfile::R9PhaseBoth4)return 4;
 return 2;
}
std::array<uint16_t,256> r9_phase_features(const Position& p,const V8Step& step,bool opening,MidProfile profile){
 std::array<uint16_t,256> f{};
 const int side=p.side;
 for(unsigned i=0;i<step.moves.size;++i){
  Move m=step.moves.a[i].m;
  unsigned piece=unsigned(abs_piece(p.board[m.from]));
  int from=side==0?int(m.from>>3):7-int(m.from>>3);
  int to=side==0?int(m.to>>3):7-int(m.to>>3);
  unsigned dest_file=m.to&7;
  unsigned x=0;
  if(step.flags[i]&(1u<<4))x|=1u<<0;       // capture
  if(step.flags[i]&(1u<<8))x|=1u<<1;       // check
  if(step.flags[i]&(1u<<5))x|=1u<<2;       // recapture
  if(step.flags[i]&(1u<<1))x|=1u<<3;       // escape attack
  if(step.flags[i]&(1u<<3))x|=1u<<4;       // destination supported
  if(piece==1 && (dest_file==3||dest_file==4))x|=1u<<5;
  if(opening){
   if((piece==2||piece==3)&&from==0&&to>0)x|=1u<<6;
   if(piece==6 && (std::abs(int(m.from)-int(m.to))==2))x|=1u<<7;
   if(piece==5)x|=1u<<8;
   if(piece==1 && to>=4)x|=1u<<9;
  }else{
   if(piece==1 && to>=5)x|=1u<<6;
   if(piece==6)x|=1u<<7;
   if(piece==4 || piece==5)x|=1u<<8;
   if(m.promotion || (piece==1 && to==6))x|=1u<<9;
  }
  if(unsigned(profile)>=172 && unsigned(profile)<=175){
    x=0;
    if(opening){
      if(piece==1 && (dest_file==3||dest_file==4))x|=1u<<0;
      if(piece==1 && to-from==2)x|=1u<<1;
      if(piece==1 && to>=4)x|=1u<<2;
      if(piece==2 && from==0)x|=1u<<3;
      if(piece==3 && from==0)x|=1u<<4;
      if(piece==6 && std::abs(int(m.from)-int(m.to))==2)x|=1u<<5;
      if(piece==5)x|=1u<<6;
    }else{
      if(piece==1 && to>=5)x|=1u<<0;
      if(piece==1 && to==6)x|=1u<<1;
      if(piece==6)x|=1u<<2;
      if(piece==4)x|=1u<<3;
      if(piece==5)x|=1u<<4;
      if(m.promotion)x|=1u<<5;
      if(piece==1 && to>from)x|=1u<<6;
    }
    if(step.flags[i]&(1u<<4))x|=1u<<7;
    if(step.flags[i]&(1u<<8))x|=1u<<8;
    if(step.flags[i]&(1u<<1))x|=1u<<9;
  }
  f[i]=uint16_t(x);
 }
 return f;
}
// DMT10 experimental destination-first calibration. Features are functions of
// destination squares / observable positions, not of the chosen move. This
// Geometry-only signals are constant across origins at a given destination.
// Tactical signals (check/SEE) can differ by origin; these also change the
// conditional origin probabilities. This is candidate calibration, not a
// destination-first arithmetic coding factorization. Online learning is causal.
std::array<uint16_t,256> r10_target_features(const Position& p,const MidStep& s,
                                             const Move* previous,MidProfile profile,unsigned stage){
    std::array<uint16_t,256> out{};
    unsigned counts[64]{};
    for(unsigned i=0;i<s.base.moves.size;++i)++counts[s.base.moves.a[i].m.to];
    unsigned id=unsigned(profile);
    unsigned kind=0;
    if(id>=231)kind=stage==1?(id==240?0:(id==241?2:1)):0;
    else if(id>=217)kind=(id==221 || id==222 || id==225 || id==226 || id==229)?1:
                 (id==223 || id==224?2:0);
    else kind=(id>=201 && id<=212)?1:(id>=213?0:(id-189)/4);
    int king=p.king_square(p.side^1);
    for(unsigned i=0;i<s.base.moves.size;++i){
        Move m=s.base.moves.a[i].m;
        int x=int(m.to&7),rank=p.side==0?int(m.to>>3):7-int(m.to>>3);
        int df=std::abs(x-int(king&7)),dr=std::abs(int(m.to>>3)-int(king>>3));
        unsigned f=0;
        if(kind==0 || kind==2){
            // Eight features encode the destination marginal independently of origin.
            if(x>=2 && x<=5)f|=1u<<0;
            if(x==3 || x==4)f|=1u<<1;
            if(rank>=3)f|=1u<<2;
            if(rank>=5)f|=1u<<3;
            if(x<=2)f|=1u<<4;
            if(x>=5)f|=1u<<5;
            if(df<=1 && dr<=1)f|=1u<<6;
            if(df<=2 && dr<=2)f|=1u<<7;
            if(counts[m.to]>=2)f|=1u<<8;
            if(counts[m.to]>=3)f|=1u<<9;
        }else{
            if(counts[m.to]>=2)f|=1u<<0;
            if(counts[m.to]>=3)f|=1u<<1;
            if(s.base.flags[i]&(1u<<4))f|=1u<<2;
            if(s.base.flags[i]&(1u<<8))f|=1u<<3;
            if(previous && m.to==previous->to)f|=1u<<4;
            if(previous && m.to==previous->from)f|=1u<<5;
            if(df<=2 && dr<=2)f|=1u<<6;
            if(rank>=5)f|=1u<<7;
            if(s.additional[i]&(1u<<0))f|=1u<<8;
            if(s.additional[i]&(1u<<1))f|=1u<<9;
        }
        // Hybrid keeps six destination geometry signals plus four tactical ones.
        if(kind==2){
            f=0;
            if(x==3 || x==4)f|=1u<<0;
            if(rank>=3)f|=1u<<1;
            if(rank>=5)f|=1u<<2;
            if(df<=2 && dr<=2)f|=1u<<3;
            if(counts[m.to]>=2)f|=1u<<4;
            if(counts[m.to]>=3)f|=1u<<5;
            if(s.base.flags[i]&(1u<<4))f|=1u<<6;
            if(s.base.flags[i]&(1u<<8))f|=1u<<7;
            if(previous && m.to==previous->to)f|=1u<<8;
            if(previous && m.to==previous->from)f|=1u<<9;
        }
        out[i]=uint16_t(f);
    }
    return out;
}
unsigned r10_rate(MidProfile profile,unsigned stage){
 unsigned id=unsigned(profile);
 if(id>=231 && stage!=1)return 4;
 if(id>=231){
    if(id==234 || id==232 || id==236)return 4;
    if(id==237)return 1;
    if(id==242)return 3;
    return 2;
 }
 if(id>=217)return (id==220 || id==222 || id==224 || id==226 || id==228)?4:2;
 if(id==201 || id==203)return 1;
 if(id==202 || id==204)return 3;
 if(id>=205)return (id%2)?2:4;
 return ((id-189)%4)&1 ? 4:2;
}
unsigned r10_alpha(MidProfile profile,const MidContext& ctx,unsigned stage){
 unsigned id=unsigned(profile);
 if(id>=231){
    if(stage!=1)return 100;
    if(id==233 || id==234 || id==242)return 100;
    if(id==237 || id==239)return 75;
    if(id==238)return 25;
    if(id==235 || id==236){
       if(ctx.r10_seen<24)return 50;
       return unsigned(std::clamp(50+ctx.r10_advantage/5,0,100));
    }
    return 50;
 }
 if(id>=217)return (id>=225 && id<=228)?100:50;
 if(id==201 || id==202)return 100;
 if(id==203 || id==204)return 75;
 if(id==205 || id==206)return 25;
 if(id==207 || id==208)return 75;
 if(id>=209){
   unsigned prior=(id==211||id==212||id==215||id==216)?25:50;
   if(ctx.r10_seen<24)return prior;
   return unsigned(std::clamp(int(prior)+ctx.r10_advantage/5,0,100));
 }
 return ((id-189)%4)>=2?100:50;
}
bool r10_phase_on(MidProfile profile,unsigned stage){
    unsigned id=unsigned(profile);
    if(id<217)return false;
    if(stage==1)return id>=229;
    if(id==217)return stage==0;
    if(id==218)return stage==2;
    return true;
}
// Fused distribution: each specialist has its own normalizer, but all three
// distributions are mixed before the final integer CDF quantization.
// No intermediate 16,000-count CDFs or three per-candidate divide passes.
V6Weights r8_fused_cdf(const MidStep& step,const MidContext& ctx,MidProfile p){
    const auto& base=step.r8_experts[4];
    const unsigned n=step.base.moves.size;
    const unsigned context=r8_context_index(p,step.r8_ctx);
    const auto prior=r8_prior(p);
    const auto& g=ctx.r8_gate[context];
    std::array<std::array<uint64_t,256>,3> parts{};
    std::array<uint64_t,3> inv{};
    for(unsigned k=0;k<3;++k){
        if(!prior[k+1])continue;
        const auto& theta=ctx.r8_specialists[context][k].theta;
#ifdef DMT_FEATURE_LUT
        const FeatureScore10 score_table(theta);
#endif
        uint64_t sum=0;
        for(unsigned j=0;j<n;++j){
#ifdef DMT_FEATURE_LUT
            int score=score_table(step.r8_signal[k][j]);
#else
            int score=0;
            uint16_t mask=step.r8_signal[k][j];
            while(mask){unsigned bit=std::countr_zero(mask);mask&=mask-1;score+=theta[bit];}
#endif
            if(p==MidProfile::R9Sharp || p==MidProfile::R9TPSharp || p==MidProfile::R9MixSharp)score=(score*3)/2;
            if(p==MidProfile::R9Damp || p==MidProfile::R9TPDamp || p==MidProfile::R9MixDamp)score=(score*3)/4;
            score=std::clamp(score,-128,128);
            auto x=uint64_t(base.weights[j])*v8_exp2_lut[score+128];
            parts[k][j]=x;sum+=x;
        }
        inv[k]=(uint64_t(1)<<36)/sum;
    }
    std::array<uint64_t,256> raw{};
    const uint64_t base_inv=(uint64_t(1)<<36)/base.total;
    for(unsigned j=0;j<n;++j){
        uint64_t w=0;
        for(unsigned k=0;k<3;++k){
            if(!prior[k+1])continue;
            unsigned gate=ctx.r8_seen[context]==0?prior[k+1]:g[k+1];
            if(p==MidProfile::R8FusedUniform || unsigned(p)>=122)gate=prior[k+1];
            w+=uint64_t(gate)*parts[k][j]*inv[k];
        }
        unsigned prior_weight=ctx.r8_seen[context]==0?prior[4]:g[4];
        if(p==MidProfile::R8FusedUniform || unsigned(p)>=122)prior_weight=prior[4];
        w+=uint64_t(prior_weight)*base.weights[j]*base_inv;
        raw[j]=w;
    }
    return v6_quantize(raw,n);
}
void mid_step_legacy_into(MidStep& result,const Position& p,const Move* previous,const Move* own_previous,
                 unsigned ply,unsigned stage,MidProfile profile,const MidContext& ctx){
    const MidProfile r11profile=profile;
    if(unsigned(profile)>=245)profile=MidProfile::R10ComboTact4Full;
    const MidProfile r10profile=profile;
    if(mid_r10_profile(profile))profile=MidProfile::R9MixSharp;
    const MidProfile r7profile=profile;
    const bool r8=mid_r8_profile(profile)||mid_r9_profile(profile);
    profile=r7_base(profile);
    // Caller supplies a freshly initialized result; no temporary MidStep.
    if(unsigned(r10profile)==243||unsigned(r10profile)==244){
        result.base=v8_step(p,previous,ply,V8Model::RelationalJoint,ctx.baseline);
        const unsigned n=result.base.moves.size;
        for(unsigned i=0;i<n;++i){
            auto m=result.base.moves.a[i].m;auto f=result.base.flags[i];
            const int rank=p.side?7-int(m.to>>3):int(m.to>>3);
            const unsigned file=m.to&7;unsigned features=0;
            if(f&(1u<<4))features|=1u<<0; // capture
            if(f&(1u<<6))features|=1u<<1; // favorable material capture
            if(f&(1u<<8))features|=1u<<2; // check
            if(f&(1u<<1))features|=1u<<3; // escape attack
            if(f&(1u<<2))features|=1u<<4; // landing attacked
            if(f&(1u<<3))features|=1u<<5; // landing supported
            if(file>=2&&file<=5&&rank>=2&&rank<=5)features|=1u<<6;
            if(abs_piece(p.board[m.from])==1&&rank>=4)features|=1u<<7;
            if(own_previous&&m.from==own_previous->to)features|=1u<<8;
            if(previous&&m.to==previous->to)features|=1u<<9;
            result.additional[i]=uint16_t(features);
        }
        result.coded=mid_residual_cdf(result.base.coded,n,result.additional,ctx.single[stage]);
        return;
    }
    result.base=v8_step(p,previous,ply,V8Model::RelationalJoint,ctx.baseline);
    result.coded=result.base.coded;
    if(r8){ result.r8_active=true;result.r8_experts[0]=result.coded; result.r8_ctx=uint8_t(r4_context(p,result.base)); }
    auto n=result.base.moves.size;
    if(stage==0 && mid_opening_enabled(profile)){
        result.additional=mid_opening_features(p,own_previous,result.base);
        result.coded=mid_residual_cdf(result.coded,n,result.additional,ctx.opening);
        if(mid_guard_open(profile))result.coded=mid_guard(result.coded,n,5);
        if(mid_r3_open_position(profile)){
            result.coded=ctx.open_store.predict(p,result.base.moves,result.coded,
                (unsigned(profile)>=40?2:4),
                profile==MidProfile::R3OpeningPositionWeak?25:
                ((mid_r4_profile(profile)||mid_r5_profile(profile)||mid_r6_profile(profile))?75:(unsigned(profile)>=40 ? (unsigned(profile)>=43?90:75):60)));
        }
        if(r9_phase_open(r7profile)){
            result.r9_phase_signal=r9_phase_features(p,result.base,true,r7profile);
            auto prediction=mid_residual_cdf(result.coded,n,result.r9_phase_signal,ctx.r9_open_residual);
            result.coded=(r7profile==MidProfile::R9PhaseOpenMix || r7profile==MidProfile::R9PhaseBothMix)?
                r7_mix(result.coded,prediction,n,60):prediction;
        }
        if(mid_r10_profile(r10profile) && r10_phase_on(r10profile,stage)){
            result.r10_before=result.coded;
            result.r10_target_signal=r10_target_features(p,result,previous,r10profile,stage);
            result.r10_expert=mid_residual_cdf(result.r10_before,n,result.r10_target_signal,ctx.r10_phase_residual[stage]);
            result.coded=r7_mix(result.r10_before,result.r10_expert,n,r10_alpha(r10profile,ctx,stage));
        }
        return;
    }
    if(stage==2 && r9_phase_end(r7profile)){
        result.r9_phase_signal=r9_phase_features(p,result.base,false,r7profile);
        auto prediction=mid_residual_cdf(result.coded,n,result.r9_phase_signal,ctx.r9_end_residual);
        result.coded=r7profile==MidProfile::R9PhaseBothMix?
            r7_mix(result.coded,prediction,n,60):prediction;
        if(mid_r10_profile(r10profile) && r10_phase_on(r10profile,stage)){
            result.r10_before=result.coded;
            result.r10_target_signal=r10_target_features(p,result,previous,r10profile,stage);
            result.r10_expert=mid_residual_cdf(result.r10_before,n,result.r10_target_signal,ctx.r10_phase_residual[stage]);
            result.coded=r7_mix(result.r10_before,result.r10_expert,n,r10_alpha(r10profile,ctx,stage));
        }
        return;
    }
    if(stage!=1 || (mid_opening_enabled(profile)&&!mid_mid_enabled(profile)))return;
    switch(profile){
        case MidProfile::FlatMild:case MidProfile::FlatStrong:case MidProfile::Sharp:
            result.coded=mid_temper(result.coded,n,profile);return;
        case MidProfile::Guard5:result.coded=mid_guard(result.coded,n,5);return;
        case MidProfile::Guard12:result.coded=mid_guard(result.coded,n,12);return;
        case MidProfile::TemperatureMixture:{
            result.flat=mid_temper(result.coded,n,MidProfile::FlatMild);
            std::array<uint64_t,256> raw{};
            for(unsigned i=0;i<n;++i){
                raw[i]=uint64_t(65536-ctx.alt)*result.base.coded.weights[i]*result.flat.total+
                       uint64_t(ctx.alt)*result.flat.weights[i]*result.base.coded.total;
            }
            result.coded=v6_quantize(raw,n);return;
        }
        case MidProfile::TemperatureConditional:{
            unsigned best=0;
            for(unsigned i=0;i<n;++i)best=std::max(best,unsigned(result.coded.weights[i]));
            if(n>=20&&uint64_t(best)*5>result.coded.total)
                result.coded=mid_temper(result.coded,n,MidProfile::FlatMild);
            return;
        }
        case MidProfile::SEE:case MidProfile::NewThreat:case MidProfile::OwnHistory:
        case MidProfile::ForcedResponse:case MidProfile::AdaptiveResidual:
        case MidProfile::AllTactics:case MidProfile::AllTacticsGuard:
        case MidProfile::SEEProxy:case MidProfile::AllFast:
        case MidProfile::AllGuard2:case MidProfile::AllGuard10:
        case MidProfile::AllGuardAdaptive:case MidProfile::AllGuardRate1:
        case MidProfile::AllGuardRate3:case MidProfile::AllGuardRate4:case MidProfile::AllGuardRate6:
        case MidProfile::OpeningOnly:case MidProfile::OpeningGuard:
        case MidProfile::OpeningMid:case MidProfile::OpeningMidGuard:
        case MidProfile::OpeningMidGuardRate3:case MidProfile::OpeningMidGuardRate4:
        case MidProfile::MidSEEValue:case MidProfile::OpeningMidSEEValue:
        case MidProfile::OpeningMidSEEValueRate4:
        case MidProfile::R3Control:case MidProfile::R3OpeningPosition:
        case MidProfile::R3MiddlegamePlan:case MidProfile::R3Both:
        case MidProfile::R3OpeningPositionWeak:case MidProfile::R3PlanRate4:
        case MidProfile::R3BothRate4:case MidProfile::R3OpeningStrong:
        case MidProfile::R3BothStrong:case MidProfile::R3BothStrongRate4:
        case MidProfile::R3OpeningAggressive:case MidProfile::R3BothAggressive:
        case MidProfile::R4Control:case MidProfile::R4Context:case MidProfile::R4Rank:
        case MidProfile::R4Guard:case MidProfile::R4ContextRank:case MidProfile::R4ContextGuard:
        case MidProfile::R4RankGuard:case MidProfile::R4All:case MidProfile::R4AllMild:
        case MidProfile::R5Control:case MidProfile::R5Pawn:case MidProfile::R5Minor:
        case MidProfile::R5Rook:case MidProfile::R5All:case MidProfile::R5AllQuiet:
        case MidProfile::R5AllQuietSlow:case MidProfile::R5AllQuietFast:
        case MidProfile::R5AllQuietGuard:case MidProfile::R5RookQuiet:
        case MidProfile::R5RookPiece:case MidProfile::R5RookQuietFast:
        case MidProfile::R5AllQuietNoPawn:case MidProfile::R5AllQuietRookPawn:
        case MidProfile::R6Control:case MidProfile::R6Queen:case MidProfile::R6Pawn:
        case MidProfile::R6QueenPawn:case MidProfile::R6QueenPawnQuiet:
        case MidProfile::R6QueenQuiet:case MidProfile::R6PawnQuiet:
        case MidProfile::R6QueenPawnQuietRate4:case MidProfile::R6QueenSafety:
        case MidProfile::R6PawnPressure:case MidProfile::R6QueenPiece:
        case MidProfile::R6PawnPiece:
        case MidProfile::R7Control:case MidProfile::R7Intent:case MidProfile::R7IntentQuiet:
        case MidProfile::R7IntentStrong:case MidProfile::R7Context:case MidProfile::R7ContextQuiet:
        case MidProfile::R7Mix50:case MidProfile::R7Mix75:case MidProfile::R7Adaptive:
        case MidProfile::R7AdaptiveContext:case MidProfile::R7Guard:case MidProfile::R7ContextStrong:
        case MidProfile::R7CalmGuard2:case MidProfile::R7CalmGuard5:
        case MidProfile::R7TacticalGuard2:case MidProfile::R7TacticalGuard5:
        case MidProfile::R7TailCalibrate:case MidProfile::R7QuietChoiceGuard:break;
        case MidProfile::Baseline:return;
    }
    result.additional=mid_features(p,previous,own_previous,result.base,profile);
    if(ctx.r13_mask&128){
        result.context=r4_context(p,result.base);MidResidual merged=ctx.mid;
        for(unsigned k=0;k<MID_F;++k)merged.theta[k]=int16_t(merged.theta[k]+ctx.context_residual[result.context].theta[k]);
        result.coded=mid_residual_cdf(result.coded,n,result.additional,merged);
    }else result.coded=mid_residual_cdf(result.coded,n,result.additional,ctx.mid);
    if(profile==MidProfile::AllTacticsGuard || mid_new_profile(profile))
        result.coded=mid_guard(result.coded,n,mid_guard_percent(profile));
    if(r8)result.r8_experts[1]=result.coded;
    if((ctx.r13_mask&1)&&mid_r3_plan(profile)){
        result.r3_plan=r3_plan_features(p,previous,own_previous,result.base);
        result.coded=mid_residual_cdf(result.coded,n,result.r3_plan,ctx.plan);
    }
    if(mid_r4_profile(profile)||mid_r5_profile(profile)||mid_r6_profile(profile)) {
        result.context=r4_context(p,result.base);
        if((ctx.r13_mask&2)&&!(ctx.r13_mask&128)&&r4_use_context(profile))
            result.coded=mid_residual_cdf(result.coded,n,result.additional,ctx.context_residual[result.context]);
        if((ctx.r13_mask&4)&&r4_use_rank(profile)) {
            result.r4_rank=r4_rank_features(p,result.base,result.additional);
            result.coded=mid_residual_cdf(result.coded,n,result.r4_rank,ctx.rank_residual);
        }
        if(r4_use_guard(profile)) {
            unsigned ct=ctx.uncertainty_total[result.context];
            unsigned tail=ctx.uncertainty_tail[result.context];
            // An online prior prevents overreacting to a handful of games.
            unsigned pct=ct<16?3:(tail*8>ct?6:(tail*16>ct?3:1));
            if(profile==MidProfile::R4AllMild)pct=(pct+1)/2;
            result.coded=mid_guard(result.coded,n,pct);
        }
    }
    if(r8)result.r8_experts[2]=result.coded;
    if(!(ctx.r13_mask&8))for(unsigned i=0;i<n;++i)result.pos_quiet[i]=uint8_t(!(result.base.flags[i]&(1u<<4)));
    if((ctx.r13_mask&8)&&(mid_r5_profile(profile)||mid_r6_profile(profile)) && profile!=MidProfile::R5Control){
        result.positional=r5_positional_features(p,result.base,result.pos_quiet,mid_r6_profile(profile)?MidProfile::R5AllQuietFast:profile);
        if(profile==MidProfile::R5AllQuiet || profile==MidProfile::R5AllQuietSlow ||
           profile==MidProfile::R5AllQuietFast || profile==MidProfile::R5AllQuietGuard ||
           unsigned(profile)>=63)
            result.coded=r5_quiet_cdf(result.coded,n,result.positional,result.pos_quiet,ctx.positional_residual);
        else result.coded=mid_residual_cdf(result.coded,n,result.positional,ctx.positional_residual);
        if(profile==MidProfile::R5AllQuietGuard)result.coded=mid_guard(result.coded,n,2);
    }
    if(r8)result.r8_experts[3]=result.coded;
    if((ctx.r13_mask&16)&&mid_r6_profile(profile) && profile!=MidProfile::R6Control){
        result.r6_features=r6_positional_features(p,result.base,result.pos_quiet,profile);
        if(profile==MidProfile::R6QueenPawnQuiet || profile==MidProfile::R6QueenQuiet ||
           profile==MidProfile::R6PawnQuiet || profile==MidProfile::R6QueenPawnQuietRate4 ||
           profile==MidProfile::R6QueenPiece || profile==MidProfile::R6PawnPiece)
            result.coded=r5_quiet_cdf(result.coded,n,result.r6_features,result.pos_quiet,ctx.r6_residual);
        else result.coded=mid_residual_cdf(result.coded,n,result.r6_features,ctx.r6_residual);
    }
    if(r8&&!(ctx.r13_mask&32))result.r8_experts[4]=result.coded;
    if(r8&&(ctx.r13_mask&32)){
        result.r8_experts[4]=result.coded;
        if(r8_special(r7profile) && r7profile!=MidProfile::R8SpecialControl){
            r8_special_features(p,result,r7profile);
            unsigned context=r8_context_index(r7profile,result.r8_ctx);
            for(unsigned k=0;k<3;++k)
                result.r8_experts[k+1]=r8_fused(r7profile)?result.r8_experts[4]:
                    mid_residual_cdf(result.r8_experts[4],n,result.r8_signal[k],
                        ctx.r8_specialists[context][k]);
            result.r8_experts[0]=result.r8_experts[4];
        }else if(r8_special(r7profile)){
            for(unsigned k=0;k<4;++k)result.r8_experts[k]=result.r8_experts[4];
        }
        if(unsigned(r11profile)>=245){
            if(unsigned(r11profile)>=247)result.r10_target_signal=r10_target_features(p,result,previous,MidProfile::R10ComboTact4Full,stage);
            std::array<uint64_t,256> raw{};
            for(unsigned i=0;i<n;++i){
                int score=0;
                for(unsigned k=0;k<3;++k){unsigned mask=result.r8_signal[k][i];
                    while(mask){unsigned bit=std::countr_zero(mask);mask&=mask-1;score+=ctx.compact[k].theta[bit];}}
                if(unsigned(r11profile)>=247){unsigned mask=result.r10_target_signal[i];
                    while(mask){unsigned bit=std::countr_zero(mask);mask&=mask-1;score+=ctx.r10_phase_residual[1].theta[bit];}}
                score=std::clamp(score,-128,128);
                raw[i]=uint64_t(result.r8_experts[4].weights[i])*v8_exp2_lut[score+128];
            }
            result.coded=v6_quantize(raw,n);
        }else result.coded=r8_fused(r7profile)?r8_fused_cdf(result,ctx,r7profile):r8_combine(result,ctx,r7profile);
    }
    if(mid_r7_profile(r7profile)){
        result.r7_before=result.coded;
        result.r7_calibration_context=uint8_t(r4_context(p,result.base));
        if(unsigned(r7profile)>=92){
            const unsigned context=result.r7_calibration_context;
            unsigned guard=0;
            if(r7profile==MidProfile::R7CalmGuard2&&context==0)guard=2;
            if(r7profile==MidProfile::R7CalmGuard5&&context==0)guard=5;
            if(r7profile==MidProfile::R7TacticalGuard2&&context>=2)guard=2;
            if(r7profile==MidProfile::R7TacticalGuard5&&context>=2)guard=5;
            if(r7profile==MidProfile::R7QuietChoiceGuard){
                unsigned quiet=0;
                for(unsigned i=0;i<n;++i)quiet+=result.pos_quiet[i];
                if(quiet*4>=n*3)guard=2;
            }
            if(r7profile==MidProfile::R7TailCalibrate){
                unsigned count=ctx.uncertainty_total[context],tails=ctx.uncertainty_tail[context];
                guard=(count>=16 && tails*8>count)?5:1;
            }
            if(guard)result.coded=mid_guard(result.coded,n,guard);
        }
        else if(r7profile!=MidProfile::R7Control){
            result.r7_intent=r7_intent_features(p,result.base,previous,own_previous);
            const MidResidual& model=r7_context(r7profile)?
                ctx.r7_intent_context[result.r7_calibration_context]:ctx.r7_intent;
            result.r7_expert=r7_quiet(r7profile)?
                r5_quiet_cdf(result.coded,n,result.r7_intent,result.pos_quiet,model):
                mid_residual_cdf(result.coded,n,result.r7_intent,model);
            result.coded=r7_mix(result.r7_before,result.r7_expert,n,
                                r7_alpha(r7profile,ctx,result.r7_calibration_context));
            if(r7profile==MidProfile::R7Guard)result.coded=mid_guard(result.coded,n,2);
        }
    }
    if((ctx.r13_mask&64)&&unsigned(r11profile)<245 && mid_r10_profile(r10profile) && stage==1 && r10_phase_on(r10profile,stage)==(unsigned(r10profile)>=217)){
        result.r10_before=result.coded;
        result.r10_target_signal=r10_target_features(p,result,previous,r10profile,stage);
        const auto& target_model=unsigned(r10profile)>=217?ctx.r10_phase_residual[stage]:ctx.r10_target_residual;
        auto prediction=mid_residual_cdf(result.r10_before,n,result.r10_target_signal,target_model);
        result.r10_expert=prediction;
        result.coded=r7_mix(result.r10_before,prediction,n,r10_alpha(r10profile,ctx,stage));
    }
    return;
}
MidStep mid_step_legacy(const Position& p,const Move* previous,const Move* own_previous,
                 unsigned ply,unsigned stage,MidProfile profile,const MidContext& ctx){MidStep result;mid_step_legacy_into(result,p,previous,own_previous,ply,stage,profile,ctx);return result;}
void mid_observe_legacy(MidContext&ctx,const Position&p,const MidStep&s,
                 unsigned index,unsigned ply,unsigned stage,MidProfile profile){
    const MidProfile r11profile=profile;
    if(unsigned(profile)>=245)profile=MidProfile::R10ComboTact4Full;
    const MidProfile r10profile=profile;
    if(unsigned(profile)==243||unsigned(profile)==244){
        ctx.single[stage].observe(s.additional,s.coded,s.base.moves.size,index,false,unsigned(profile)==243?2:4);
        v8_observe(ctx.baseline,p,s.base,index,ply,V8Model::RelationalJoint);
        return;
    }
    if(mid_r10_profile(profile))profile=MidProfile::R9MixSharp;
    const MidProfile r7profile=profile;
    profile=r7_base(profile);
    if((ctx.r13_mask&64)&&unsigned(r11profile)<245 && mid_r10_profile(r10profile) && stage==1 && (unsigned(r10profile)<217 || r10_phase_on(r10profile,stage))){
        // Learn candidate calibration from the already-coded distribution.
        if((unsigned(r10profile)>=209 && unsigned(r10profile)<217) || r10profile==MidProfile::R10ComboTact2Gate || r10profile==MidProfile::R10ComboTact4Gate){
           const int64_t left=int64_t(s.r10_expert.weights[index])*s.r10_before.total;
           const int64_t right=int64_t(s.r10_before.weights[index])*s.r10_expert.total;
           int change=int(std::clamp<int64_t>((left-right)*64/std::max<int64_t>(1,right),-48,48));
           ctx.r10_advantage=std::clamp((ctx.r10_advantage*127)/128+change,-600,600);
           ++ctx.r10_seen;
        }
        auto& target_model=unsigned(r10profile)>=217?ctx.r10_phase_residual[stage]:ctx.r10_target_residual;
        target_model.observe(s.r10_target_signal,s.coded,s.base.moves.size,index,false,r10_rate(r10profile,stage));
    }
    if(stage==0 && r9_phase_open(r7profile)){
        auto ref=s.coded;
        if(r7profile==MidProfile::R9BothSelf4 || r7profile==MidProfile::R9BothSelf6)
            ref=mid_residual_cdf(s.base.coded,s.base.moves.size,s.r9_phase_signal,ctx.r9_open_residual);
        ctx.r9_open_residual.observe(s.r9_phase_signal,ref,s.base.moves.size,index,false,r9_phase_rate(r7profile,true));
    }
    if(stage==2 && r9_phase_end(r7profile)){
        auto ref=s.coded;
        if(r7profile==MidProfile::R9BothSelf4 || r7profile==MidProfile::R9BothSelf6)
            ref=mid_residual_cdf(s.base.coded,s.base.moves.size,s.r9_phase_signal,ctx.r9_end_residual);
        ctx.r9_end_residual.observe(s.r9_phase_signal,ref,s.base.moves.size,index,false,r9_phase_rate(r7profile,false));
    }
    if(mid_r10_profile(r10profile) && stage!=1 && r10_phase_on(r10profile,stage)){
        ctx.r10_phase_residual[stage].observe(s.r10_target_signal,s.coded,s.base.moves.size,index,false,r10_rate(r10profile,stage));
    }
    if(unsigned(r11profile)>=245 && stage==1){
        if(unsigned(r11profile)>=247)ctx.r10_phase_residual[1].observe(s.r10_target_signal,s.coded,s.base.moves.size,index,false,unsigned(r11profile)==247?2:4);
        for(unsigned k=0;k<3;++k)ctx.compact[k].observe(s.r8_signal[k],s.coded,s.base.moves.size,index,false,(unsigned(r11profile)==245||unsigned(r11profile)==247)?2:4);
    }
    if((ctx.r13_mask&32)&&unsigned(r11profile)<245 && (mid_r8_profile(r7profile)||mid_r9_profile(r7profile)) && stage==1){
        r8_update(ctx,s,index,r7profile);
        if(r8_special(r7profile) && r7profile!=MidProfile::R8SpecialControl){
            const unsigned context=r8_context_index(r7profile,s.r8_ctx);
            const auto prior=r8_prior(r7profile);
            for(unsigned k=0;k<3;++k)if(prior[k+1])
            {
                const bool self=(r7profile==MidProfile::R9TPSelf2 || r7profile==MidProfile::R9TPSelf3 ||
                    r7profile==MidProfile::R9TPSelf4 || r7profile==MidProfile::R9TPSelfBlend3 ||
                    r7profile==MidProfile::R9TPSelfBetterTactics ||
                    r7profile==MidProfile::R9MixSelf3 || r7profile==MidProfile::R9MixSelf4);
                V6Weights expert;
                if(self){
                    expert=mid_residual_cdf(s.r8_experts[4],s.base.moves.size,
                        s.r8_signal[k],ctx.r8_specialists[context][k]);
                    if(r7profile==MidProfile::R9TPSelfBlend3){
                        std::array<uint64_t,256> weights{};
                        for(unsigned j=0;j<s.base.moves.size;++j)
                            weights[j]=uint64_t(expert.weights[j])*s.r8_experts[4].total+
                                uint64_t(s.r8_experts[4].weights[j])*expert.total;
                        expert=v6_quantize(weights,s.base.moves.size);
                    }
                }
                ctx.r8_specialists[context][k].observe(s.r8_signal[k],
                    self?expert:(r8_fused(r7profile)?s.r8_experts[4]:s.r8_experts[k+1]),
                    s.base.moves.size,index,false,
                    r7profile==MidProfile::R9Rate1 || r7profile==MidProfile::R9Rate1Base20 ||
                    r7profile==MidProfile::R9TPRate1 || r7profile==MidProfile::R9MixRate1 ? 1:
                    r7profile==MidProfile::R9Rate3 || r7profile==MidProfile::R9Rate3Base20 ||
                    r7profile==MidProfile::R9Rate3Piece50 || r7profile==MidProfile::R9TPRate3 ||
                    r7profile==MidProfile::R9TPSelf3 || r7profile==MidProfile::R9TPSelfBlend3 ||
                    r7profile==MidProfile::R9TPBetterTacticsRate3 || r7profile==MidProfile::R9MixRate3 ||
                    r7profile==MidProfile::R9MixSelf3 ? 3:
                    r7profile==MidProfile::R9Rate4 || r7profile==MidProfile::R9Rate4Base20 ||
                    r7profile==MidProfile::R9TPRate4 || r7profile==MidProfile::R9TPSelf4 ||
                    r7profile==MidProfile::R9MixRate4 || r7profile==MidProfile::R9MixSelf4 ? 4:
                    r7profile==MidProfile::R9Rate6 || r7profile==MidProfile::R9MixRate6 ? 6:2);
            }
        }
    }
    if(mid_r7_profile(r7profile) && stage==1 && r7profile!=MidProfile::R7Control){
        const unsigned context=s.r7_calibration_context;
        const unsigned rate=(r7profile==MidProfile::R7IntentStrong||
            r7profile==MidProfile::R7ContextStrong)?4:2;
        MidResidual& r=r7_context(r7profile)?ctx.r7_intent_context[context]:ctx.r7_intent;
        if(r7_quiet(r7profile))r.observe_quiet(s.r7_intent,s.coded,s.pos_quiet,s.base.moves.size,index,rate);
        else r.observe(s.r7_intent,s.coded,s.base.moves.size,index,false,rate);
        if(r7_adaptive(r7profile)){
            // Reward improved assigned probability; evidence is pre-observation.
            const int64_t newp=int64_t(s.r7_expert.weights[index])*s.r7_before.total;
            const int64_t oldp=int64_t(s.r7_before.weights[index])*s.r7_expert.total;
            const int64_t denom=std::max<int64_t>(newp,oldp);
            int delta=denom?int(std::clamp<int64_t>((newp-oldp)*16/denom,-16,16)):0;
            auto& cal=ctx.r7_calibration[context];
            cal=int16_t(std::clamp(int(cal)+delta,-45,45));
            auto& seen=ctx.r7_calibration_seen[context];
            if(seen<60000)++seen;
            if((seen&31u)==0){if(cal>0)--cal;else if(cal<0)++cal;}
        }
    }

    if(stage==0 && mid_opening_enabled(profile))
        ctx.opening.observe(s.additional,s.coded,s.base.moves.size,index,false,
                            mid_fixed_rate(profile)?mid_fixed_rate(profile):2);
    if(stage==1){
        if(profile==MidProfile::TemperatureMixture){
            uint64_t a=uint64_t(65536-ctx.alt)*s.base.coded.weights[index]*s.flat.total;
            uint64_t b=uint64_t(ctx.alt)*s.flat.weights[index]*s.base.coded.total;
            if(a+b){ctx.alt=uint32_t((b*65536)/(a+b));ctx.alt=(ctx.alt*63u+32768u)/64u;}
        }
        if((profile>=MidProfile::SEE && profile<=MidProfile::AllFast) ||
           (mid_new_profile(profile)&&mid_mid_enabled(profile)))
            ctx.mid.observe(s.additional,s.coded,s.base.moves.size,index,
                            profile==MidProfile::AdaptiveResidual||profile==MidProfile::AllGuardAdaptive,
                            mid_fixed_rate(profile));
    }
    if((ctx.r13_mask&1)&&mid_r3_plan(profile) && stage==1)
        ctx.plan.observe(s.r3_plan,s.coded,s.base.moves.size,index,false,
            profile==MidProfile::R3PlanRate4 || profile==MidProfile::R3BothRate4 || (profile==MidProfile::R3BothStrongRate4 || mid_r4_profile(profile) || mid_r5_profile(profile) || mid_r6_profile(profile))?4:2);
    if(mid_r3_open_position(profile) && stage==0)
        ctx.open_store.observe(p,s.base.moves.a[index].m);
    if((mid_r4_profile(profile)||mid_r5_profile(profile)||mid_r6_profile(profile)) && stage==1) {
        if((ctx.r13_mask&2)&&r4_use_context(profile))ctx.context_residual[s.context].observe(
            s.additional,s.coded,s.base.moves.size,index,false,
            profile==MidProfile::R4AllMild?1:2);
        if((ctx.r13_mask&4)&&r4_use_rank(profile))ctx.rank_residual.observe(
            s.r4_rank,s.coded,s.base.moves.size,index,false,
            profile==MidProfile::R4AllMild?1:2);
        uint32_t& count=ctx.uncertainty_total[s.context];
        uint32_t& tail=ctx.uncertainty_tail[s.context];
        if(count>=4096){count=(count+1)/2;tail=(tail+1)/2;}
        ++count;
        if(uint64_t(s.coded.weights[index])*64 < s.coded.total)++tail;
    }
    if((ctx.r13_mask&8)&&(mid_r5_profile(profile)||mid_r6_profile(profile)) && stage==1 && profile!=MidProfile::R5Control){
        unsigned rate=profile==MidProfile::R5AllQuietSlow?1:
                      (profile==MidProfile::R5AllQuietFast||profile==MidProfile::R5RookQuietFast||mid_r6_profile(profile))?4:2;
        if(profile==MidProfile::R5AllQuiet||profile==MidProfile::R5AllQuietSlow||
           profile==MidProfile::R5AllQuietFast||profile==MidProfile::R5AllQuietGuard||
           unsigned(profile)>=63)
            ctx.positional_residual.observe_quiet(s.positional,s.coded,s.pos_quiet,
                           s.base.moves.size,index,rate);
        else ctx.positional_residual.observe(s.positional,s.coded,s.base.moves.size,index,false,rate);
    }
    if((ctx.r13_mask&16)&&mid_r6_profile(profile) && stage==1 && profile!=MidProfile::R6Control){
        unsigned rate=profile==MidProfile::R6QueenPawnQuietRate4?4:2;
        if(profile==MidProfile::R6QueenPawnQuiet || profile==MidProfile::R6QueenQuiet ||
           profile==MidProfile::R6PawnQuiet || profile==MidProfile::R6QueenPawnQuietRate4 ||
           profile==MidProfile::R6QueenPiece || profile==MidProfile::R6PawnPiece)
            ctx.r6_residual.observe_quiet(s.r6_features,s.coded,s.pos_quiet,s.base.moves.size,index,rate);
        else ctx.r6_residual.observe(s.r6_features,s.coded,s.base.moves.size,index,false,rate);
    }
    v8_observe(ctx.baseline,p,s.base,index,ply,V8Model::RelationalJoint);
}
// Candidate interaction model: five small categorical feature families.
// Context is observable before the chosen move. One shared residual distribution.
void mid_step_into(MidStep& result,const Position& p,const Move* previous,const Move* own_previous,
                 unsigned ply,unsigned stage,MidProfile profile,const MidContext& ctx){
    if(profile!=MidProfile::R12Configurable&&profile!=MidProfile::R12JointAll&&profile!=MidProfile::R13Structure&&profile!=MidProfile::R14Structure&&profile!=MidProfile::R15PawnSplit&&profile!=MidProfile::R16SourceModel){mid_step_legacy_into(result,p,previous,own_previous,ply,stage,profile,ctx);return;}
    mid_step_legacy_into(result,p,previous,own_previous,ply,stage,ctx.config.base?MidProfile::R11Joint40Rate2:MidProfile::R10ComboTact4Full,ctx);
    if(!(ctx.config.phase_mask&(1u<<stage)))return;
    const unsigned n=result.base.moves.size;const auto& model=ctx.r12[stage];
    detail::R14PawnFiles pawn_files{};
    const unsigned structural=ctx.r14_active&&stage==1?ctx.r14.features:0;
    if(structural&3)pawn_files=detail::r14_pawn_files(p);
    uint32_t capture_mass=0;
    if(ctx.r16_active&&stage==1&&(ctx.r16.layout==1||ctx.r16.layout==5))for(unsigned i=0;i<n;++i)if(result.base.flags[i]&(1u<<4))capture_mass+=result.coded.weights[i];
    const unsigned source_context=unsigned(uint64_t(capture_mass)*2>result.coded.total);
    std::array<uint64_t,256> raw; // All entries [0,n) are assigned before quantization.
    for(unsigned i=0;i<n;++i){
        auto m=result.base.moves.a[i].m;const unsigned piece=unsigned(abs_piece(p.board[m.from])-1);
        const unsigned rank=p.side?7-(m.to>>3):(m.to>>3);
        const int x=m.to&7,y=m.to>>3;
        const unsigned center=unsigned(std::min(3,std::min(std::abs(x-3),std::abs(x-4))+std::min(std::abs(y-3),std::abs(y-4))));
        const unsigned distance=unsigned(std::max(std::abs(x-int(m.from&7)),std::abs(y-int(m.from>>3))));
        const auto f=result.base.flags[i];
        auto& keys=result.r12_features[i];
        keys={uint8_t(piece*8+rank),uint8_t(48+piece*4+center),
              uint8_t(72+piece*4+unsigned(bool(f&(1u<<2)))+2*unsigned(bool(f&(1u<<3)))),
              uint8_t(96+piece*8+distance),
              uint8_t(144+piece*4+unsigned(bool(f&(1u<<4)))+2*unsigned(bool(f&(1u<<8))))};
        // Replace the existing center/distance categorical slots, not another model.
        if((structural&3)&&(piece==0||piece==3)&&((structural&4)||!(f&(1u<<4)))&&!(ctx.r15_active&&piece==0&&ctx.r15.mode)){
            const auto categories=detail::r14_categories(p,m,pawn_files);
            if(structural&1)keys[1]=uint8_t(48+piece*4+categories[0]);
            if(structural&2)keys[3]=uint8_t(96+piece*8+categories[1]);
        }
        if(ctx.r15_active&&stage==1&&piece==0&&ctx.r15.mode){
            auto categories=detail::r15_categories(p,m,pawn_files,ctx.r15.mode,bool(f&(1u<<4)));
            if(ctx.r15.mode>=7&&(f&(1u<<4))){keys[1]=uint8_t(detail::r15_center_bank[categories[0]]);keys[3]=uint8_t(detail::r15_distance_bank[categories[1]]);}
            else {keys[1]=uint8_t(48+categories[0]);keys[3]=uint8_t(96+categories[1]);}
        }
        if(ctx.r16_active&&stage==1&&ctx.r16.layout)
            keys=detail::r16_keys(p,m,keys,ctx.r16.layout,source_context,bool(f&(1u<<4)),bool(f&(1u<<2)),bool(f&(1u<<3)));
        int score=0;for(unsigned k=0;k<5;++k){if(ctx.config.families&(1u<<k))score+=model.theta[keys[k]];else keys[k]=255;}
        score=std::clamp(score,-128,128);raw[i]=uint64_t(result.coded.weights[i])*v8_exp2_lut[score+128];
    }
    auto calibrated=v6_quantize(raw,n);
    result.coded=r7_mix(result.coded,calibrated,n,ctx.config.alpha);
    return;
}
MidStep mid_step(const Position& p,const Move* previous,const Move* own_previous,
                 unsigned ply,unsigned stage,MidProfile profile,const MidContext& ctx){MidStep result;mid_step_into(result,p,previous,own_previous,ply,stage,profile,ctx);return result;}
void mid_observe(MidContext&ctx,const Position&p,const MidStep&s,unsigned index,unsigned ply,unsigned stage,MidProfile profile){
    if(profile!=MidProfile::R12Configurable&&profile!=MidProfile::R12JointAll&&profile!=MidProfile::R13Structure&&profile!=MidProfile::R14Structure&&profile!=MidProfile::R15PawnSplit&&profile!=MidProfile::R16SourceModel){mid_observe_legacy(ctx,p,s,index,ply,stage,profile);return;}
    if(ctx.config.phase_mask&(1u<<stage)){
        auto& model=ctx.r12[stage];std::array<uint32_t,168> sums{};
#ifdef DMT_ACTIVE_CATEGORY_UPDATES
        std::array<uint64_t,3> active{};
#endif
        for(unsigned i=0;i<s.base.moves.size;++i)for(auto key:s.r12_features[i])if(key!=255){
            sums[key]+=s.coded.weights[i];
#ifdef DMT_ACTIVE_CATEGORY_UPDATES
            active[key>>6]|=uint64_t(1)<<(key&63);
#endif
        }
        std::array<bool,168> actual{};for(auto key:s.r12_features[index])if(key!=255)actual[key]=true;
        const int base_rate=ctx.r14_active&&stage==1?ctx.r14.rate:ctx.config.rate;
        const int cap=ctx.r16_active&&stage==1?ctx.r16.cap:ctx.config.cap;
        const bool separate_rate=ctx.r15_active&&stage==1&&ctx.r15.capture_rate!=base_rate;
        auto apply=[&](unsigned k,int32_t expected,int rate){
            model.residual[k]+=rate*((actual[k]?4096:0)-expected);
            int delta=model.residual[k]/4096;model.residual[k]-=delta*4096;
            model.theta[k]=int16_t(std::clamp(int(model.theta[k])+delta,-cap,cap));
        };
#ifdef DMT_U32_CATEGORY_UPDATES
        if(!separate_rate&&s.coded.total>1){
            const uint32_t total=s.coded.total,reciprocal=uint32_t((uint64_t(1)<<32)/total);
            for(unsigned k=0;k<168;++k){
                // At most five features per candidate and total<32768 ensure
                // sums[k]*4096+total/2 fits uint32, even with duplicate keys.
                uint32_t numerator=sums[k]*4096u+total/2;
                apply(k,int32_t(detail::r16_divide_u32(numerator,total,reciprocal)),base_rate);
            }
        }else
#endif
        {
            const ExactDivisor divide(s.coded.total);
            auto update=[&](unsigned k){
                const int32_t expected=int32_t(divide(uint64_t(sums[k])*4096+s.coded.total/2));
                int rate=base_rate;if(separate_rate&&detail::r15_capture_key(ctx.r15.mode,k))rate=ctx.r15.capture_rate;
                apply(k,expected,rate);
            };
#ifdef DMT_ACTIVE_CATEGORY_UPDATES
            for(unsigned word=0;word<3;++word){auto bits=active[word];while(bits){unsigned k=word*64+std::countr_zero(bits);bits&=bits-1;update(k);}}
#else
            for(unsigned k=0;k<168;++k){
#ifndef DMT_DENSE_CATEGORY_UPDATES
                if(!sums[k])continue;
#endif
                update(k);
            }
#endif
        }
        if((++model.steps&((1u<<ctx.config.decay_log2)-1))==0)for(auto& t:model.theta){if(t>0)--t;else if(t<0)++t;}
    }
    mid_observe_legacy(ctx,p,s,index,ply,stage,ctx.config.base?MidProfile::R11Joint40Rate2:MidProfile::R10ComboTact4Full);
}
void mid_record(MidDiagnostics* diag,unsigned stage,const MidStep&s,unsigned ix){
    if(!diag)return;
    if(stage==1 && s.r8_active){
        const unsigned context=s.r8_ctx;
        ++diag->r8_context_count[context];
        for(unsigned k=0;k<5;++k){
            const auto& expert=s.r8_experts[k];
            double bits=std::log2(double(expert.total)/expert.weights[ix]);
            diag->r8_expert_loss[k]+=bits;
            diag->r8_context_loss[context][k]+=bits;
        }
    }
    auto&r=diag->phase[stage];++r.moves;
    double bits=std::log2(double(s.coded.total)/s.coded.weights[ix]);r.logloss+=bits;
    uint32_t source_mass=0;for(unsigned i=0;i<s.base.moves.size;++i)if(s.base.moves.a[i].m.from==s.base.moves.a[ix].m.from)source_mass+=s.coded.weights[i];
    r.source_logloss+=std::log2(double(s.coded.total)/source_mass);
    if(uint64_t(s.coded.weights[ix])*64<s.coded.total){++r.tails;r.tailbits+=bits;}
    unsigned top=0;
    for(unsigned i=1;i<s.base.moves.size;++i)if(s.coded.weights[i]>s.coded.weights[top])top=i;
    if(ix==top)++r.top1;
    for(unsigned k=0;k<MID_F;++k){
        if(s.additional[ix]&(1u<<k))++r.feature_selected[k];
        for(unsigned j=0;j<s.base.moves.size;++j)
            if(s.additional[j]&(1u<<k))++r.feature_candidates[k];
    }
}
const char* mid_names[]={"baseline","flat-mild","flat-strong","sharp","guard5","guard12",
"temp-mixture","temp-conditional","see","new-threat","own-history","forced-response",
"adaptive-residual","all-tactics","all-tactics-guard","see-proxy","all-fast",
"all-guard2","all-guard10","all-guard-adaptive","all-guard-rate1",
"all-guard-rate3","all-guard-rate4","all-guard-rate6",
"opening-only","opening-guard","opening-mid","opening-mid-guard",
"opening-mid-guard-rate3","opening-mid-guard-rate4",
"mid-see-value","opening-mid-see-value","opening-mid-see-value-rate4",
"r3-control","r3-opening-position","r3-mid-plan","r3-both","r3-opening-weak","r3-plan-rate4","r3-both-rate4",
"r3-opening-strong","r3-both-strong","r3-both-strong-rate4",
"r3-opening-aggressive","r3-both-aggressive",
"r4-control","r4-context","r4-rank","r4-guard", "r4-context-rank",
"r4-context-guard","r4-rank-guard","r4-all","r4-all-mild",
"r5-control","r5-pawn","r5-minor","r5-rook","r5-all","r5-all-quiet",
"r5-all-quiet-slow","r5-all-quiet-fast","r5-all-quiet-guard",
"r5-rook-quiet","r5-rook-piece","r5-rook-quiet-fast","r5-quiet-no-pawn","r5-quiet-rook-pawn",
"r6-control","r6-queen","r6-pawn","r6-queen-pawn","r6-queen-pawn-quiet",
"r6-queen-quiet","r6-pawn-quiet","r6-queen-pawn-quiet-rate4","r6-queen-safety","r6-pawn-pressure","r6-queen-piece","r6-pawn-piece",
"r7-control","r7-intent","r7-intent-quiet","r7-intent-strong","r7-context",
"r7-context-quiet","r7-mix50","r7-mix75","r7-adaptive","r7-adaptive-context",
"r7-guard","r7-context-strong", "r7-calm-guard2","r7-calm-guard5",
"r7-tactical-guard2","r7-tactical-guard5","r7-tail-calibrate","r7-quiet-choice-guard",
"r8-control","r8-uniform","r8-global","r8-context","r8-context-fast",
"r8-context-slow","r8-context-tactical","r8-context-heavy-final","r8-context-two",
"r8-switch-global","r8-switch-context","r8-switch-conservative",
"r8-switch-two","r8-switch-quick",
"r8-special-control","r8-special-uniform","r8-special-global",
"r8-special-context","r8-special-piece","r8-special-tactical","r8-special-plan",
"r8-fused-uniform","r8-fused-global","r8-fused-context",
"r8-fused-tactical-piece","r8-fused-tactical-plan","r8-fused-piece-plan",
"r9-rate1","r9-rate3","r9-rate4","r9-rate6","r9-base20","r9-base40",
"r9-tactical50","r9-piece50","r9-plan50","r9-tactical-piece",
"r9-base-piece","r9-rate3-base20","r9-rate1-base20","r9-rate4-base20",
"r9-rate3-piece50","r9-sharp","r9-damp",
"r9-tp-rate1","r9-tp-rate3","r9-tp-rate4","r9-tp-sharp","r9-tp-damp",
"r9-tp-self2","r9-tp-self3","r9-tp-self4","r9-tp-selfblend3",
"r9-tp-better-tactics","r9-tp-featuremix","r9-tp-better-tactics-rate3",
"r9-tp-self-better-tactics","r9-tp-alternative-piece","r9-tp-all-new-features",
"r9-phase-open1","r9-phase-open2","r9-phase-open4",
"r9-phase-end1","r9-phase-end2","r9-phase-end4",
"r9-phase-both1","r9-phase-both2","r9-phase-both4",
"r9-phase-open-mix","r9-phase-both-mix",
"r9-both-rate6","r9-both-rate8","r9-open8-end4","r9-open4-end8",
"r9-both-alt2","r9-both-alt4","r9-both-alt6","r9-both-alt8",
"r9-both-self4","r9-both-self6",
"r9-mix-rate1","r9-mix-rate3","r9-mix-rate4","r9-mix-rate6",
"r9-mix-sharp","r9-mix-damp","r9-mix-tact65","r9-mix-piece65",
"r9-mix-tact35","r9-mix-self3","r9-mix-self4",
"r10-geom2-mix","r10-geom4-mix","r10-geom2-full","r10-geom4-full",
"r10-tact2-mix","r10-tact4-mix","r10-tact2-full","r10-tact4-full",
"r10-hybrid2-mix","r10-hybrid4-mix","r10-hybrid2-full","r10-hybrid4-full",
"r10-tact1-full","r10-tact3-full","r10-tact1-mix75","r10-tact3-mix75",
"r10-tact2-mix25","r10-tact4-mix25","r10-tact2-mix75","r10-tact4-mix75",
"r10-gate-tact2","r10-gate-tact4","r10-gate-tact2-mild","r10-gate-tact4-mild",
"r10-gate-geom2","r10-gate-geom4","r10-gate-geom2-mild","r10-gate-geom4-mild",
"r10-open-geom2","r10-end-geom2","r10-phase-geom2","r10-phase-geom4",
"r10-phase-tact2","r10-phase-tact4","r10-phase-hybrid2","r10-phase-hybrid4",
"r10-phase-tact2-full","r10-phase-tact4-full","r10-phase-geom2-full","r10-phase-geom4-full",
"r10-all-tact2","r10-all-geom2",
"r10-combo-tact2-mix","r10-combo-tact4-mix","r10-combo-tact2-full","r10-combo-tact4-full",
"r10-combo-tact2-gate","r10-combo-tact4-gate","r10-combo-tact1-mix75","r10-combo-tact2-mix25",
"r10-combo-tact2-mix75","r10-combo-geom2-mix","r10-combo-hybrid2-mix","r10-combo-tact3-full","r11-single-rate2","r11-single-rate4","r11-joint30-rate2","r11-joint30-rate4","r11-joint40-rate2","r11-joint40-rate4","r12-configurable","r12-joint-all","r13-structure","r14-structural","r15-pawn-split","r16-adaptive"};
} // anonymous
const char* mid_name(MidProfile p){unsigned i=unsigned(p);return i<MID_PROFILE_COUNT?mid_names[i]:"invalid";}
static V6Archive mid_encode_impl(const std::vector<std::vector<Move>>& games,MidProfile profile,MidDiagnostics* diag,std::vector<MidTrace>* trace,const Dmt12Config* configuration,const uint8_t* structure=nullptr,const Dmt14Config* structural=nullptr,const Dmt15Config* pawn_split=nullptr,const Dmt16Config* source_model=nullptr){
    if(unsigned(profile)>=MID_PROFILE_COUNT)fail("bad DMT profile");
    if(games.size()>100000)fail("too many DMT1 games");
    if(diag)*diag={};
    if(trace)trace->clear();
    ArithmeticEncoder coder;MidContext ctx;
    if(configuration)ctx.config=*configuration;
    if(profile==MidProfile::R12JointAll||profile==MidProfile::R13Structure||profile==MidProfile::R14Structure||profile==MidProfile::R15PawnSplit||profile==MidProfile::R16SourceModel)ctx.config=R12_FIXED;
    if(profile==MidProfile::R13Structure){ctx.r13_mask=structure?*structure:127;if((ctx.r13_mask&128)&&!(ctx.r13_mask&2))fail("merge requires context");}
    if(profile==MidProfile::R12Configurable||profile==MidProfile::R12JointAll)r12_validate(ctx.config);
    if(profile==MidProfile::R14Structure){
        ctx.r14_active=true;ctx.r14=structural?*structural:Dmt14Config{};
        if(ctx.r14.features>7||!ctx.r14.rate||ctx.r14.rate>16||((ctx.r14.structure&128)&&!(ctx.r14.structure&2)))fail("invalid DMT14 config");
        ctx.r13_mask=ctx.r14.structure;
    }
    if(profile==MidProfile::R15PawnSplit){
        ctx.r15_active=true;ctx.r15=pawn_split?*pawn_split:Dmt15Config{};
        if(ctx.r15.mode>9||!ctx.r15.rate||ctx.r15.rate>16||!ctx.r15.capture_rate||ctx.r15.capture_rate>16)fail("invalid DMT15 config");
        ctx.r14_active=true;ctx.r14={95,7,ctx.r15.rate};ctx.r13_mask=95;
    }
    if(profile==MidProfile::R16SourceModel){
        ctx.r16_active=true;ctx.r16=source_model?*source_model:Dmt16Config{};
        if(ctx.r16.layout>5||!ctx.r16.rate||ctx.r16.rate>16||!ctx.r16.cap||ctx.r16.cap>128)fail("invalid DMT16 config");
        ctx.r14_active=true;ctx.r14={95,7,ctx.r16.rate};ctx.r13_mask=95;
    }
    uint64_t total=0;
    for(unsigned game_index=0;game_index<games.size();++game_index){
        const auto& game=games[game_index];
        if(game.size()>1000000||(total+=game.size())>300000000ULL)fail("bad DMT1 game length");
        Position p=Position::standard();Move previous{},own[2]{};bool has_prev=false,own_valid[2]={false,false};
        for(unsigned ply=0;ply<game.size();++ply){
            unsigned stage=V7Patterns::phase(p,ply);
            int side=p.side;
            MidStep s=mid_step(p,has_prev?&previous:nullptr,own_valid[side]?&own[side]:nullptr,ply,stage,profile,ctx);
            unsigned ix=index_of(s.base.moves,game[ply]);
            mid_record(diag,stage,s,ix);
            if(trace){
                unsigned rank=1;
                for(unsigned ci=0;ci<s.base.moves.size;++ci)
                    if(s.coded.weights[ci]>s.coded.weights[ix])++rank;
                trace->push_back(MidTrace{game_index,ply,s.base.moves.size,
                    uint8_t(stage),uint8_t(stage==1?r4_context(p,s.base):0),
                    uint8_t(bool(s.base.flags[ix]&(1u<<4))),
                    uint8_t(p.square_attacked(p.king_square(p.side),p.side^1)),
                    uint16_t(rank),uint16_t(s.coded.weights[ix]),s.additional[ix],
                    std::log2(double(s.coded.total)/s.coded.weights[ix]),game[ply]});
            }
            coder.put(v6_before(s.coded,ix),s.coded.weights[ix],s.coded.total);
            mid_observe(ctx,p,s,ix,ply,stage,profile);
            p.apply_unchecked(game[ply]); // index_of selected a fully legal candidate
            own[side]=game[ply];own_valid[side]=true;previous=game[ply];has_prev=true;
        }
    }
    auto payload=total?coder.finish():EntropyPayload{};
    std::vector<uint8_t> bytes={'D','M','T',uint8_t(unsigned(profile)==254?'H':unsigned(profile)==253?'G':unsigned(profile)==252?'F':unsigned(profile)==251?'E':unsigned(profile)==250?'D':unsigned(profile)==249?'C':unsigned(profile)>=243?'B':mid_r10_profile(profile)?'A':(mid_r9_profile(profile)?'9':(mid_r8_profile(profile)?'8':(mid_r7_profile(profile)?'7':(mid_r6_profile(profile)?'6':(mid_r5_profile(profile)?'5':(mid_r4_profile(profile)?'4':(mid_r3_profile(profile)?'3':(mid_new_profile(profile)?'2':'1'))))))))),1,uint8_t(profile)};
    if(profile==MidProfile::R12Configurable){const auto& c=ctx.config;
        for(auto value:{c.families,c.rate,c.alpha,c.phase_mask,c.base,c.decay_log2,c.cap})bytes.push_back(value);}
    if(profile==MidProfile::R13Structure)bytes.push_back(ctx.r13_mask);
    if(profile==MidProfile::R14Structure)for(auto value:{ctx.r14.structure,ctx.r14.features,ctx.r14.rate})bytes.push_back(value);
    if(profile==MidProfile::R15PawnSplit)for(auto value:{ctx.r15.mode,ctx.r15.rate,ctx.r15.capture_rate})bytes.push_back(value);
    if(profile==MidProfile::R16SourceModel)for(auto value:{ctx.r16.layout,ctx.r16.rate,ctx.r16.cap})bytes.push_back(value);
    v6_varint(bytes,uint32_t(games.size()));
    for(const auto&g:games)v6_varint(bytes,uint32_t(g.size()));
    v6_varint(bytes,payload.bit_count);
    uint64_t framing=bytes.size()+4;
    bytes.insert(bytes.end(),payload.bytes.begin(),payload.bytes.end());
    uint32_t crc=v6_crc(bytes.data(),bytes.size());
    for(unsigned j=0;j<4;++j)bytes.push_back(uint8_t(crc>>(8*j)));
    return {std::move(bytes),payload.bit_count,framing};
}
V6Archive mid_encode(const std::vector<std::vector<Move>>& games,MidProfile profile,MidDiagnostics* diag,std::vector<MidTrace>* trace){return mid_encode_impl(games,profile,diag,trace,nullptr);}
V6Archive mid_encode_experimental(const std::vector<std::vector<Move>>& games,const Dmt12Config& config,MidDiagnostics* diag){return mid_encode_impl(games,MidProfile::R12Configurable,diag,nullptr,&config);}
V6Archive mid_encode_structure(const std::vector<std::vector<Move>>& games,uint8_t mask,MidDiagnostics* diag,std::vector<MidTrace>* trace){return mid_encode_impl(games,MidProfile::R13Structure,diag,trace,nullptr,&mask);}
V6Archive mid_encode_structural(const std::vector<std::vector<Move>>& games,const Dmt14Config& config,MidDiagnostics* diag,std::vector<MidTrace>* trace){return mid_encode_impl(games,MidProfile::R14Structure,diag,trace,nullptr,nullptr,&config);}
V6Archive mid_encode_pawn_split(const std::vector<std::vector<Move>>& games,const Dmt15Config& config,MidDiagnostics* diag,std::vector<MidTrace>* trace){return mid_encode_impl(games,MidProfile::R15PawnSplit,diag,trace,nullptr,nullptr,nullptr,&config);}
V6Archive mid_encode_source_model(const std::vector<std::vector<Move>>& games,const Dmt16Config& config,MidDiagnostics* diag,std::vector<MidTrace>* trace){return mid_encode_impl(games,MidProfile::R16SourceModel,diag,trace,nullptr,nullptr,nullptr,nullptr,&config);}
std::vector<std::vector<Move>> mid_decode(const std::vector<uint8_t>&bytes){
    if(bytes.size()<10||bytes[0]!='D'||bytes[1]!='M'||bytes[2]!='T'||bytes[4]!=1)
        fail("bad DMT header");
    const unsigned id=bytes[5];
    const char version=char(bytes[3]);
    const bool valid=(version=='1' && id<MID_V1_COUNT) ||
        (version=='2' && id>=MID_V1_COUNT && id<33) ||
        (version=='3' && id>=33 && id<45) ||
        (version=='4' && id>=45 && id<54) ||
        (version=='5' && id>=54 && id<68) ||
        (version=='6' && id>=68 && id<80) ||
        (version=='7' && id>=80 && id<98) ||
        (version=='8' && id>=98 && id<125) ||
        (version=='9' && id>=125 && id<189) ||
        (version=='A' && id>=189 && id<243) ||
        (version=='B' && id>=243 && id<249) || (version=='C' && id==249) || (version=='D' && id==250) || (version=='E' && id==251) || (version=='F' && id==252) || (version=='G' && id==253) || (version=='H' && id==254);
    if(!valid)fail("bad DMT header");
    uint32_t crc=v6_crc(bytes.data(),bytes.size()-4),given=0;
    for(unsigned k=0;k<4;++k)given|=uint32_t(bytes[bytes.size()-4+k])<<(8*k);
    if(crc!=given)fail("DMT1 CRC mismatch");
    MidProfile profile=MidProfile(bytes[5]);
    size_t off=6;Dmt12Config configuration;
    if(profile==MidProfile::R12Configurable){
        if(bytes.size()<17)fail("short DMT12 header");
        configuration={bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12]};
        r12_validate(configuration);off=13;
    }
    uint8_t structure=127;
    if(profile==MidProfile::R12JointAll||profile==MidProfile::R13Structure||profile==MidProfile::R14Structure||profile==MidProfile::R15PawnSplit||profile==MidProfile::R16SourceModel)configuration=R12_FIXED;
    if(profile==MidProfile::R13Structure){if(bytes.size()<12)fail("short DMT13 header");structure=bytes[6];off=7;if((structure&128)&&!(structure&2))fail("merge requires context");}
    Dmt14Config structural;
    if(profile==MidProfile::R14Structure){
        if(bytes.size()<14)fail("short DMT14 header");
        structural={bytes[6],bytes[7],bytes[8]};
        if(structural.features>7||!structural.rate||structural.rate>16||((structural.structure&128)&&!(structural.structure&2)))fail("invalid DMT14 config");
        structure=structural.structure;off=9;
    }
    Dmt15Config split;
    if(profile==MidProfile::R15PawnSplit){
        if(bytes.size()<14)fail("short DMT15 header");split={bytes[6],bytes[7],bytes[8]};
        if(split.mode>9||!split.rate||split.rate>16||!split.capture_rate||split.capture_rate>16)fail("invalid DMT15 config");
        structure=95;structural={95,7,split.rate};off=9;
    }
    Dmt16Config source_config;
    if(profile==MidProfile::R16SourceModel){
        if(bytes.size()<14)fail("short DMT16 header");source_config={bytes[6],bytes[7],bytes[8]};
        if(source_config.layout>5||!source_config.rate||source_config.rate>16||!source_config.cap||source_config.cap>128)fail("invalid DMT16 config");
        structure=95;structural={95,7,source_config.rate};off=9;
    }
    uint32_t count=v6_unvar(bytes,off);
    if(count>100000)fail("bad DMT1 archive size");
    std::vector<uint32_t> lens;uint64_t total=0;
    for(unsigned i=0;i<count;++i){auto x=v6_unvar(bytes,off);if(x>1000000||(total+=x)>300000000ULL)fail("bad DMT1 length");lens.push_back(x);}
    uint32_t bits=v6_unvar(bytes,off);
    if((uint64_t(bits)+7)/8!=bytes.size()-off-4||(total&&!bits))fail("bad DMT1 payload size");
    ArithmeticDecoder coder(bytes.data()+off,bits);MidContext ctx;ctx.config=configuration;ctx.r13_mask=structure;ctx.r14_active=profile==MidProfile::R14Structure||profile==MidProfile::R15PawnSplit||profile==MidProfile::R16SourceModel;ctx.r14=structural;ctx.r15_active=profile==MidProfile::R15PawnSplit;ctx.r15=split;ctx.r16_active=profile==MidProfile::R16SourceModel;ctx.r16=source_config;
    std::vector<std::vector<Move>> result;result.reserve(count);
    for(auto len:lens){
        Position p=Position::standard();Move previous{},own[2]{};bool has_prev=false,own_valid[2]={false,false};
        std::vector<Move> game;game.reserve(len);
        for(unsigned ply=0;ply<len;++ply){
            unsigned stage=V7Patterns::phase(p,ply);int side=p.side;
            MidStep s=mid_step(p,has_prev?&previous:nullptr,own_valid[side]?&own[side]:nullptr,ply,stage,profile,ctx);
            uint32_t before=0;
            unsigned ix=v6_select(s.coded,s.base.moves.size,coder.target(s.coded.total),before);
            coder.take(before,s.coded.weights[ix],s.coded.total);
            Move m=s.base.moves.a[ix].m;
            mid_observe(ctx,p,s,ix,ply,stage,profile);
            p.apply_unchecked(m); // decoder selected a fully legal candidate
            own[side]=m;own_valid[side]=true;previous=m;has_prev=true;
            game.push_back(m);
        }
        result.push_back(std::move(game));
    }
    return result;
}
} // rmc
