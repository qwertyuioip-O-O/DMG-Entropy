#include "dmg_v6.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
static std::vector<uint8_t> read_bytes(const fs::path& p) {
    std::ifstream f(p,std::ios::binary);
    if(!f)throw std::runtime_error("cannot open: "+p.string());
    return {std::istreambuf_iterator<char>(f),{}};
}
static void write_bytes(const fs::path& p,const std::vector<uint8_t>& data){
    std::ofstream f(p,std::ios::binary);
    if(!f)throw std::runtime_error("cannot write: "+p.string());
    f.write(reinterpret_cast<const char*>(data.data()),data.size());
    if(!f)throw std::runtime_error("write failed: "+p.string());
}
int main(int argc,char** argv) {
    try {
        if(argc>=5 && std::string(argv[1])=="pack") {
            std::string mode_name=argv[2];
            rmc::V6Model mode=rmc::V6Model::Baseline;
            bool ok=false;
            for(int k=0;k<5;++k)if(mode_name==rmc::v6_name(rmc::V6Model(k))) {
                mode=rmc::V6Model(k);ok=true;break;
            }
            if(!ok)throw std::runtime_error("mode must be baseline/hierarchical/mixture/dictionary/all");
            std::vector<std::vector<rmc::Move>> games;
            for(int i=4;i<argc;++i) {
                auto data=read_bytes(argv[i]);
                games.push_back(rmc::parse_moves(std::string(data.begin(),data.end())));
            }
            auto encoded=rmc::v6_encode(games,mode);
            write_bytes(argv[3],encoded.bytes);
            std::cout<<"packed "<<games.size()<<" games, "<<encoded.payload_bits
                     <<" payload bits, "<<encoded.bytes.size()<<" total bytes\n";
        } else if(argc==4 && std::string(argv[1])=="unpack") {
            auto games=rmc::v6_decode(read_bytes(argv[2]));
            fs::create_directories(argv[3]);
            for(size_t i=0;i<games.size();++i){
                fs::path out=fs::path(argv[3])/("game_"+std::to_string(i+1)+".uci");
                std::ofstream f(out);
                if(!f)throw std::runtime_error("cannot write UCI file");
                f<<rmc::format_moves(games[i])<<'\n';
            }
            std::cout<<"unpacked "<<games.size()<<" games\n";
        } else {
            throw std::runtime_error("usage: dmg_v6 pack MODE output.dmg6 input.uci ... | dmg_v6 unpack archive.dmg6 output_dir");
        }
    } catch(const std::exception& e) {std::cerr<<"Error: "<<e.what()<<"\n";return 1;}
}
