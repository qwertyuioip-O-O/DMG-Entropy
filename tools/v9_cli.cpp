#include "dmg_v9.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
static std::vector<uint8_t> read(const fs::path& p){
    std::ifstream f(p,std::ios::binary);
    if(!f)throw std::runtime_error("cannot read "+p.string());
    return {std::istreambuf_iterator<char>(f),{}};
}
static void write(const fs::path& p,const std::vector<uint8_t>& data){
    std::ofstream f(p,std::ios::binary);
    if(!f)throw std::runtime_error("cannot write "+p.string());
    f.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));
}
int main(int argc,char** argv){
    try{
        if(argc>=5 && std::string(argv[1])=="pack"){
            rmc::V9Model model=rmc::V9Model::Auto;bool found=false;
            for(unsigned i=0;i<=unsigned(rmc::V9Model::Auto);++i)
                if(argv[2]==std::string(rmc::v9_name(rmc::V9Model(i)))){model=rmc::V9Model(i);found=true;break;}
            if(!found)throw std::runtime_error("invalid model; use delta-opportunity/delta-no-pattern/auto");
            std::vector<std::vector<rmc::Move>> games;
            for(int i=4;i<argc;++i){auto data=read(argv[i]);games.push_back(rmc::parse_moves(std::string(data.begin(),data.end())));}
            auto encoded=rmc::v9_encode(games,model);write(argv[3],encoded.bytes);
            std::cout<<games.size()<<" games, "<<encoded.payload_bits<<" payload bits, "<<encoded.bytes.size()<<" total bytes\n";
        }else if(argc==4 && std::string(argv[1])=="unpack"){
            auto games=rmc::v9_decode(read(argv[2]));fs::create_directories(argv[3]);
            for(size_t i=0;i<games.size();++i){
                std::ofstream out(fs::path(argv[3])/("game_"+std::to_string(i+1)+".uci"));
                if(!out)throw std::runtime_error("cannot write decoded game");
                out<<rmc::format_moves(games[i])<<'\n';
            }
            std::cout<<games.size()<<" games extracted\n";
        }else throw std::runtime_error("usage: dmg_v9 pack MODE archive.dmg9 *.uci | dmg_v9 unpack archive out_dir");
    }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
