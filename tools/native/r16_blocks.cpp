#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iostream>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=2)return 2;std::ifstream in(argv[1]);if(!in)throw std::runtime_error("bundle");std::vector<std::vector<Move>>games;std::string line;uint64_t plies=0;
 while(std::getline(in,line))if(!line.empty()){auto g=parse_moves(line);plies+=g.size();games.push_back(g);}
 std::cout<<"chunk_size,chunks,games,plies,bits252,bits254,bits_control,bytes252,bytes254,bytes_control,roundtrip\n";
 for(unsigned size:{1u,10u,30u,120u,480u}){
  std::array<uint64_t,3>bits{},bytes{};unsigned chunks=0;
  for(size_t i=0;i<games.size();i+=size){std::vector<std::vector<Move>>block(games.begin()+i,games.begin()+std::min(games.size(),i+size));++chunks;
   for(unsigned model=0;model<3;++model){auto a=model==0?mid_encode_structural(block,{95,7,1}):mid_encode_source_model(block,{uint8_t(model==1?1:0),2,128});if(mid_decode(a.bytes)!=block)throw std::runtime_error("roundtrip");bits[model]+=a.payload_bits;bytes[model]+=a.bytes.size();}}
  std::cout<<size<<','<<chunks<<','<<games.size()<<','<<plies;for(auto x:bits)std::cout<<','<<x;for(auto x:bytes)std::cout<<','<<x;std::cout<<",PASS\n"<<std::flush;
 }
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
