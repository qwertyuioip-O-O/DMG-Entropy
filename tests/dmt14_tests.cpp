#include "dmg_mid.hpp"
#include "rmc.hpp"
#include "../src/dmt14_features.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
using namespace rmc;
static void require(bool b){if(!b)throw std::runtime_error("DMT14 assertion");}
static void crc(std::vector<uint8_t>&v){uint32_t c=~0u;for(size_t i=0;i<v.size()-4;++i){c^=v[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?0xedb88320u:0);}c=~c;for(unsigned i=0;i<4;++i)v[v.size()-4+i]=uint8_t(c>>(8*i));}
template<class F>static void rejects(F f){bool bad=false;try{f();}catch(...){bad=true;}require(bad);}
static void feature(const char* fen,const char* uci,unsigned center,unsigned distance){
 auto p=Position::from_fen(fen);auto m=parse_uci(uci);auto after=p;require(after.apply_checked(m));
 const auto expected=std::array<unsigned,2>{center,distance};require(detail::r14_categories(p,m,detail::r14_pawn_files(p))==expected);
 Position mirrored;mirrored.side=p.side^1;for(unsigned i=0;i<64;++i)mirrored.board[i^56]=-p.board[i];
 m.from^=56;m.to^=56;require(detail::r14_categories(mirrored,m,detail::r14_pawn_files(mirrored))==expected);
}
int main(){try{
 feature("7k/8/8/8/8/8/8/R6K w - - 0 1","a1a4",0,0);
 feature("7k/8/P7/8/8/8/8/R6K w - - 0 1","a1a4",1,1);
 feature("7k/8/p7/8/8/8/8/R6K w - - 0 1","a1a4",2,2);
 feature("7k/p7/P7/8/8/8/8/R6K w - - 0 1","a1a4",3,3);
 feature("7k/8/1p6/8/8/8/8/R6K w - - 0 1","a1b1",2,6);
 feature("7k/8/8/8/8/8/P7/7K w - - 0 1","a2a3",0,5);
 feature("7k/8/8/P7/8/8/P7/7K w - - 0 1","a2a3",0,7);
 feature("7k/8/8/8/8/8/PP6/7K w - - 0 1","a2a3",1,4);
 feature("7k/8/8/p7/8/8/P7/7K w - - 0 1","a2a3",2,1);
 feature("7k/8/8/1p6/8/8/P7/7K w - - 0 1","a2a3",0,1);
 feature("7k/8/8/1p6/P7/8/8/7K w - - 0 1","a4b5",0,5);
 feature("7k/8/8/Pp6/8/8/8/7K w - b6 0 1","a5b6",0,5);
 feature("7k/P7/8/8/8/8/8/7K w - - 0 1","a7a8q",0,5);
 std::ifstream in(std::string(RMC_SOURCE_DIR)+"/data-r13/select.uci-bundle");require(bool(in));std::string line;std::getline(in,line);std::vector<std::vector<Move>> human{parse_moves(line)},random;
 std::mt19937_64 rng(14);for(unsigned g=0;g<3;++g){auto p=Position::standard();std::vector<Move>game;for(unsigned j=0;j<100;++j){auto ms=p.legal_moves();if(ms.empty())break;auto m=ms[rng()%ms.size()];p.apply_unchecked(m);game.push_back(m);}random.push_back(game);}
 unsigned cases=0;
 for(const auto&games:std::vector<std::vector<std::vector<Move>>>{{},{{}},human,random}){
  require(mid_encode(games,MidProfile::R14Structure).bytes==mid_encode_structural(games,{95,7,1}).bytes);
  auto old=mid_encode_structure(games,95);auto zero=mid_encode_structural(games,{95,0,4});require(old.payload_bits==zero.payload_bits&&old.bytes.size()+2==zero.bytes.size());require(std::equal(old.bytes.begin()+7,old.bytes.end()-4,zero.bytes.begin()+9,zero.bytes.end()-4));
  for(auto structure:{0,63,95,127,255})for(unsigned mode=0;mode<8;++mode)for(unsigned rate:{1u,4u,16u}){
   Dmt14Config c{uint8_t(structure),uint8_t(mode),uint8_t(rate)};auto a=mid_encode_structural(games,c);require(a.bytes[3]=='F'&&a.bytes[5]==252&&a.bytes[6]==structure&&a.bytes[7]==mode&&a.bytes[8]==rate);require(mid_decode(a.bytes)==games);require(mid_encode_structural(games,c).bytes==a.bytes);++cases;
  }
  auto good=mid_encode_structural(games,{95,3,4}).bytes;
  auto bad=good;bad.back()^=1;rejects([&]{mid_decode(bad);});
  for(auto pair:std::vector<std::pair<unsigned,unsigned>>{{6,128},{7,8},{8,0},{8,17},{3,'E'}}){bad=good;bad[pair.first]=uint8_t(pair.second);crc(bad);rejects([&]{mid_decode(bad);});}
 }
 auto good=mid_encode_structural(human,{95,3,4}).bytes;for(size_t n=0;n<good.size();++n){std::vector<uint8_t>bad(good.begin(),good.begin()+n);if(n>=10)crc(bad);rejects([&]{mid_decode(bad);});}
 for(auto c:std::vector<Dmt14Config>{{128,3,4},{95,8,4},{95,3,0},{95,3,17}})rejects([&]{mid_encode_structural(human,c);});
 require(MID_DEFAULT_PROFILE==MidProfile::R10ComboTact4Full);
 std::cout<<"DMT14 PASS: 13 semantic feature fixtures and color mirrors; "<<cases<<" codec configurations; empty/human/random; deterministic, roundtrip, mode0/control payload identity, CRC, malformed valid-CRC configs/version, all truncations\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
