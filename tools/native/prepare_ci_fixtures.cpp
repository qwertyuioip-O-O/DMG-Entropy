// Compile against the pinned pre-reorganization codec, never the codec under test.
#include "dmg_mid.hpp"
#include "dmg_phase.hpp"
#include "dmg_v8.hpp"
#include "dmg_v9.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace fs = std::filesystem;
using Games = std::vector<std::vector<rmc::Move>>;

std::string read(const fs::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Missing input: " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}

void write_missing(const fs::path& path, const std::vector<uint8_t>& bytes) {
    if (fs::exists(path)) return;
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (!output) throw std::runtime_error("Cannot write: " + path.string());
}

int main() {
    try {
        std::vector<fs::path> paths;
        for (const auto& entry : fs::directory_iterator("examples/heldout"))
            if (entry.path().extension() == ".uci") paths.push_back(entry.path());
        std::sort(paths.begin(), paths.end());
        if (paths.empty()) throw std::runtime_error("No tracked example games");
        Games human;
        for (const auto& path : paths) human.push_back(rmc::parse_moves(read(path)));
        const fs::path golden = "examples/golden";
        write_missing(golden / "italian-relational.dmg8", rmc::v8_encode(
            {rmc::parse_moves(read("examples/italian-opening.uci"))},
            rmc::V8Model::RelationalJoint).bytes);
        write_missing(golden / "human23-delta-opportunity.dmg9",
                      rmc::v9_encode(human, rmc::V9Model::DeltaOpportunity).bytes);
        write_missing(golden / "human23-opening-mixture-endgame.dmp",
                      rmc::phase_encode(human, rmc::PhaseProfile::OpeningMixtureEndgame).bytes);
        for (const auto& [id, name] : std::vector<std::pair<unsigned, const char*>>{
            {14, "human23-mid-all.dmt"}, {32, "human23-r2-best.dmt"},
            {42, "human23-r3-best.dmt"}, {49, "human23-r4-context-rank.dmt"},
            {61, "human23-r5-quiet-fast.dmt"}, {75, "human23-r6-best.dmt"},
            {82, "human23-r7-intent-quiet.dmt"}, {119, "human23-r8-fused-uniform.dmt"},
            {182, "human23-r9-mix-sharp.dmt"}})
            write_missing(golden / name, rmc::mid_encode(human, rmc::MidProfile(id)).bytes);
        // Research selection corpora are unpublished. Tracked examples provide
        // deterministic CI smoke inputs; they are not replacement benchmark data.
        const auto sample = read(paths.front());
        const std::vector<uint8_t> bundle(sample.begin(), sample.end());
        write_missing("data-r12/new_select.uci-bundle", bundle);
        write_missing("data-r13/select.uci-bundle", bundle);
        std::cout << "Prepared missing CI fixtures using the pinned reference codec\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
