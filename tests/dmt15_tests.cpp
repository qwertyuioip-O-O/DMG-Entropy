#include "dmg_mid.hpp"
#include "rmc.hpp"
#include "../src/dmt15_features.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
using namespace rmc;
static void require(bool b){if(!b)throw std::runtime_error("DMT15 assertion");}
template<class F>static void rejects(F f){bool bad=false;try{f();}catch(...){bad=true;}require(bad);}
static void crc(std::vector<uint8_t>&v){uint32_t c=~0u;for(size_t i=0;i<v.size()-4;++i){c^=v[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?0xedb88320u:0);}c=~c;for(unsigned i=0;i<4;++i)v[v.size()-4+i]=uint8_t(c>>(8*i));}
static void feature(const char*fen,const char*uci,unsigned mode,bool capture,unsigned a,unsigned b){
 auto p=Position::from_fen(fen);auto m=parse_uci(uci);auto after=p;require(after.apply_checked(m));auto expected=std::array<unsigned,2>{a,b};require(detail::r15_categories(p,m,detail::r14_pawn_files(p),mode,capture)==expected);
 Position mirror;mirror.side=p.side^1;for(unsigned i=0;i<64;++i)mirror.board[i^56]=-p.board[i];m.from^=56;m.to^=56;require(detail::r15_categories(mirror,m,detail::r14_pawn_files(mirror),mode,capture)==expected);
}
int main(){try{
 feature("7k/8/8/8/8/8/P7/7K w - - 0 1","a2a3",4,false,0,3);
 feature("7k/8/8/8/8/8/PP6/7K w - - 0 1","a2a3",4,false,1,2);
 feature("7k/8/8/1p6/P7/8/8/7K w - - 0 1","a4b5",4,true,3,6);
 feature("7k/8/8/1p6/P7/8/P7/7K w - - 0 1","a4b5",4,true,2,6);
 feature("7k/8/8/1p6/P1P5/8/8/7K w - - 0 1","a4b5",4,true,3,7);
 feature("7k/8/8/Pp6/8/8/8/7K w - b6 0 1","a5b6",4,true,3,6);
 feature("7k/P7/8/8/8/8/8/7K w - - 0 1","a7a8q",4,false,0,3);
 feature("7k/8/8/1p6/P7/8/8/7K w - - 0 1","a4b5",5,true,2,7);
 feature("7k/8/8/1p6/P7/8/8/7K w - - 0 1","a4b5",1,true,2,1);
 for(unsigned mode=0;mode<=9;++mode)for(unsigned key=0;key<168;++key){bool expected=((mode==2||mode==4||mode==5)&&(key==50||key==51))||((mode==3||mode==4||mode==5)&&key>=100&&key<=103);if(mode>=7){expected=false;for(auto x:detail::r15_center_bank)expected|=key==x;for(auto x:detail::r15_distance_bank)expected|=key==x;}require(detail::r15_capture_key(mode,key)==expected);}
 std::ifstream in(std::string(RMC_SOURCE_DIR)+"/data-r13/select.uci-bundle");require(bool(in));std::string line;std::getline(in,line);std::vector<std::vector<Move>> human{parse_moves(line)},random;
 std::mt19937_64 rng(15);for(unsigned g=0;g<3;++g){auto p=Position::standard();std::vector<Move>game;for(unsigned j=0;j<100;++j){auto ms=p.legal_moves();if(ms.empty())break;auto m=ms[rng()%ms.size()];p.apply_unchecked(m);game.push_back(m);}random.push_back(game);}
 // Check the reserved bank never aliases a live knight/king distance key.
 for(const auto&games:std::vector<std::vector<std::vector<Move>>>{human,random})for(const auto&game:games){auto p=Position::standard();for(auto chosen:game){for(auto m:p.legal_moves()){
  const unsigned piece=unsigned(std::abs(int(p.board[m.from]))-1);if(piece==1||piece==5){unsigned d=unsigned(std::max(std::abs(int(m.to&7)-int(m.from&7)),std::abs(int(m.to>>3)-int(m.from>>3))));require(!detail::r15_capture_key(7,96+piece*8+d));}}
  p.apply_unchecked(chosen);}}
 feature("7k/8/8/1p6/P7/8/8/7K w - - 0 1","a4b5",7,true,0,5);
 feature("7k/8/8/1p6/P7/8/8/7K w - - 0 1","a4b5",8,true,2,1);
 feature("7k/8/8/1p6/P7/8/8/7K w - - 0 1","a4b5",9,true,3,1);
 unsigned cases=0;
 for(const auto&games:std::vector<std::vector<std::vector<Move>>>{{},{{}},human,random}){
  auto old=mid_encode_structural(games,{95,7,1}),zero=mid_encode_pawn_split(games,{0,1,1});require(old.payload_bits==zero.payload_bits&&old.bytes.size()==zero.bytes.size());require(std::equal(old.bytes.begin()+9,old.bytes.end()-4,zero.bytes.begin()+9,zero.bytes.end()-4));
  for(unsigned mode=0;mode<=9;++mode)for(unsigned rate:{1u,4u})for(unsigned cap:{1u,4u,16u}){Dmt15Config c{uint8_t(mode),uint8_t(rate),uint8_t(cap)};auto a=mid_encode_pawn_split(games,c);require(a.bytes[3]=='G'&&a.bytes[5]==253&&a.bytes[6]==mode&&a.bytes[7]==rate&&a.bytes[8]==cap);require(mid_decode(a.bytes)==games);require(mid_encode_pawn_split(games,c).bytes==a.bytes);++cases;}
  auto good=zero.bytes;auto bad=good;bad.back()^=1;rejects([&]{mid_decode(bad);});
  for(auto pair:std::vector<std::pair<unsigned,unsigned>>{{6,10},{7,0},{7,17},{8,0},{8,17},{3,'F'}}){bad=good;bad[pair.first]=uint8_t(pair.second);crc(bad);rejects([&]{mid_decode(bad);});}
 }
 auto good=mid_encode_pawn_split(human,{4,1,1}).bytes;for(size_t n=0;n<good.size();++n){std::vector<uint8_t>bad(good.begin(),good.begin()+n);if(n>=10)crc(bad);rejects([&]{mid_decode(bad);});}
 for(auto c:std::vector<Dmt15Config>{{10,1,1},{4,0,1},{4,17,1},{4,1,0},{4,1,17}})rejects([&]{mid_encode_pawn_split(human,c);});
 require(MID_DEFAULT_PROFILE==MidProfile::R10ComboTact4Full);
 std::cout<<"DMT15 PASS:12 semantic fixtures and color mirrors; disjoint capture keys; "<<cases<<" codec cases; empty/human/random, mode0/R14 identity, deterministic, roundtrip, malformed config/version/CRC, all truncations\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
