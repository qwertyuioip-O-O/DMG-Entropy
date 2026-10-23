#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iostream>
#include <random>
using namespace rmc;
static void require(bool b){if(!b)throw std::runtime_error("DMT12 assertion failed");}
static void crc(std::vector<uint8_t>& v){uint32_t c=~0u;for(size_t i=0;i<v.size()-4;++i){c^=v[i];for(unsigned b=0;b<8;++b)c=(c>>1)^((c&1)?0xedb88320u:0);}c=~c;for(unsigned i=0;i<4;++i)v[v.size()-4+i]=uint8_t(c>>(8*i));}
int main(){try{
 std::ifstream in(std::string(RMC_SOURCE_DIR)+"/data-r12/new_select.uci-bundle");require(bool(in));std::vector<std::vector<Move>> human;std::string line;
 while(std::getline(in,line))if(!line.empty())human.push_back(parse_moves(line));
 std::mt19937_64 rng(20261022);std::vector<std::vector<Move>> random;
 for(unsigned g=0;g<12;++g){auto p=Position::standard();std::vector<Move> game;
  for(unsigned ply=0;ply<120;++ply){auto moves=p.legal_moves();if(moves.empty())break;auto m=moves[rng()%moves.size()];p.apply_unchecked(m);game.push_back(m);}random.push_back(game);}
 const Dmt12Config fixed{31,4,100,7,0,10,32};unsigned tested=0;
 for(const auto& games:std::vector<std::vector<std::vector<Move>>>{{},{{}},human,random}){
  auto f=mid_encode(games,MidProfile::R12JointAll);auto g=mid_encode_experimental(games,fixed);
  require(f.bytes[3]=='D'&&f.bytes[5]==250&&g.bytes[3]=='C'&&g.bytes[5]==249);
  require(f.payload_bits==g.payload_bits);require(std::equal(f.bytes.begin()+6,f.bytes.end()-4,g.bytes.begin()+13,g.bytes.end()-4));
  require(mid_decode(f.bytes)==games&&mid_decode(g.bytes)==games);
  require(mid_encode(games,MidProfile::R12JointAll).bytes==f.bytes&&mid_encode_experimental(games,fixed).bytes==g.bytes);
  auto zero=fixed;zero.alpha=0;auto z=mid_encode_experimental(games,zero);auto base=mid_encode(games,MidProfile::R10ComboTact4Full);
  require(z.payload_bits==base.payload_bits);require(std::equal(base.bytes.begin()+6,base.bytes.end()-4,z.bytes.begin()+13,z.bytes.end()-4));
  for(auto config:std::vector<Dmt12Config>{{},{1,1,25,1,0,4,8},{4,4,100,7,1,12,64},{31,16,75,7,0,10,32}}){auto a=mid_encode_experimental(games,config);require(mid_decode(a.bytes)==games);++tested;}
  auto corrupt=g.bytes;corrupt.back()^=1;bool rejected=false;try{mid_decode(corrupt);}catch(...){rejected=true;}require(rejected);
  for(unsigned idx:{6u,7u,8u,9u,10u,11u,12u}){auto bad=g.bytes;bad[idx]=255;crc(bad);bool rejected=false;try{mid_decode(bad);}catch(...){rejected=true;}require(rejected);}
  auto wrong=f.bytes;wrong[3]='B';crc(wrong);rejected=false;try{mid_decode(wrong);}catch(...){rejected=true;}require(rejected);
 }
 auto fixture=mid_encode_experimental(human,fixed).bytes;
 for(size_t length=0;length<fixture.size();++length){std::vector<uint8_t> truncated(fixture.begin(),fixture.begin()+length);if(length>=10)crc(truncated);
  bool rejected=false;try{mid_decode(truncated);}catch(...){rejected=true;}require(rejected);}
 auto invalid=fixed;invalid.rate=0;bool rejected=false;try{mid_encode_experimental(human,invalid);}catch(...){rejected=true;}require(rejected);
 std::cout<<"DMT12 PASS: empty archives/games, new human25, random12; fixed/configurable payload identity; 16 configurations; deterministic; alpha0 identity; CRC; valid-CRC malformed configurations and version rejection\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
