#include "dmg_mid.hpp"
#include "rmc.hpp"
#include "../src/dmt16_features.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
using namespace rmc;
static void require(bool b){if(!b)throw std::runtime_error("DMT16 assertion");}
template<class F>static void rejects(F f){bool bad=false;try{f();}catch(...){bad=true;}require(bad);}
static void crc(std::vector<uint8_t>&v){uint32_t c=~0u;for(size_t i=0;i<v.size()-4;++i){c^=v[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?0xedb88320u:0);}c=~c;for(unsigned i=0;i<4;++i)v[v.size()-4+i]=uint8_t(c>>(8*i));}
int main(){try{
 auto p=Position::standard();auto m=parse_uci("g1f3");std::array<uint8_t,5> old{10,54,78,106,148};
 require(detail::r16_keys(p,m,old,1,0,false,false,true)==std::array<uint8_t,5>{6,74,114,142,162});
 require(detail::r16_keys(p,m,old,1,1,false,false,true)==std::array<uint8_t,5>{6,74,126,142,162});
 require(detail::r16_keys(p,m,old,2,0,false,false,true)[3]==110);
 require(detail::r16_keys(p,m,old,3,0,false,false,true)[3]==104);
 require(detail::r16_keys(p,m,old,4,0,false,false,true)[3]==109);
 Position mirrored;mirrored.side=1;for(unsigned i=0;i<64;++i)mirrored.board[i^56]=-p.board[i];auto mirror_move=m;mirror_move.from^=56;mirror_move.to^=56;
 for(unsigned layout=0;layout<6;++layout)require(detail::r16_keys(p,m,old,layout,1,false,false,true)==detail::r16_keys(mirrored,mirror_move,old,layout,1,false,false,true));
 auto queen=Position::from_fen("7k/8/8/8/3Q4/8/8/7K w - - 0 1");unsigned direction=0;
 for(const char*uci:{"d4c3","d4d3","d4e3","d4c4","d4e4","d4c5","d4d5","d4e5"})require(detail::r16_direction(queen,parse_uci(uci))==direction++);
 // Independently reconstruct the first-position source marginal from all20 choices.
 uint32_t total=0,source=0,selected=0;MidDiagnostics selected_diag;
 for(auto move:p.legal_moves()){MidDiagnostics d;std::vector<MidTrace>t;mid_encode_source_model({{move}},{1,1,32},&d,&t);require(t.size()==1);total+=t[0].selected_probability;
  if(move.from==parse_uci("e2e4").from)source+=t[0].selected_probability;
  if(move==parse_uci("e2e4")){selected=t[0].selected_probability;selected_diag=d;}}
 require(std::abs(selected_diag.phase[0].source_logloss-std::log2(double(total)/source))<1e-10);
 require(std::abs(selected_diag.phase[0].logloss-std::log2(double(total)/selected))<1e-10);
 std::ifstream in(std::string(RMC_SOURCE_DIR)+"/data-r13/select.uci-bundle");require(bool(in));std::string line;std::getline(in,line);std::vector<std::vector<Move>>human{parse_moves(line)},random;
 std::mt19937_64 rng(16);for(unsigned g=0;g<3;++g){auto pos=Position::standard();std::vector<Move>game;for(unsigned j=0;j<100;++j){auto ms=pos.legal_moves();if(ms.empty())break;auto move=ms[rng()%ms.size()];pos.apply_unchecked(move);game.push_back(move);}random.push_back(game);}
 unsigned cases=0;
 for(const auto&games:std::vector<std::vector<std::vector<Move>>>{{},{{}},human,random}){
  require(mid_encode(games,MidProfile::R16SourceModel).bytes==mid_encode_source_model(games,{0,2,128}).bytes);
  auto baseline=mid_encode_structural(games,{95,7,1}),control=mid_encode_source_model(games,{0,1,32});require(baseline.payload_bits==control.payload_bits&&baseline.bytes.size()==control.bytes.size());require(std::equal(baseline.bytes.begin()+9,baseline.bytes.end()-4,control.bytes.begin()+9,control.bytes.end()-4));
  for(unsigned layout=0;layout<=5;++layout)for(unsigned rate:{1u,4u})for(unsigned cap:{16u,32u,64u,128u}){
   Dmt16Config c{uint8_t(layout),uint8_t(rate),uint8_t(cap)};MidDiagnostics d;auto a=mid_encode_source_model(games,c,&d);require(a.bytes[3]=='H'&&a.bytes[5]==254&&a.bytes[6]==layout&&a.bytes[7]==rate&&a.bytes[8]==cap);require(mid_decode(a.bytes)==games);require(mid_encode_source_model(games,c).bytes==a.bytes);
   for(const auto&phase:d.phase)require(phase.source_logloss>=-1e-8&&phase.source_logloss<=phase.logloss+1e-8);++cases;
  }
  auto good=control.bytes,bad=good;bad.back()^=1;rejects([&]{mid_decode(bad);});
  for(auto pair:std::vector<std::pair<unsigned,unsigned>>{{6,6},{7,0},{7,17},{8,0},{8,129},{3,'G'}}){bad=good;bad[pair.first]=uint8_t(pair.second);crc(bad);rejects([&]{mid_decode(bad);});}
 }
 auto good=mid_encode_source_model(human,{1,1,32}).bytes;for(size_t n=0;n<good.size();++n){std::vector<uint8_t>bad(good.begin(),good.begin()+n);if(n>=10)crc(bad);rejects([&]{mid_decode(bad);});}
 for(auto c:std::vector<Dmt16Config>{{6,1,32},{1,0,32},{1,17,32},{1,1,0},{1,1,129}})rejects([&]{mid_encode_source_model(human,c);});
 require(MID_DEFAULT_PROFILE==MidProfile::R10ComboTact4Full);
 std::cout<<"DMT16 PASS: canonical source/feature layouts, color mirror,8directions, independently reconstructed source marginal; "<<cases<<" codec cases; empty/human/random; control252 payload identity, diagnostics chain rule, deterministic, roundtrip, malformed config/version/CRC, all truncations\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
