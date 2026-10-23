#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=5)return 2;std::ifstream in(argv[1]);if(!in)throw std::runtime_error("bundle");std::vector<std::vector<Move>>games;std::string line;
 while(std::getline(in,line))if(!line.empty())games.push_back(parse_moves(line));
 unsigned start=std::stoul(argv[3]),end=std::stoul(argv[4]);if(start>end||end>MID_PROFILE_COUNT)throw std::runtime_error("range");std::filesystem::create_directories(argv[2]);
 for(unsigned id=start;id<end;++id){auto a=mid_encode(games,MidProfile(id));if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");std::ofstream out(std::filesystem::path(argv[2])/(std::to_string(id)+".dmt"),std::ios::binary);out.write(reinterpret_cast<const char*>(a.bytes.data()),a.bytes.size());if(!out)throw std::runtime_error("write");}
 std::cout<<"audit PASS "<<start<<".."<<end-1<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
