#include "../src/generated/dmt16_expected.hpp"
#include <iostream>
#include <random>
#include <limits>
int main(){try{
 uint64_t cases=0;
 auto check=[&](uint32_t x,uint32_t d){auto reciprocal=uint32_t((uint64_t(1)<<32)/d);if(rmc::detail::r16_divide_u32(x,d,reciprocal)!=x/d)throw std::runtime_error("u32 exactness");++cases;};
 for(uint32_t total=16129;total<=16384;++total)for(uint32_t mass=0;mass<=total*5;++mass)check(mass*4096u+total/2,total);
 std::mt19937 rng(16);for(unsigned i=0;i<200000;++i){uint32_t d=rng();if(d<2)d=2;check(rng(),d);}
 for(uint32_t d:{2u,3u,7u,16383u,16384u,16385u,32767u,2147483649u,4294967295u})for(uint32_t x:{0u,1u,2u,4294967294u,4294967295u})check(x,d);
 std::cout<<"U32 exact division PASS "<<cases<<" reference quotients; model-domain exhaustive, full-width random and edges\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
