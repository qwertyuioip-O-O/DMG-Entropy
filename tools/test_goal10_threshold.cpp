#define DMT_VERIFY_COUNT
#include "../src/entropy_g10threshold.cpp"
#include <random>
#include <iostream>
using namespace rmc;
int main(){try{
 std::mt19937 rng(20261023);Position p=Position::standard();
 // Synthetic candidate sets include impossible/tied flag combinations to stress
 // the full proven score range, independently of corpus coverage.
 for(unsigned i=0;i<64;++i)p.board[i]=int8_t(1+i%6);
 uint64_t checked=0;
 for(unsigned trial=0;trial<8192;++trial){
  V8Step base;std::array<uint16_t,256> tactics{};
  base.moves.size=trial%257;
  for(unsigned i=0;i<base.moves.size;++i){base.moves.a[i].m={uint8_t(rng()%64),uint8_t(rng()%64),0};base.flags[i]=rng();tactics[i]=uint16_t(rng());}
  auto result=r4_rank_features(p,base,tactics);(void)result;checked+=base.moves.size;
 }
 std::cout<<"PASS "<<checked<<" synthetic candidate rank comparisons in 8192 sets (sizes 0..256)\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
