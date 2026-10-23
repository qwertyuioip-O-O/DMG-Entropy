#include "rmc.hpp"
#include "entropy.hpp"
#include <algorithm>
#include <bit>
#include <cctype>
#include <climits>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace rmc {
namespace {
[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }
uint64_t bit(int sq) { return uint64_t{1} << sq; }
int color(int8_t piece) { return piece < 0 ? 1 : 0; }
int typ(int8_t piece) { return piece < 0 ? -piece : piece; }
int sign(int side) { return side == 0 ? 1 : -1; }
int square_of(char file, char rank) {
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') fail("invalid square");
    return (rank - '1') * 8 + (file - 'a');
}
int promotion_type(uint8_t p) {
    switch (p) { case 1: return 5; case 2: return 4; case 3: return 3; case 4: return 2; default: fail("invalid promotion"); }
}
int piece_type(char x) {
    switch (static_cast<char>(std::tolower(static_cast<unsigned char>(x)))) {
        case 'p': return 1; case 'n': return 2; case 'b': return 3;
        case 'r': return 4; case 'q': return 5; case 'k': return 6;
        default: fail("invalid FEN piece");
    }
}
uint64_t mask_for_offset(int sq, const int (*offsets)[2], int n) {
    uint64_t mask = 0;
    int f = sq & 7, r = sq >> 3;
    for (int i = 0; i < n; ++i) {
        int nf = f + offsets[i][0], nr = r + offsets[i][1];
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) mask |= bit(8 * nr + nf);
    }
    return mask;
}
const std::array<uint64_t, 64>& knights() {
    static const auto table = [] {
        const int o[][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
        std::array<uint64_t,64> a{};
        for (int i = 0; i < 64; ++i) a[i] = mask_for_offset(i, o, 8);
        return a;
    }(); return table;
}
const std::array<uint64_t, 64>& kings() {
    static const auto table = [] {
        const int o[][2] = {{1,1},{1,0},{1,-1},{0,1},{0,-1},{-1,1},{-1,0},{-1,-1}};
        std::array<uint64_t,64> a{};
        for (int i = 0; i < 64; ++i) a[i] = mask_for_offset(i, o, 8);
        return a;
    }(); return table;
}
struct Goal10SlidingRays {
    uint64_t ray[64][8]{};
    constexpr Goal10SlidingRays(){
        constexpr int d[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
        for(int sq=0;sq<64;++sq)for(int i=0;i<8;++i){
            int f=(sq&7)+d[i][0],rank=(sq>>3)+d[i][1];
            while(unsigned(f)<8 && unsigned(rank)<8){ray[sq][i]|=uint64_t(1)<<(rank*8+f);f+=d[i][0];rank+=d[i][1];}
        }
    }
};
inline constexpr Goal10SlidingRays goal10_sliding_rays{};
uint64_t sliding(const Position& p,int sq,bool orthogonal,bool diagonal){
    const uint64_t occupied=p.occupied[0]|p.occupied[1];
    uint64_t moves=0;
    for(int d=orthogonal?0:4;d<(diagonal?8:4);++d){
        uint64_t ray=goal10_sliding_rays.ray[sq][d];
        const uint64_t blockers=ray&occupied;
        if(blockers){
            const unsigned nearest=(d&1)?63u-std::countl_zero(blockers):std::countr_zero(blockers);
            ray&=~goal10_sliding_rays.ray[nearest][d];
        }
        moves|=ray;
    }
    return moves&~p.occupied[color(p.board[sq])];
}
unsigned width(unsigned n) { return n <= 1 ? 0u : std::bit_width(n - 1); }
unsigned rank_in(uint64_t mask, unsigned sq) {
    return std::popcount(mask & (bit(static_cast<int>(sq)) - 1));
}
int select(uint64_t mask, unsigned rank) {
    while (rank--) mask &= mask - 1;
    if (!mask) fail("invalid compressed move index");
    return std::countr_zero(mask);
}
struct BitWriter {
    std::vector<uint8_t> bytes;
    uint32_t bits=0;
    void write(uint32_t value, unsigned count) {
        if (count > 32 || (count < 32 && (value >> count))) fail("value exceeds bit width");
        for (unsigned i=0; i<count; ++i) {
            if ((bits & 7u)==0) bytes.push_back(0);
            if (value & (uint32_t{1}<<i)) bytes.back() |= uint8_t(1u << (bits & 7u));
            if (bits == UINT32_MAX) fail("bitstream too large");
            ++bits;
        }
    }
};
struct BitReader {
    const uint8_t* bytes;
    uint32_t bits, pos=0;
    uint32_t read(unsigned count) {
        if (count > 32 || count > bits-pos) fail("truncated bitstream");
        uint32_t value=0;
        for (unsigned i=0; i<count; ++i,++pos)
            value |= uint32_t((bytes[pos>>3] >> (pos&7)) & 1u) << i;
        return value;
    }
};
void write_u32(std::vector<uint8_t>& dst, uint32_t v) {
    for (int i=0;i<4;++i) dst.push_back(uint8_t(v>>(i*8)));
}
uint32_t read_u32(const std::vector<uint8_t>& v, size_t offset) {
    if (offset+4>v.size()) fail("truncated header");
    uint32_t out=0;
    for (int i=0;i<4;++i) out |= uint32_t(v[offset+i]) << (8*i);
    return out;
}
struct Context {
    Position pos;
    std::optional<Move> previous;
    bool previous_capture=false;
    explicit Context(Position p):pos(std::move(p)){}
    void advance(Move m) {
        int8_t piece=pos.board[m.from];
        bool capture=pos.board[m.to]!=0 || (typ(piece)==1 && pos.en_passant==m.to && (m.from&7)!=(m.to&7));
        if (!pos.apply_checked(m)) fail("illegal move in stream");
        previous=m;
        previous_capture=capture;
    }
};
Move default_move(int from, int to, const Position& pos) {
    uint8_t promo=0;
    if (typ(pos.board[from])==1 && ((to>>3)==0 || (to>>3)==7)) promo=1;
    return Move{uint8_t(from),uint8_t(to),promo};
}
// Returns pseudo-legal attackers of the last opponent destination in ascending square order.
uint64_t previous_target_attackers(const Context& ctx) {
    if (!ctx.previous) return 0;
    int target=ctx.previous->to;
    int8_t piece=ctx.pos.board[target];
    if (!piece || color(piece)==ctx.pos.side) return 0;
    uint64_t candidates=0, our=ctx.pos.occupied[ctx.pos.side];
    while (our) {
        int from=std::countr_zero(our); our &= our-1;
        if (ctx.pos.pseudo_destinations(from) & bit(target)) candidates|=bit(from);
    }
    return candidates;
}
std::optional<Move> predicted(const Context& ctx) {
    if (!ctx.previous) return std::nullopt;
    if (ctx.previous_capture) {
        uint64_t attackers=previous_target_attackers(ctx);
        if (attackers) {
            int best=-1, bestcost=INT_MAX;
            while (attackers) {
                int from=std::countr_zero(attackers); attackers &= attackers-1;
                int price=typ(ctx.pos.board[from]);
                // Pawn, knight, bishop, rook, queen, king (low is preferred).
                static constexpr int costs[7]={0,1,3,3,5,9,10};
                if (costs[price]<bestcost) {best=from;bestcost=costs[price];}
            }
            if (best>=0) return default_move(best,ctx.previous->to,ctx.pos);
        }
    }
    // Mirror the opponent's last move through the horizontal midline.
    int from=ctx.previous->from ^ 56, to=ctx.previous->to ^ 56;
    if (ctx.pos.board[from] && color(ctx.pos.board[from])==ctx.pos.side &&
        (ctx.pos.pseudo_destinations(from)&bit(to))) return default_move(from,to,ctx.pos);
    return std::nullopt;
}
void write_promo(BitWriter& w, Move move) {
    if (move.promotion) w.write(move.promotion-1,2);
}
Move read_promo(BitReader& r, Move move, const Position& pos) {
    if (typ(pos.board[move.from])==1 && ((move.to>>3)==0 || (move.to>>3)==7))
        move.promotion=uint8_t(r.read(2)+1);
    return move;
}
void write_general(BitWriter& w, const Position& p, Move move) {
    uint64_t ours=p.occupied[p.side];
    unsigned n=std::popcount(ours);
    if (!n || !(ours&bit(move.from))) fail("no moving piece");
    w.write(rank_in(ours,move.from),width(n));
    uint64_t dest=p.pseudo_destinations(move.from);
    unsigned count=std::popcount(dest);
    if (!count || !(dest & bit(move.to))) fail("move not pseudo-legal");
    w.write(rank_in(dest,move.to),width(count));
    write_promo(w,move);
}
Move read_general(BitReader& r, const Position& p) {
    uint64_t ours=p.occupied[p.side];
    unsigned n=std::popcount(ours);
    if (!n) fail("invalid stream: no pieces");
    unsigned index=r.read(width(n));
    if (index>=n) fail("invalid from index");
    int from=select(ours,index);
    uint64_t dest=p.pseudo_destinations(from);
    unsigned count=std::popcount(dest);
    if (!count) fail("invalid stream: no destinations");
    unsigned dst=r.read(width(count));
    if (dst>=count) fail("invalid destination index");
    return read_promo(r,Move{uint8_t(from),uint8_t(select(dest,dst)),0},p);
}
} // namespace

Position Position::standard() { return from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); }
Position Position::from_fen(std::string_view fen) {
    Position p;
    std::istringstream input{std::string(fen)};
    std::string placement, turn, rights, ep;
    if (!(input>>placement>>turn>>rights>>ep)) fail("FEN must have board, turn, castling and ep fields");
    int rank=7,file=0;
    for (char x:placement) {
        if (x=='/') {if(file!=8||rank==0) fail("invalid FEN ranks");--rank;file=0;continue;}
        if (x>='1'&&x<='8') {file+=x-'0';if(file>8) fail("invalid FEN rank width");continue;}
        if(file>=8) fail("invalid FEN square");
        int pt=piece_type(x), side=std::isupper(static_cast<unsigned char>(x))?0:1;
        p.put(rank*8+file, int8_t(sign(side)*pt));
        ++file;
    }
    if (rank!=0||file!=8) fail("invalid FEN board dimensions");
    if (turn=="w") p.side=0;
    else if(turn=="b") p.side=1;
    else fail("invalid FEN turn");
    if(rights!="-") for(char ch:rights) {
        switch(ch) {case 'K':p.castling|=1;break;case 'Q':p.castling|=2;break;
                    case 'k':p.castling|=4;break;case 'q':p.castling|=8;break;
                    default:fail("invalid castling rights");}
    }
    if(ep!="-") {
        if(ep.size()!=2) fail("invalid FEN ep square");
        int sq=square_of(ep[0],ep[1]);
        if ((sq>>3) != (p.side==0?5:2)) fail("inconsistent FEN ep square");
        p.en_passant=int8_t(sq);
    }
    if(std::popcount(p.piece_bits[0][5])!=1||std::popcount(p.piece_bits[1][5])!=1)
        fail("FEN must have one king per side");
    return p;
}
void Position::take(int square) {
    int8_t piece=board[square];
    if(!piece)return;
    auto side=color(piece); uint64_t sq=bit(square);
    piece_bits[side][typ(piece)-1]&=~sq; occupied[side]&=~sq;board[square]=0;
}
void Position::put(int square,int8_t piece) {
    if(board[square]) fail("internal occupied-square error");
    auto side=color(piece);uint64_t sq=bit(square);
    piece_bits[side][typ(piece)-1]|=sq;occupied[side]|=sq;board[square]=piece;
}
int Position::king_square(int side_) const {
    uint64_t bb=piece_bits[side_][5];
    return bb?std::countr_zero(bb):-1;
}
uint64_t Position::pseudo_destinations(int from) const {
    int8_t piece=board[from];
    if(!piece||color(piece)!=side)return 0;
    int type=typ(piece), f=from&7, r=from>>3;
    uint64_t destination=0;
    switch(type) {
        case 1: {
            int direction=side==0?1:-1;
            int nr=r+direction;
            if(nr>=0&&nr<8) {
                int one=from+direction*8;
                if(!board[one]) {
                    destination |= bit(one);
                    if (r==(side==0?1:6) && !board[from+direction*16]) destination |= bit(from+direction*16);
                }
                for(int df:{-1,1}) {
                    if(f+df<0||f+df>7)continue;
                    int to=nr*8+f+df;
                    if((board[to]&&color(board[to])!=side)||en_passant==to)destination|=bit(to);
                }
            }
            break;
        }
        case 2: destination=knights()[from]&~occupied[side]; break;
        case 3: destination=sliding(*this,from,false,true); break;
        case 4: destination=sliding(*this,from,true,false); break;
        case 5: destination=sliding(*this,from,true,true); break;
        case 6: {
            destination=kings()[from]&~occupied[side];
            if(side==0&&from==4) {
                if((castling&1)&&board[7]==4&&!board[5]&&!board[6])destination|=bit(6);
                if((castling&2)&&board[0]==4&&!board[1]&&!board[2]&&!board[3])destination|=bit(2);
            }
            if(side==1&&from==60) {
                if((castling&4)&&board[63]==-4&&!board[61]&&!board[62])destination|=bit(62);
                if((castling&8)&&board[56]==-4&&!board[57]&&!board[58]&&!board[59])destination|=bit(58);
            }
            break;
        }
        default: fail("invalid internal piece type");
    }
    return destination;
}
// Cache-free, exact bitboard attack oracle. The ray masks contain squares
// through the board edge and are clipped at the first blocker. The piece
// bitboards are kept synchronized by Position::take/put (incl. EP/castling).
namespace {
struct AttackLookup {
    uint64_t ray[64][8]{}, pawn_origin[2][64]{};
    constexpr AttackLookup(){
        constexpr int df[8]={1,-1,0,0,1,1,-1,-1};
        constexpr int dr[8]={0,0,1,-1,1,-1,1,-1};
        for(int sq=0;sq<64;++sq){
            const int f=sq&7,r=sq>>3;
            for(int d=0;d<8;++d){
                int x=f+df[d],y=r+dr[d];
                while(unsigned(x)<8&&unsigned(y)<8){
                    ray[sq][d]|=uint64_t{1}<<(y*8+x);
                    x+=df[d]; y+=dr[d];
                }
            }
            for(int side=0;side<2;++side){
                const int origin_rank=r+(side==0?-1:1);
                if(unsigned(origin_rank)>=8)continue;
                if(f>0)pawn_origin[side][sq]|=uint64_t{1}<<(origin_rank*8+f-1);
                if(f<7)pawn_origin[side][sq]|=uint64_t{1}<<(origin_rank*8+f+1);
            }
        }
    }
};
inline constexpr AttackLookup attack_lookup{};
}
bool Position::square_attacked(int sq,int attacker) const {
    if(attack_lookup.pawn_origin[attacker][sq]&piece_bits[attacker][0])return true;
    if(knights()[sq]&piece_bits[attacker][1])return true;
    if(kings()[sq]&piece_bits[attacker][5])return true;
    const uint64_t all=occupied[0]|occupied[1];
    const uint64_t rook_queen=piece_bits[attacker][3]|piece_bits[attacker][4];
    const uint64_t bishop_queen=piece_bits[attacker][2]|piece_bits[attacker][4];
    for(int d=0;d<8;++d){
        const uint64_t blockers=attack_lookup.ray[sq][d]&all;
        if(!blockers)continue;
        const int first=(d==0||d==2||d==4||d==6)?
            std::countr_zero(blockers):63-std::countl_zero(blockers);
        if((uint64_t{1}<<first)&(d<4?rook_queen:bishop_queen))return true;
    }
    return false;
}
void Position::apply_unchecked(Move move) {
    int from=move.from,to=move.to;
    int8_t piece=board[from];
    int old_ep=en_passant;
    en_passant=-1;
    // Clear rights if corner rook moves or is captured; harmless even if piece wasn't rook.
    if(from==0||to==0) castling &=uint8_t(~2);
    if(from==7||to==7) castling &=uint8_t(~1);
    if(from==56||to==56)castling &=uint8_t(~8);
    if(from==63||to==63)castling &=uint8_t(~4);
    if(typ(piece)==6)castling &=uint8_t(side==0?~3:~12);
    if(typ(piece)==1 && to==old_ep && !board[to] && ((from&7)!=(to&7)))
        take(to+(side==0?-8:8));
    take(to);take(from);
    if(typ(piece)==6 && ((from==4&&(to==6||to==2))||(from==60&&(to==62||to==58)))) {
        int rook_from=to>from?(side==0?7:63):(side==0?0:56);
        int rook_to=to>from?to-1:to+1;
        int8_t rook=board[rook_from];take(rook_from);put(rook_to,rook);
    }
    if(move.promotion)piece=int8_t(sign(side)*promotion_type(move.promotion));
    put(to,piece);
    if(typ(board[to])==1 && (to-from==16 || from-to==16)) en_passant=int8_t((to+from)/2);
    side ^= 1;
}
bool Position::apply_checked(Move move) {
    if(move.from>=64||move.to>=64)return false;
    int8_t piece=board[move.from];
    if(!piece||color(piece)!=side||typ(piece)==0)return false;
    if(!(pseudo_destinations(move.from)&bit(move.to)))return false;
    // Cannot capture the enemy king.
    if(board[move.to]&&typ(board[move.to])==6)return false;
    bool promotes=typ(piece)==1 && ((move.to>>3)==0||(move.to>>3)==7);
    if(promotes!=(move.promotion!=0)||move.promotion>4)return false;
    if(typ(piece)==6 && ((move.from==4&&(move.to==6||move.to==2)) ||
                         (move.from==60&&(move.to==62||move.to==58)))) {
        int enemy=side^1;
        if(square_attacked(move.from,enemy))return false;
        Position transit=*this;
        int middle=(move.from+move.to)/2;
        transit.take(move.from);transit.put(middle,piece);
        if(transit.square_attacked(middle,enemy))return false;
    }
    Position next=*this;
    next.apply_unchecked(move);
    int our_king=next.king_square(side);
    if(our_king<0||next.square_attacked(our_king,side^1))return false;
    *this=std::move(next);
    return true;
}
std::vector<Move> Position::legal_moves() const {
    std::vector<Move> out;
    uint64_t sources=occupied[side];
    while(sources) {
        int from=std::countr_zero(sources);sources&=sources-1;
        uint64_t dest=pseudo_destinations(from);
        while(dest) {
            int to=std::countr_zero(dest);dest&=dest-1;
            bool promotes=typ(board[from])==1&&((to>>3)==0||(to>>3)==7);
            int max=promotes?4:1;
            for(int i=0;i<max;++i) {
                Move m{uint8_t(from),uint8_t(to),uint8_t(promotes?i+1:0)};
                Position probe=*this;
                if(probe.apply_checked(m))out.push_back(m);
            }
        }
    }
    return out;
}
Move parse_uci(std::string_view value) {
    if(value.size()!=4&&value.size()!=5)fail("UCI move must have 4 or 5 characters");
    uint8_t promo=0;
    if(value.size()==5) {
        switch(value[4]) {case 'q':promo=1;break;case 'r':promo=2;break;
                         case 'b':promo=3;break;case 'n':promo=4;break;
                         default:fail("invalid UCI promotion");}
    }
    return Move{uint8_t(square_of(value[0],value[1])),uint8_t(square_of(value[2],value[3])),promo};
}
std::string to_uci(Move m) {
    if(m.from>=64||m.to>=64||m.promotion>4)fail("invalid move");
    std::string text;
    for(int sq:{int(m.from),int(m.to)}) {
        text.push_back(char('a'+(sq&7)));text.push_back(char('1'+(sq>>3)));
    }
    if(m.promotion)text.push_back("\0qrbn"[m.promotion]);
    return text;
}
std::vector<Move> parse_moves(std::string_view text) {
    std::istringstream input{std::string(text)};
    std::vector<Move> moves;
    std::string token;
    while(input>>token) {
        if(token[0]=='#') {std::getline(input,token);continue;}
        moves.push_back(parse_uci(token));
    }
    return moves;
}
std::string format_moves(const std::vector<Move>& moves) {
    std::string out;
    for(auto m:moves) {out+=to_uci(m);out+='\n';}
    return out;
}
const char* mode_name(Mode mode) {
    switch(mode) {case Mode::Hybrid:return "RMC-Hybrid";case Mode::Structural:return "SMC-Structural";
                  case Mode::Coordinates:return "Coordinates-12+promotion";case Mode::Conditional:return "RMC-Conditional";case Mode::Auto:return "Auto-best-of-2";case Mode::DMGTruncated:return "DMG-Truncated";case Mode::EntropyUniform:return "Entropy-Uniform";case Mode::EntropyStatic:return "Entropy-Static";case Mode::EntropyAdaptive:return "Entropy-Adaptive";case Mode::Best:return "Best-of-all";case Mode::EntropyPositional:return "Entropy-Positional";case Mode::EntropyTactical:return "Entropy-Tactical";case Mode::EntropyRich:return "Entropy-Rich";case Mode::EntropyCheckFiltered:return "Entropy-CheckFiltered";case Mode::EntropyTacticalCheck:return "Entropy-TacticalCheck";case Mode::EntropyRichCheck:return "Entropy-RichCheck";case Mode::EntropyPhase:return "Entropy-Phase";case Mode::EntropyPhaseTactical:return "Entropy-PhaseTactical";case Mode::EntropyPhaseCheck:return "Entropy-PhaseCheck";case Mode::EntropyPhaseForcing:return "Entropy-PhaseForcing";case Mode::EntropyPhaseOpening:return "Entropy-PhaseOpening";case Mode::EntropyPhaseMiddle:return "Entropy-PhaseMiddle";case Mode::EntropyPhaseEndgame:return "Entropy-PhaseEndgame";case Mode::EntropyCheckSignal:return "Entropy-CheckSignal";case Mode::EntropyCheckSignalLight:return "Entropy-CheckSignalLight";}
    return "invalid";
}
Encoded encode(const std::vector<Move>& moves,Mode mode,std::string_view fen) {
    if(mode==Mode::Auto) {
        // Never worse in payload bits than the structural baseline. Encoder work is doubled.
        auto baseline=encode(moves,Mode::Structural,fen);
        auto conditional=encode(moves,Mode::Conditional,fen);
        if(conditional.stats.payload_bits<baseline.stats.payload_bits) return conditional;
        return baseline; // favor simpler/faster decoder for ties
    }
    if(mode==Mode::Best) {
        Encoded best=encode(moves,Mode::Structural,fen);
        for(auto alternative:{Mode::Conditional,Mode::DMGTruncated,Mode::EntropyUniform,Mode::EntropyStatic,Mode::EntropyAdaptive,Mode::EntropyPositional,Mode::EntropyTactical,Mode::EntropyRich,Mode::EntropyCheckFiltered,Mode::EntropyTacticalCheck,Mode::EntropyRichCheck,Mode::EntropyPhase,Mode::EntropyPhaseTactical,Mode::EntropyPhaseCheck,Mode::EntropyPhaseForcing,Mode::EntropyPhaseOpening,Mode::EntropyPhaseMiddle,Mode::EntropyPhaseEndgame,Mode::EntropyCheckSignal,Mode::EntropyCheckSignalLight}) {
            auto trial=encode(moves,alternative,fen);
            if(trial.stats.payload_bits<best.stats.payload_bits)best=std::move(trial);
        }
        return best;
    }
    if(mode!=Mode::Hybrid&&mode!=Mode::Structural&&mode!=Mode::Coordinates&&mode!=Mode::Conditional&&mode!=Mode::DMGTruncated&&mode!=Mode::EntropyUniform&&mode!=Mode::EntropyStatic&&mode!=Mode::EntropyAdaptive&&mode!=Mode::EntropyPositional&&mode!=Mode::EntropyTactical&&mode!=Mode::EntropyRich&&mode!=Mode::EntropyCheckFiltered&&mode!=Mode::EntropyTacticalCheck&&mode!=Mode::EntropyRichCheck&&mode!=Mode::EntropyPhase&&mode!=Mode::EntropyPhaseTactical&&mode!=Mode::EntropyPhaseCheck&&mode!=Mode::EntropyPhaseForcing&&mode!=Mode::EntropyPhaseOpening&&mode!=Mode::EntropyPhaseMiddle&&mode!=Mode::EntropyPhaseEndgame&&mode!=Mode::EntropyCheckSignal&&mode!=Mode::EntropyCheckSignalLight)fail("unknown compression mode");
    if(moves.size()>1'000'000)fail("too many plies");
    if(fen.size()>65535)fail("FEN too long");
    Context ctx(fen.empty()?Position::standard():Position::from_fen(fen));
    if(mode==Mode::DMGTruncated||mode==Mode::EntropyUniform||mode==Mode::EntropyStatic||mode==Mode::EntropyAdaptive||mode==Mode::EntropyPositional||mode==Mode::EntropyTactical||mode==Mode::EntropyRich||mode==Mode::EntropyCheckFiltered||mode==Mode::EntropyTacticalCheck||mode==Mode::EntropyRichCheck||mode==Mode::EntropyPhase||mode==Mode::EntropyPhaseTactical||mode==Mode::EntropyPhaseCheck||mode==Mode::EntropyPhaseForcing||mode==Mode::EntropyPhaseOpening||mode==Mode::EntropyPhaseMiddle||mode==Mode::EntropyPhaseEndgame||mode==Mode::EntropyCheckSignal||mode==Mode::EntropyCheckSignalLight) {
        auto payload=entropy_encode(moves,mode,ctx.pos);
        Stats stats{};stats.total=moves.size();stats.general=moves.size();stats.payload_bits=payload.bit_count;
        std::vector<uint8_t> out;
        out.reserve(16+(fen.empty()?0:2+fen.size())+payload.bytes.size());
        out.insert(out.end(),{'R','M','C','1'});
        out.push_back(uint8_t(mode));out.push_back(fen.empty()?0:1);out.push_back(0);out.push_back(0);
        write_u32(out,uint32_t(moves.size()));write_u32(out,payload.bit_count);
        if(!fen.empty()) {
            out.push_back(uint8_t(fen.size()));out.push_back(uint8_t(fen.size()>>8));
            out.insert(out.end(),fen.begin(),fen.end());
        }
        out.insert(out.end(),payload.bytes.begin(),payload.bytes.end());
        return {std::move(out),stats};
    }
    BitWriter writer;
    Stats stats{};
    for(Move m:moves) {
        // Validate before emitting bits; if invalid, no corrupt output escapes.
        Position probe=ctx.pos;
        if(!probe.apply_checked(m))fail("input has illegal chess move");
        if(mode==Mode::Coordinates) {
            writer.write(m.from,6);writer.write(m.to,6);write_promo(writer,m);
            ++stats.general;
        } else if(mode==Mode::Structural) {
            write_general(writer,ctx.pos,m);++stats.general;
        } else if(mode==Mode::Conditional) {
            auto candidate=predicted(ctx);
            if(candidate && *candidate==m) {
                writer.write(0,1); ++stats.full;
            } else {
                uint64_t attackers=previous_target_attackers(ctx);
                bool has_attacker=attackers!=0;
                bool partial=has_attacker&&ctx.previous&&m.to==ctx.previous->to&&(attackers&bit(m.from));
                if(partial) {
                    // Context-adaptive prefix omission: no code needed when a branch is impossible.
                    if(candidate) writer.write(1,1);
                    writer.write(0,1);
                    writer.write(rank_in(attackers,m.from),width(std::popcount(attackers)));
                    write_promo(writer,m); ++stats.partial;
                } else {
                    if(candidate) writer.write(1,1);
                    if(has_attacker) writer.write(1,1);
                    write_general(writer,ctx.pos,m); ++stats.general;
                }
            }
        } else {
            auto candidate=predicted(ctx);
            if(candidate && *candidate==m) {
                writer.write(0,1);++stats.full;
            } else {
                uint64_t attackers=0;
                if(ctx.previous && m.to==ctx.previous->to) attackers=previous_target_attackers(ctx);
                if(attackers&&(attackers&bit(m.from))) {
                    writer.write(1,1);writer.write(0,1); // 10 in stream order
                    writer.write(rank_in(attackers,m.from),width(std::popcount(attackers)));
                    write_promo(writer,m);++stats.partial;
                } else {
                    writer.write(1,1);writer.write(1,1); // 11
                    write_general(writer,ctx.pos,m);++stats.general;
                }
            }
        }
        // Avoid duplicating checked moves from the main path by swapping in verified board.
        bool capture=ctx.pos.board[m.to]!=0 ||
                     (typ(ctx.pos.board[m.from])==1&&ctx.pos.en_passant==m.to&&(m.from&7)!=(m.to&7));
        ctx.pos=std::move(probe);ctx.previous=m;ctx.previous_capture=capture;
        ++stats.total;
    }
    stats.payload_bits=writer.bits;
    std::vector<uint8_t> out;
    out.reserve(16+(fen.empty()?0:2+fen.size())+writer.bytes.size());
    out.insert(out.end(),{'R','M','C','1'});
    out.push_back(uint8_t(mode));out.push_back(fen.empty()?0:1);out.push_back(0);out.push_back(0);
    write_u32(out,uint32_t(moves.size()));write_u32(out,writer.bits);
    if(!fen.empty()) {
        out.push_back(uint8_t(fen.size()));out.push_back(uint8_t(fen.size()>>8));
        out.insert(out.end(),fen.begin(),fen.end());
    }
    out.insert(out.end(),writer.bytes.begin(),writer.bytes.end());
    return Encoded{std::move(out),stats};
}
std::vector<Move> decode(const std::vector<uint8_t>& bytes) {
    if(bytes.size()<16 || std::memcmp(bytes.data(),"RMC1",4)!=0)fail("invalid RMC1 header");
    auto mode=Mode(bytes[4]);
    if(mode!=Mode::Hybrid&&mode!=Mode::Structural&&mode!=Mode::Coordinates&&mode!=Mode::Conditional&&mode!=Mode::DMGTruncated&&mode!=Mode::EntropyUniform&&mode!=Mode::EntropyStatic&&mode!=Mode::EntropyAdaptive&&mode!=Mode::EntropyPositional&&mode!=Mode::EntropyTactical&&mode!=Mode::EntropyRich&&mode!=Mode::EntropyCheckFiltered&&mode!=Mode::EntropyTacticalCheck&&mode!=Mode::EntropyRichCheck&&mode!=Mode::EntropyPhase&&mode!=Mode::EntropyPhaseTactical&&mode!=Mode::EntropyPhaseCheck&&mode!=Mode::EntropyPhaseForcing&&mode!=Mode::EntropyPhaseOpening&&mode!=Mode::EntropyPhaseMiddle&&mode!=Mode::EntropyPhaseEndgame&&mode!=Mode::EntropyCheckSignal&&mode!=Mode::EntropyCheckSignalLight)fail("unknown compression mode");
    if(bytes[5]>1||bytes[6]!=0||bytes[7]!=0)fail("invalid RMC header flags");
    uint32_t count=read_u32(bytes,8), bits=read_u32(bytes,12);
    if(count>1'000'000)fail("unreasonable ply count");
    size_t offset=16;
    std::string fen;
    if(bytes[5]) {
        if(offset+2>bytes.size())fail("truncated FEN length");
        size_t n=size_t(bytes[offset]) | size_t(bytes[offset+1])<<8;offset+=2;
        if(n>bytes.size()-offset)fail("truncated FEN");
        fen.assign(reinterpret_cast<const char*>(bytes.data()+offset),n);offset+=n;
    }
    if((bits+uint64_t{7})/8 != bytes.size()-offset)fail("incorrect bitstream size");
    if((bits&7u)&& !bytes.empty() && (bytes.back()>>(bits&7u)))fail("nonzero padding bits");
    Context ctx(fen.empty()?Position::standard():Position::from_fen(fen));
    if(mode==Mode::DMGTruncated||mode==Mode::EntropyUniform||mode==Mode::EntropyStatic||mode==Mode::EntropyAdaptive||mode==Mode::EntropyPositional||mode==Mode::EntropyTactical||mode==Mode::EntropyRich||mode==Mode::EntropyCheckFiltered||mode==Mode::EntropyTacticalCheck||mode==Mode::EntropyRichCheck||mode==Mode::EntropyPhase||mode==Mode::EntropyPhaseTactical||mode==Mode::EntropyPhaseCheck||mode==Mode::EntropyPhaseForcing||mode==Mode::EntropyPhaseOpening||mode==Mode::EntropyPhaseMiddle||mode==Mode::EntropyPhaseEndgame||mode==Mode::EntropyCheckSignal||mode==Mode::EntropyCheckSignalLight)
        return entropy_decode(bytes.data()+offset,bits,count,mode,ctx.pos);
    BitReader reader{bytes.data()+offset,bits};
    std::vector<Move> result;
    result.reserve(count);
    for(uint32_t i=0;i<count;++i) {
        Move m;
        if(mode==Mode::Coordinates) {
            m=read_promo(reader,Move{uint8_t(reader.read(6)),uint8_t(reader.read(6)),0},ctx.pos);
        } else if(mode==Mode::Structural) {
            m=read_general(reader,ctx.pos);
        } else if(mode==Mode::Conditional) {
            auto candidate=predicted(ctx);
            bool is_full=false, is_partial=false;
            if(candidate && reader.read(1)==0) is_full=true;
            if(is_full) m=*candidate;
            else {
                uint64_t attackers=previous_target_attackers(ctx);
                if(attackers && reader.read(1)==0) is_partial=true;
                if(is_partial) {
                    unsigned n=std::popcount(attackers);
                    unsigned index=reader.read(width(n));
                    if(index>=n) fail("invalid partial attacker index");
                    m=read_promo(reader,Move{uint8_t(select(attackers,index)),ctx.previous->to,0},ctx.pos);
                } else m=read_general(reader,ctx.pos);
            }
        } else {
            if(reader.read(1)==0) {
                auto candidate=predicted(ctx);
                if(!candidate)fail("full reconstruction unavailable");
                m=*candidate;
            } else if(reader.read(1)==0) {
                uint64_t attackers=previous_target_attackers(ctx);
                auto n=std::popcount(attackers);
                if(!n)fail("partial reconstruction unavailable");
                unsigned index=reader.read(width(n));
                if(index>=unsigned(n))fail("invalid partial attacker index");
                int from=select(attackers,index);
                m=read_promo(reader,Move{uint8_t(from),ctx.previous->to,0},ctx.pos);
            } else m=read_general(reader,ctx.pos);
        }
        ctx.advance(m);
        result.push_back(m);
    }
    if(reader.pos!=reader.bits)fail("unexpected trailing bits");
    return result;
}
} // namespace rmc
