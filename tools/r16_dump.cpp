#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iostream>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=6)return 2;int layout=std::stoi(argv[3]),rate=std::stoi(argv[4]),cap=std::stoi(argv[5]);if(layout<0||layout>5||rate<1||rate>16||cap<1||cap>128)throw std::runtime_error("range");
 std::ifstream in(argv[1]);if(!in)throw std::runtime_error("bundle");std::vector<std::vector<Move>>games;std::string line;while(std::getline(in,line))if(!line.empty())games.push_back(parse_moves(line));
 auto a=mid_encode_source_model(games,{uint8_t(layout),uint8_t(rate),uint8_t(cap)});if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(a.bytes.data()),a.bytes.size());if(!out)throw std::runtime_error("write");std::cout<<a.payload_bits<<','<<a.bytes.size()<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
