// Standalone TU deliberately includes implementation to test private kernels.
#include "../src/entropy.cpp"
#include <random>
#include <iostream>
namespace rmc {
void kernel_tests(){
 std::mt19937_64 rng(20261022);uint64_t divisions=0,maps=0;
 for(unsigned i=0;i<20000;++i){uint64_t d=rng();if(!d)d=1; if(i<64)d=uint64_t(1)<<i;
  ExactDivisor divide(d);
  for(uint64_t x:{uint64_t(0),uint64_t(1),d-1,d,UINT64_MAX,rng(),rng(),rng()}){
   if(divide(x)!=x/d)throw std::runtime_error("exact division mismatch");++divisions;}
 }
 auto check=[&](const Position&p){OwnAttackReuse reuse(p);
  for(auto m:p.legal_moves()){Position after=p;after.apply_unchecked(m);
   if(reuse.after(p,after)!=AttackCache(after).attacked_mask)throw std::runtime_error("attack reuse mismatch");++maps;}};
 // Explicit EP, both castles, all promotions and sparse/sliding positions.
 for(const char* fen:{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
  "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1",
  "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
  "4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1",
  "4k2r/6P1/8/8/8/8/1p6/R3K3 w - - 0 1",
  "4k2r/6P1/8/8/8/8/1p6/R3K3 b - - 0 1"})check(Position::from_fen(fen));
 for(unsigned g=0;g<128;++g){auto p=Position::standard();for(unsigned ply=0;ply<144;++ply){
  check(p);auto legal=p.legal_moves();if(legal.empty())break;p.apply_unchecked(legal[rng()%legal.size()]);}}
 uint64_t score_checks=0;
 for(unsigned trial=0;trial<128;++trial){std::array<int16_t,10> theta{};
  for(auto& t:theta)t=int16_t(int(rng()%49)-24);FeatureScore10 table(theta);
  for(unsigned mask=0;mask<1024;++mask){int reference=0;for(unsigned b=0;b<10;++b)if(mask&(1u<<b))reference+=theta[b];
   if(table(mask)!=reference)throw std::runtime_error("feature score table mismatch");++score_checks;}}
 std::cout<<"score table PASS: "<<score_checks<<" exhaustive-mask comparisons\n";
 std::cout<<"kernel PASS: "<<divisions<<" exact divisions, "<<maps<<" legal-candidate attack maps\n";
}
}
int main(){try{rmc::kernel_tests();}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
