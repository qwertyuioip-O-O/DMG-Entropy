#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rmc {

enum class Mode : uint8_t { Hybrid = 0, Structural = 1, Coordinates = 2, Conditional = 3, Auto = 4, DMGTruncated = 5, EntropyUniform = 6, EntropyStatic = 7, EntropyAdaptive = 8, Best = 9, EntropyPositional = 10, EntropyTactical = 11, EntropyRich = 12, EntropyCheckFiltered = 13, EntropyTacticalCheck = 14, EntropyRichCheck = 15, EntropyPhase = 16, EntropyPhaseTactical = 17, EntropyPhaseCheck = 18, EntropyPhaseForcing = 19, EntropyPhaseOpening = 20, EntropyPhaseMiddle = 21, EntropyPhaseEndgame = 22, EntropyCheckSignal = 23, EntropyCheckSignalLight = 24 }; // encoder-only: choose smallest of Structural and Conditional

struct Move {
    uint8_t from = 0, to = 0;
    uint8_t promotion = 0; // 0: none; 1: queen, 2: rook, 3: bishop, 4: knight
    bool operator==(const Move&) const = default;
};

struct Position {
    std::array<int8_t, 64> board{}; // 1..6 = white pawn..king, -1..-6 = black
    std::array<std::array<uint64_t, 6>, 2> piece_bits{};
    std::array<uint64_t, 2> occupied{};
    uint8_t side = 0; // 0 white, 1 black
    uint8_t castling = 0; // WK=1 WQ=2 BK=4 BQ=8
    int8_t en_passant = -1;
    static Position standard();
    static Position from_fen(std::string_view fen);
    uint64_t pseudo_destinations(int square) const;
    bool apply_checked(Move move); // strict rules and king-safety validation
    void apply_unchecked(Move move); // caller already verified move
    std::vector<Move> legal_moves() const; // test/reference only, not used in codec hot path
    bool square_attacked(int square, int attacker_side) const;
    int king_square(int color) const;
private:
    void put(int square, int8_t piece);
    void take(int square);
};

struct Stats {
    uint64_t total = 0, full = 0, partial = 0, general = 0;
    uint64_t payload_bits = 0;
};

struct Encoded {
    std::vector<uint8_t> bytes;
    Stats stats;
};

Move parse_uci(std::string_view value);
std::string to_uci(Move move);
std::vector<Move> parse_moves(std::string_view text);
std::string format_moves(const std::vector<Move>& moves);
Encoded encode(const std::vector<Move>& moves, Mode mode = Mode::Hybrid,
               std::string_view fen = {});
std::vector<Move> decode(const std::vector<uint8_t>& bytes);
const char* mode_name(Mode mode);
} // namespace rmc
