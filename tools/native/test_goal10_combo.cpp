#include "../../src/experiments/goal10/entropy_g10combo.cpp"
#include <iostream>
#include <random>
using namespace rmc;
int main(){try{
 uint64_t checks=0,castles=0,ep=0,promotions=0;std::mt19937 rng(20261023);
 auto check=[&](const Position&p){OwnAttackReuse reuse(p);AttackCache enemy(p);
  for(auto m:p.legal_moves()){Position next=p;next.apply_unchecked(m);
   if(reuse.after(p,next)!=AttackCache(next).attacked_mask)fail("cached attack mask mismatch");
   if(v8_relations(p,next,m,nullptr,enemy,15,&reuse)!=v8_relations(p,next,m,nullptr,enemy,15,nullptr))fail("relation mismatch");
   ++checks;if(abs_piece(p.board[m.from])==6&&std::abs(int(m.to)-int(m.from))==2)++castles;
   if(abs_piece(p.board[m.from])==1&&(m.from&7)!=(m.to&7)&&!p.board[m.to])++ep;
   if(m.promotion)++promotions;
  }
 };
 for(auto fen:{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1","4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1","4k3/P7/8/8/8/8/7p/4K3 w - - 0 1","4k3/8/8/8/8/8/4R3/4K3 b - - 0 1","4k3/8/8/8/8/PPPPPPPP/PPPPPPPP/4K3 w - - 0 1"}){
  auto p=Position::from_fen(fen);check(p);for(auto m:p.legal_moves()){auto q=p;q.apply_unchecked(m);check(q);for(auto n:q.legal_moves()){auto z=q;z.apply_unchecked(n);check(z);}}
 }
 for(unsigned game=0;game<24;++game){auto p=Position::standard();for(unsigned ply=0;ply<120;++ply){check(p);auto moves=p.legal_moves();if(moves.empty())break;p.apply_unchecked(moves[rng()%moves.size()]);}}
 if(!castles||!ep||!promotions)fail("special coverage missing");
 std::cout<<"PASS "<<checks<<" full-mask/relation comparisons; castles="<<castles<<" en_passant="<<ep<<" promotions="<<promotions<<'\n';
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
