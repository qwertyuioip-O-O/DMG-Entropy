#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <bit>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=4)return 2;std::string selector=argv[1];
 std::ifstream in(argv[2]);if(!in)throw std::runtime_error("missing bundle");std::vector<std::vector<Move>> games;std::string line;
 while(std::getline(in,line))if(!line.empty())games.push_back(parse_moves(line));
 MidDiagnostics diag;std::vector<MidTrace> trace;Dmt14Config c;V6Archive a;if(selector=="250")a=mid_encode(games,MidProfile::R12JointAll,&diag,&trace);else if(selector=="95")a=mid_encode_structure(games,95,&diag,&trace);else{auto sep=selector.find(':');if(sep==std::string::npos)throw std::runtime_error("need features:rate");int mode=std::stoi(selector.substr(0,sep)),rate=std::stoi(selector.substr(sep+1));if(mode<0||mode>7||rate<1||rate>16)throw std::runtime_error("range");c.features=uint8_t(mode);c.rate=uint8_t(rate);a=mid_encode_structural(games,c,&diag,&trace);}if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");
 std::ofstream out(argv[3]);if(!out)throw std::runtime_error("output");
 out<<"game,ply,phase,piece,uci,candidates,rank,bits,probability,capture,in_check,gives_check,landing_attacked,landing_supported,escape,see_win,see_loss,promotion,castle,previous_capture,material,own_piece_count,enemy_piece_count\n"<<std::setprecision(12);
 size_t j=0;
 for(unsigned g=0;g<games.size();++g){auto p=Position::standard();bool last_capture=false;
  for(unsigned ply=0;ply<games[g].size();++ply){auto m=games[g][ply];const auto& t=trace.at(j++);int pc=std::abs(int(p.board[m.from]));
   bool cap=p.board[m.to]!=0||(pc==1&&(m.from&7)!=(m.to&7));int side=p.side;
   bool attacked_from=p.square_attacked(m.from,side^1);bool check=p.square_attacked(p.king_square(side),side^1);
   unsigned own=std::popcount(p.occupied[side]),enemy=std::popcount(p.occupied[side^1]);int material=0;for(auto x:p.board){int v=std::abs(int(x));if(v&&v<6)material+=(v==1?1:v==4?5:v==5?9:3);}
   auto after=p;if(!after.apply_checked(m))throw std::runtime_error("illegal replay");
   bool attacked=after.square_attacked(m.to,after.side),supported=after.square_attacked(m.to,side),gives=after.square_attacked(after.king_square(after.side),side);
   out<<g<<','<<ply<<','<<unsigned(t.stage)<<','<<pc<<','<<to_uci(m)<<','<<t.candidate_count<<','<<t.rank<<','<<t.logloss<<','<<std::exp2(-t.logloss)<<','<<cap<<','<<check<<','<<gives<<','<<attacked<<','<<supported<<','<<(attacked_from&&!attacked)<<','<<bool(t.selected_flags&1)<<','<<bool(t.selected_flags&2)<<','<<unsigned(m.promotion)<<','<<(pc==6&&std::abs(int(m.to)-int(m.from))==2)<<','<<last_capture<<','<<material<<','<<own<<','<<enemy<<'\n';
   p=after;last_capture=cap;
  }
 }
 std::cout<<"trace "<<selector<<" games="<<games.size()<<" plies="<<trace.size()<<" bits="<<a.payload_bits<<" bytes="<<a.bytes.size()<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
