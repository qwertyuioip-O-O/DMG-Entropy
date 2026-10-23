#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace rmc;
int main(int argc,char**argv){
 try{
  if(argc<4)throw std::runtime_error("usage: dmg_mid_trace PROFILE BUNDLE result.csv");
  unsigned id=unsigned(std::stoul(argv[1]));if(id>=MID_PROFILE_COUNT)throw std::runtime_error("bad profile");
  std::ifstream f(argv[2]);if(!f)throw std::runtime_error("missing bundle");
  std::vector<std::vector<Move>> games;std::string ln;
  while(std::getline(f,ln))if(!ln.empty())games.push_back(parse_moves(ln));
  MidDiagnostics d;std::vector<MidTrace> traces;
  auto a=mid_encode(games,MidProfile(id),&d,&traces);
  if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip failed");
  std::ofstream out(argv[3]);if(!out)throw std::runtime_error("cannot write csv");
  out<<"game,ply,phase,context,candidates,rank,uci,capture,check,prob_x16384,bits,tactical_flags\n";
  out<<std::setprecision(9);
  for(auto& v:traces)out<<v.game<<','<<v.ply<<','<<unsigned(v.stage)<<','<<unsigned(v.context)<<','<<v.candidate_count<<','<<v.rank<<','<<to_uci(v.selected)<<','<<unsigned(v.capture)<<','<<unsigned(v.check)<<','<<v.selected_probability<<','<<v.logloss<<','<<v.selected_flags<<'\n';
  std::cout<<"profile="<<mid_name(MidProfile(id))<<" games="<<games.size()<<" plies="<<traces.size()<<" payload_bits="<<a.payload_bits<<" bytes="<<a.bytes.size()<<" middle="<<d.phase[1].logloss/d.phase[1].moves<<'\n';
 }catch(std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
