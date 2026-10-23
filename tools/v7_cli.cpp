#include "dmg_v7.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
static std::vector<uint8_t> read(const fs::path& p){
    std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("can't read "+p.string());
    return {std::istreambuf_iterator<char>(f),{}};
}
static void write(const fs::path& p,const std::vector<uint8_t>& data){
    std::ofstream f(p,std::ios::binary);if(!f)throw std::runtime_error("can't write "+p.string());
    f.write(reinterpret_cast<const char*>(data.data()),data.size());
}
int main(int argc,char** argv){
    try {
        if(argc>=5&&std::string(argv[1])=="pack"){
            rmc::V7Model mode=rmc::V7Model::Baseline;bool ok=false;
            for(int i=0;i<=11;++i)if(std::string(argv[2])==rmc::v7_name(rmc::V7Model(i))){mode=rmc::V7Model(i);ok=true;break;}
            if(!ok)throw std::runtime_error("invalid mode; use baseline/exact/geometry/square/combined/competition/switching/opportunity-geometry/opportunity-square/opportunity-competition/auto/selective-competition");
            std::vector<std::vector<rmc::Move>> games;
            for(int i=4;i<argc;++i){auto b=read(argv[i]);games.push_back(rmc::parse_moves(std::string(b.begin(),b.end())));}
            auto a=rmc::v7_encode(games,mode);write(argv[3],a.bytes);
            std::cout<<games.size()<<" games / "<<a.payload_bits<<" payload bits / "<<a.bytes.size()<<" bytes\n";
        }else if(argc==4&&std::string(argv[1])=="unpack"){
            auto games=rmc::v7_decode(read(argv[2]));fs::create_directories(argv[3]);
            for(size_t i=0;i<games.size();++i){
                std::ofstream out(fs::path(argv[3])/("game_"+std::to_string(i+1)+".uci"));
                if(!out)throw std::runtime_error("can't output game");
                out<<rmc::format_moves(games[i])<<"\n";
            }
            std::cout<<"unpacked "<<games.size()<<" games\n";
        }else throw std::runtime_error("usage: dmg_v7 pack MODE output.dmg7 *.uci | dmg_v7 unpack input.dmg7 out_dir");
    }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
