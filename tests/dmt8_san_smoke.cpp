#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
int main(int argc,char**argv){
 if(argc!=2) return 2;
 std::ifstream file(argv[1]);if(!file)return 2;
 std::vector<std::vector<rmc::Move>> games;
 std::string line;
 while(std::getline(file,line))if(!line.empty())games.push_back(rmc::parse_moves(line));
 for(int id:{75,98,107,112,113,114,115,116,117,118,119,120,121,122,123,124}){
  auto a=rmc::mid_encode(games,rmc::MidProfile(id));
  if(rmc::mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip mismatch");
  if(rmc::mid_encode(games,rmc::MidProfile(id)).bytes!=a.bytes)throw std::runtime_error("non-determinism");
  auto damaged=a.bytes;damaged.back()^=0x10;
  bool rejected=false;try{rmc::mid_decode(damaged);}catch(const std::exception&){rejected=true;}
  if(!rejected)throw std::runtime_error("bad CRC not detected");
  std::cout<<id<<":"<<a.payload_bits<<" PASS\n";
 }
}
