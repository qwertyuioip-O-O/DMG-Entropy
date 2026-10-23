#include "dmg_phase.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
static std::vector<uint8_t> read(const fs::path& path){
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot read "+path.string());
    return {std::istreambuf_iterator<char>(file),{}};
}
static void write(const fs::path& path,const std::vector<uint8_t>& data){
    std::ofstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot write "+path.string());
    file.write(reinterpret_cast<const char*>(data.data()),data.size());
    if(!file)throw std::runtime_error("failed to write "+path.string());
}
int main(int argc,char** argv){
 try{
    if(argc>=5&&std::string(argv[1])=="pack"){
        rmc::PhaseProfile chosen=rmc::PhaseProfile::Shared;bool known=false;
        for(unsigned i=0;i<=unsigned(rmc::PhaseProfile::OpeningMixtureEndgame);++i){
            auto p=rmc::PhaseProfile(i);
            if(std::string(argv[2])==rmc::phase_name(p)){chosen=p;known=true;break;}
        }
        if(!known)throw std::runtime_error("unknown DMP1 profile");
        std::vector<std::vector<rmc::Move>> games;
        for(int i=4;i<argc;++i){auto bytes=read(argv[i]);
            games.push_back(rmc::parse_moves(std::string(bytes.begin(),bytes.end())));}
        auto archive=rmc::phase_encode(games,chosen);
        write(argv[3],archive.bytes);
        std::cout<<games.size()<<" games / "<<archive.payload_bits<<" bits / "<<archive.bytes.size()<<" bytes\n";
    }else if(argc==4&&std::string(argv[1])=="unpack"){
        auto games=rmc::phase_decode(read(argv[2]));fs::create_directories(argv[3]);
        for(size_t k=0;k<games.size();++k){
            std::ofstream output(fs::path(argv[3])/("game_"+std::to_string(k+1)+".uci"));
            output<<rmc::format_moves(games[k])<<"\n";
            if(!output)throw std::runtime_error("cannot write output");
        }
        std::cout<<"decoded "<<games.size()<<" games\n";
    }else throw std::runtime_error("usage: dmg_phase pack PROFILE file.dmp *.uci | dmg_phase unpack file.dmp output_dir");
 }catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
