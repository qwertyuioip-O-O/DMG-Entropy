#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
using namespace rmc;
static void require(bool b){if(!b)throw std::runtime_error("DMT13 assertion");}
static void crc(std::vector<uint8_t>&v){uint32_t c=~0u;for(size_t i=0;i<v.size()-4;++i){c^=v[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?0xedb88320u:0);}c=~c;for(unsigned i=0;i<4;++i)v[v.size()-4+i]=uint8_t(c>>(8*i));}
template<class F>static void rejects(F f){bool bad=false;try{f();}catch(...){bad=true;}require(bad);}
int main(){try{
 std::ifstream in(std::string(RMC_SOURCE_DIR)+"/data-r13/select.uci-bundle");require(bool(in));std::string line;std::getline(in,line);
 std::vector<std::vector<Move>> human{parse_moves(line)},random;
 std::mt19937_64 rng(13);for(unsigned g=0;g<3;++g){auto p=Position::standard();std::vector<Move> game;for(unsigned j=0;j<100;++j){auto ms=p.legal_moves();if(ms.empty())break;auto m=ms[rng()%ms.size()];p.apply_unchecked(m);game.push_back(m);}random.push_back(game);}
 unsigned cases=0;
 for(const auto& games:std::vector<std::vector<std::vector<Move>>>{{},{{}},human,random}){
  auto base=mid_encode(games,MidProfile::R12JointAll),full=mid_encode_structure(games,127);
  require(base.payload_bits==full.payload_bits&&base.bytes.size()+1==full.bytes.size());
  require(std::equal(base.bytes.begin()+6,base.bytes.end()-4,full.bytes.begin()+7,full.bytes.end()-4));
  for(unsigned mask=0;mask<256;++mask){
   if((mask&128)&&!(mask&2)){rejects([&]{mid_encode_structure(games,uint8_t(mask));});continue;}
   auto a=mid_encode_structure(games,uint8_t(mask));require(a.bytes[3]=='E'&&a.bytes[5]==251&&a.bytes[6]==mask);require(mid_decode(a.bytes)==games);require(mid_encode_structure(games,uint8_t(mask)).bytes==a.bytes);++cases;
  }
  auto bad=full.bytes;bad.back()^=1;rejects([&]{mid_decode(bad);});
  bad=full.bytes;bad[6]=128;crc(bad);rejects([&]{mid_decode(bad);});
  bad=full.bytes;bad[3]='D';crc(bad);rejects([&]{mid_decode(bad);});
 }
 auto a=mid_encode_structure(human,95);for(size_t n=0;n<a.bytes.size();++n){std::vector<uint8_t>b(a.bytes.begin(),a.bytes.begin()+n);if(n>=10)crc(b);rejects([&]{mid_decode(b);});}
 require(MID_DEFAULT_PROFILE==MidProfile::R10ComboTact4Full);
 std::cout<<"DMT13 PASS "<<cases<<" valid-mask cases, empty/human/random, deterministic, roundtrip, 127/250 payload identity, CRC, valid-CRC malformed mask/version, all truncations\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
