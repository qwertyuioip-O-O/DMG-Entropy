#include <array>
#include <vector>
#include <cstdint>
#include <bit>
#include <iostream>
#include <fstream>
#include <iomanip>
struct Desc{uint64_t mask,mul;unsigned offset,shift;};
uint64_t state=0x471b39ae39c6217full;
uint64_t random64(){state^=state>>12;state^=state<<25;state^=state>>27;return state*2685821657736338717ull;}
constexpr int dirs[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
uint64_t mask_for(int sq,bool bishop){uint64_t mask=0;for(int d=bishop?4:0;d<(bishop?8:4);++d){int f=(sq&7)+dirs[d][0],r=(sq>>3)+dirs[d][1];while(unsigned(f)<8&&unsigned(r)<8){int nf=f+dirs[d][0],nr=r+dirs[d][1];if(unsigned(nf)>=8||unsigned(nr)>=8)break;mask|=1ull<<(r*8+f);f=nf;r=nr;}}return mask;}
uint64_t attack(int sq,bool bishop,uint64_t occ){uint64_t out=0;for(int d=bishop?4:0;d<(bishop?8:4);++d){int f=(sq&7)+dirs[d][0],r=(sq>>3)+dirs[d][1];while(unsigned(f)<8&&unsigned(r)<8){uint64_t b=1ull<<(r*8+f);out|=b;if(occ&b)break;f+=dirs[d][0];r+=dirs[d][1];}}return out;}
int main(int argc,char**argv){if(argc!=2)return 1;std::array<std::array<Desc,64>,2> desc;std::vector<uint64_t> all;
for(int bishop=0;bishop<2;++bishop)for(int sq=0;sq<64;++sq){uint64_t mask=mask_for(sq,bishop);unsigned bits=std::popcount(mask),n=1u<<bits,shift=64-bits;std::vector<uint64_t> occ(n),att(n),table(n);std::vector<uint32_t> stamp(n,0);uint64_t subset=0;unsigned i=0;do{occ[i]=subset;att[i]=attack(sq,bishop,subset);++i;subset=(subset-mask)&mask;}while(subset);uint64_t magic=0;uint32_t trial=0;
for(;;){++trial;if(!trial){std::fill(stamp.begin(),stamp.end(),0);trial=1;}magic=random64()&random64()&random64();if(std::popcount((mask*magic)&0xff00000000000000ull)<6)continue;bool ok=true;for(i=0;i<n;++i){unsigned k=unsigned((occ[i]*magic)>>shift);if(stamp[k]==trial&&table[k]!=att[i]){ok=false;break;}stamp[k]=trial;table[k]=att[i];}if(ok)break;}
for(i=0;i<n;++i)if(stamp[i]!=trial)table[i]=0;
desc[bishop][sq]={mask,magic,unsigned(all.size()),shift};all.insert(all.end(),table.begin(),table.end());std::cerr<<(bishop?'B':'R')<<sq<<" trials "<<trial<<'\n';}
std::ofstream f(argv[1]);f<<"#pragma once\n#include <cstdint>\nstruct Goal10MagicDescriptor {uint64_t mask,multiplier;uint32_t offset,shift;};\ninline constexpr Goal10MagicDescriptor goal10_magic[2][64]={\n";for(auto& group:desc){f<<"{\n";for(auto d:group)f<<"{0x"<<std::hex<<d.mask<<"ull,0x"<<d.mul<<"ull,"<<std::dec<<d.offset<<","<<d.shift<<"},\n";f<<"},\n";}f<<"};\ninline constexpr uint64_t goal10_magic_attacks["<<all.size()<<"]={\n";unsigned c=0;for(auto a:all){f<<"0x"<<std::hex<<a<<"ull,";if(++c%8==0)f<<'\n';}f<<"\n};\n";std::cout<<std::dec<<all.size()<<" attack entries\n";return !f;}
