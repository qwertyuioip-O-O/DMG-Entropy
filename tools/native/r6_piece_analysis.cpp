#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
using namespace rmc;
struct Cell { unsigned count=0; double old_bits=0, new_bits=0; };
int main(int argc,char**argv){
 try{
 if(argc!=5)throw std::runtime_error("usage: r6_piece_analysis BUNDLE old_profile new_profile OUTPUT.csv");
 std::vector<std::vector<Move>> games;std::ifstream in(argv[1]);std::string line;
 while(std::getline(in,line))if(!line.empty())games.push_back(parse_moves(line));
 std::vector<MidTrace> a,b;
 auto old=mid_encode(games,MidProfile(std::stoi(argv[2])),nullptr,&a);
 auto newer=mid_encode(games,MidProfile(std::stoi(argv[3])),nullptr,&b);
 if(mid_decode(newer.bytes)!=games)throw std::runtime_error("roundtrip mismatch");
 if(a.size()!=b.size())throw std::runtime_error("trace mismatch");
 std::array<std::array<Cell,2>,7> cells{};
 size_t idx=0;
 for(const auto& game:games){
  Position p=Position::standard();
  for(auto m:game){
   auto ta=a[idx],tb=b[idx];
   if(ta.stage!=tb.stage || ta.selected!=tb.selected)throw std::runtime_error("trace divergence");
   if(ta.stage==1){
    int kind=std::abs(int(p.board[m.from]));
    if(kind<1||kind>6)throw std::runtime_error("invalid board piece");
    auto &c=cells[kind][ta.capture?1:0];
    c.count++;c.old_bits+=ta.logloss;c.new_bits+=tb.logloss;
   }
   if(!p.apply_checked(m))throw std::runtime_error("illegal replay");++idx;
  }
 }
 std::ofstream out(argv[4]);out<<"kind,capture,count,baseline_average,experimental_average,total_bits_saved\n"<<std::setprecision(10);
 for(int kind=1;kind<=6;++kind)for(int capture=0;capture<=1;++capture){auto c=cells[kind][capture];
  out<<kind<<','<<capture<<','<<c.count<<','<<(c.count?c.old_bits/c.count:0)<<','<<(c.count?c.new_bits/c.count:0)<<','<<c.old_bits-c.new_bits<<'\n';}
 std::cout<<"old="<<old.payload_bits<<" new="<<newer.payload_bits<<" records="<<idx<<"\n";
 }catch(std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
