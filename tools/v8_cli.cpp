#include "dmg_v8.hpp"
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
            rmc::V8Model mode=rmc::V8Model::LegalOpportunity;bool ok=false;
            for(int i=0;i<=5;++i)if(std::string(argv[2])==rmc::v8_name(rmc::V8Model(i))){mode=rmc::V8Model(i);ok=true;break;}
            if(!ok)throw std::runtime_error("invalid mode; use legal-opportunity/relation-ae/relation-joint/relation-mixture/relation-mixture-reset/auto");
            std::vector<std::vector<rmc::Move>> games;
            for(int i=4;i<argc;++i){auto b=read(argv[i]);games.push_back(rmc::parse_moves(std::string(b.begin(),b.end())));}
            auto a=rmc::v8_encode(games,mode);write(argv[3],a.bytes);
            std::cout<<games.size()<<" games / "<<a.payload_bits<<" payload bits / "<<a.bytes.size()<<" bytes\n";
        }else if(argc==4&&std::string(argv[1])=="unpack"){
            auto games=rmc::v8_decode(read(argv[2]));fs::create_directories(argv[3]);
            for(size_t i=0;i<games.size();++i){
                std::ofstream out(fs::path(argv[3])/("game_"+std::to_string(i+1)+".uci"));
                if(!out)throw std::runtime_error("can't output game");
                out<<rmc::format_moves(games[i])<<"\n";
            }
            std::cout<<"unpacked "<<games.size()<<" games\n";
        }else throw std::runtime_error("usage: dmg_v8 pack MODE output.dmg8 *.uci | dmg_v8 unpack input.dmg8 out_dir");
    }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
