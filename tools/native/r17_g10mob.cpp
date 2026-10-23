// Isolated research codec: deliberately includes the unchanged reference core.
#include "../../src/experiments/goal10/entropy_g10mob.cpp"
#include "../../src/generated/dmt17_prior.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <chrono>
using namespace rmc;
using Games=std::vector<std::vector<Move>>;
using Prior=std::array<int16_t,168>;
Games read_games(const char* path){std::ifstream f(path);if(!f)fail("bundle open");Games g;std::string l;while(std::getline(f,l))if(!l.empty())g.push_back(parse_moves(l));return g;}
Prior read_prior(const char*path){if(std::string(path)=="builtin")return dmt17_prior_v1;Prior p{};std::ifstream f(path);if(!f)fail("prior open");int x;for(auto&v:p){if(!(f>>x)||x< -128||x>128)fail("prior range/length");v=int16_t(x);}if(f>>x)fail("extra prior values");return p;}
uint32_t prior_id(const Prior&p){std::vector<uint8_t>b;for(int v:p){b.push_back(uint8_t(v));b.push_back(uint8_t(uint16_t(v)>>8));}return v6_crc(b.data(),b.size());}
struct PriorContext:MidContext{uint32_t gate=32768;};
struct PriorStep:MidStep{V6Weights adaptive{},pretrained{};};
PriorContext context(unsigned mode,const Prior&p){PriorContext c;c.config=R12_FIXED;c.r13_mask=95;c.r14_active=true;c.r14={95,7,2};c.r16_active=true;c.r16={0,2,128};if(mode==1||mode==2)c.r12[1].theta=p;return c;}
PriorStep step(const Position&p,const Move*prev,const Move*own,unsigned ply,unsigned stage,PriorContext&c,unsigned mode,const Prior&prior){
 PriorStep s;mid_step_into(s,p,prev,own,ply,stage,MidProfile::R16SourceModel,c);
 if(mode>=3&&stage==1){
  // Mix a static pretrained correction into the adaptive zero-start model.
  // mid_step is read-only: swap only the 336-byte table, never copy maps.
  auto saved=c.r12[1].theta;c.r12[1].theta=prior;
  auto t=mid_step(p,prev,own,ply,stage,MidProfile::R16SourceModel,c);
  c.r12[1].theta=saved;
  s.adaptive=s.coded;s.pretrained=t.coded;
  s.coded=r7_mix(s.coded,t.coded,s.base.moves.size,mode==5?unsigned((uint64_t(c.gate)*100+32768)/65536):mode==3?25:50);
 }
 return s;
}
void observe(PriorContext&c,const Position&p,const PriorStep&s,unsigned ix,unsigned ply,unsigned stage,unsigned mode,const Prior&prior){
 if(mode==2&&stage==1){mid_observe_legacy(c,p,s,ix,ply,stage,MidProfile::R10ComboTact4Full);return;}
 if(mode==5&&stage==1){uint64_t a=uint64_t(65536-c.gate)*s.adaptive.weights[ix]*s.pretrained.total,b=uint64_t(c.gate)*s.pretrained.weights[ix]*s.adaptive.total;c.gate=uint32_t(b*65536/(a+b));c.gate=(c.gate*255u+32768u)/256u;}
 mid_observe(c,p,s,ix,ply,stage,MidProfile::R16SourceModel);
 if(mode==2&&stage==1){c.r12[1].theta=prior;c.r12[1].residual={};}
}
V6Archive encode(const Games&games,unsigned mode,const Prior&prior,MidDiagnostics*diag=nullptr,std::ostream*examples=nullptr){
 if(mode>5)fail("mode");if(diag)*diag={};auto c=context(mode,prior);ArithmeticEncoder coder;uint64_t total=0;
 for(const auto&g:games){if(g.size()>1000000||(total+=g.size())>300000000)fail("length");Position p=Position::standard();Move prev{},own[2]{};bool hp=false,ho[2]={};
  for(unsigned ply=0;ply<g.size();++ply){unsigned stage=V7Patterns::phase(p,ply),side=p.side;
   auto s=step(p,hp?&prev:nullptr,ho[side]?&own[side]:nullptr,ply,stage,c,mode,prior);unsigned ix=index_of(s.base.moves,g[ply]);
   if(examples&&stage==1){auto put16=[&](unsigned v){examples->put(char(v));examples->put(char(v>>8));};put16(s.base.moves.size);put16(ix);for(unsigned i=0;i<s.base.moves.size;++i){put16(s.coded.weights[i]);for(auto k:s.r12_features[i])examples->put(char(k));}}
   mid_record(diag,stage,s,ix);coder.put(v6_before(s.coded,ix),s.coded.weights[ix],s.coded.total);observe(c,p,s,ix,ply,stage,mode,prior);
   if(examples&&stage==1)c.r12[1]=R12Model{};
   p.apply_unchecked(g[ply]);own[side]=g[ply];ho[side]=true;prev=g[ply];hp=true;
  }
 }
 auto payload=total?coder.finish():EntropyPayload{};std::vector<uint8_t>b={'D','1','7','P',1,uint8_t(mode)};uint32_t id=prior_id(prior);for(int k=0;k<4;++k)b.push_back(uint8_t(id>>(8*k)));
 v6_varint(b,uint32_t(games.size()));for(auto&g:games)v6_varint(b,uint32_t(g.size()));v6_varint(b,payload.bit_count);uint64_t framing=b.size()+4;b.insert(b.end(),payload.bytes.begin(),payload.bytes.end());auto crc=v6_crc(b.data(),b.size());for(int k=0;k<4;++k)b.push_back(uint8_t(crc>>(8*k)));return {std::move(b),payload.bit_count,framing};
}
Games decode(const std::vector<uint8_t>&b,const Prior&prior){
 if(b.size()<16||std::string(b.begin(),b.begin()+4)!="D17P"||b[4]!=1||b[5]>5)fail("research header");uint32_t given=0,id=0;for(int k=0;k<4;++k){given|=uint32_t(b[b.size()-4+k])<<(8*k);id|=uint32_t(b[6+k])<<(8*k);}if(given!=v6_crc(b.data(),b.size()-4))fail("research CRC");if(id!=prior_id(prior))fail("wrong prior");
 size_t off=10;auto count=v6_unvar(b,off);if(count>100000)fail("count");std::vector<uint32_t>lens;uint64_t total=0;for(unsigned j=0;j<count;++j){auto n=v6_unvar(b,off);if(n>1000000||(total+=n)>300000000)fail("length");lens.push_back(n);}auto bits=v6_unvar(b,off);if(off+4>b.size()||(uint64_t(bits)+7)/8!=b.size()-off-4||(total&&!bits))fail("payload");
 auto c=context(b[5],prior);ArithmeticDecoder coder(b.data()+off,bits);Games games;
 for(auto len:lens){Position p=Position::standard();Move prev{},own[2]{};bool hp=false,ho[2]={};std::vector<Move>g;
  for(unsigned ply=0;ply<len;++ply){unsigned stage=V7Patterns::phase(p,ply),side=p.side;auto s=step(p,hp?&prev:nullptr,ho[side]?&own[side]:nullptr,ply,stage,c,b[5],prior);uint32_t before=0;unsigned ix=v6_select(s.coded,s.base.moves.size,coder.target(s.coded.total),before);coder.take(before,s.coded.weights[ix],s.coded.total);auto m=s.base.moves.a[ix].m;observe(c,p,s,ix,ply,stage,b[5],prior);p.apply_unchecked(m);own[side]=m;ho[side]=true;prev=m;hp=true;g.push_back(m);}
  games.push_back(std::move(g));
 }return games;
}
#ifndef DMT_R17_REFERENCE_ONLY
int main(int argc,char**argv){try{
 if(argc<4)fail("export BUNDLE OUT | eval BUNDLE PRIOR | speed BUNDLE PRIOR MODE | pack BUNDLE PRIOR MODE OUT | unpack FILE PRIOR OUT");std::string command=argv[1];
 if(command=="unpack"){if(argc!=5)fail("unpack args");std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});auto g=decode(b,read_prior(argv[3]));std::ofstream out(argv[4]);for(auto&game:g){for(auto m:game)out<<to_uci(m)<<' ';out<<'\n';}return 0;}
 auto games=read_games(argv[2]);uint64_t plies=0;for(auto&g:games)plies+=g.size();if(command=="export"){std::ofstream f(argv[3],std::ios::binary);encode(games,0,Prior{},nullptr,&f);return 0;}auto prior=read_prior(argv[3]);
 if(command=="test"){
  unsigned checks=0;auto reject=[&](const auto&b,const Prior&p){try{decode(b,p);}catch(const std::exception&){++checks;return;}fail("malformed accepted");};
  for(const Games&g:std::vector<Games>{Games{},Games{{}},Games{parse_moves("e2e4")},games})for(unsigned mode=0;mode<=5;++mode){auto a=encode(g,mode,prior);if(decode(a.bytes,prior)!=g)fail("test roundtrip");++checks;auto bad=a.bytes;bad.back()^=1;reject(bad,prior);auto wrong=prior;wrong[0]=wrong[0]==128?127:wrong[0]+1;reject(a.bytes,wrong);for(size_t n=0;n<a.bytes.size();++n)if(n<20||n+8>=a.bytes.size())reject(std::vector<uint8_t>(a.bytes.begin(),a.bytes.begin()+n),prior);}
  auto a=encode(games,0,prior),b=mid_encode_source_model(games,{0,2,128});if(a.payload_bits!=b.payload_bits||!std::equal(a.bytes.begin()+a.framing_bytes-4,a.bytes.end()-4,b.bytes.begin()+b.framing_bytes-4,b.bytes.end()-4))fail("R16 payload mismatch");++checks;
  if(encode(games,0,Prior{}).bytes!=encode(games,1,Prior{}).bytes){auto x=encode(games,0,Prior{}),y=encode(games,1,Prior{});if(x.payload_bits!=y.payload_bits||!std::equal(x.bytes.begin()+x.framing_bytes-4,x.bytes.end()-4,y.bytes.begin()+y.framing_bytes-4,y.bytes.end()-4))fail("zero prior identity");}++checks;
  std::cout<<"PASS "<<checks<<" checks; R16 payload identity, zero-prior identity, modes0..5, empty, short, CRC, truncation, wrong-prior\n";return 0;
 }
 if(command=="blocks"){
  std::cout<<"block_size,mode,files,bits,bytes,mid_n,mid_cdf,roundtrip\n"<<std::setprecision(12);
  for(unsigned size:{1u,10u,30u,120u,unsigned(games.size())}){if(!size)continue;for(unsigned mode=0;mode<=2;++mode){uint64_t bits=0,bytes=0,n=0,files=0;double loss=0;for(size_t begin=0;begin<games.size();begin+=size){Games g(games.begin()+begin,games.begin()+std::min(games.size(),begin+size));MidDiagnostics d;auto a=encode(g,mode,prior,&d);if(decode(a.bytes,prior)!=g)fail("block roundtrip");++files;bits+=a.payload_bits;bytes+=a.bytes.size();n+=d.phase[1].moves;loss+=d.phase[1].logloss;}std::cout<<size<<','<<mode<<','<<files<<','<<bits<<','<<bytes<<','<<n<<','<<(n?loss/n:0)<<",PASS\n"<<std::flush;}}
  return 0;
 }
 if(command=="pack"){if(argc!=6)fail("pack args");auto a=encode(games,std::stoul(argv[4]),prior);if(decode(a.bytes,prior)!=games)fail("roundtrip");std::ofstream f(argv[5],std::ios::binary);f.write((const char*)a.bytes.data(),a.bytes.size());return 0;}
 if(command=="speed"){if(argc!=5)fail("speed args");unsigned mode=std::stoul(argv[4]);auto a=encode(games,mode,prior);if(decode(a.bytes,prior)!=games)fail("warm");auto t0=std::chrono::steady_clock::now();for(int k=0;k<3;++k)if(encode(games,mode,prior).bytes!=a.bytes)fail("determinism");auto t1=std::chrono::steady_clock::now();for(int k=0;k<3;++k)if(decode(a.bytes,prior)!=games)fail("roundtrip");auto t2=std::chrono::steady_clock::now();std::cout<<mode<<','<<plies<<','<<std::chrono::duration<double>(t1-t0).count()/3<<','<<std::chrono::duration<double>(t2-t1).count()/3<<'\n';return 0;}
 if(command!="eval"&&command!="eval-frozen")fail("command");std::cout<<"mode,games,plies,bits,bytes,mid_n,mid_cdf,open_cdf,end_cdf,roundtrip\n"<<std::setprecision(12);
 for(unsigned mode=0;mode<=5;++mode){if(command=="eval-frozen"&&mode!=0&&mode!=2)continue;MidDiagnostics d;auto a=encode(games,mode,prior,&d);if(decode(a.bytes,prior)!=games||encode(games,mode,prior).bytes!=a.bytes)fail("roundtrip/determinism");std::cout<<mode<<','<<games.size()<<','<<plies<<','<<a.payload_bits<<','<<a.bytes.size()<<','<<d.phase[1].moves;for(unsigned stage:{1,0,2})std::cout<<','<<(d.phase[stage].moves?d.phase[stage].logloss/d.phase[stage].moves:0);std::cout<<",PASS\n"<<std::flush;}
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

#endif // DMT_R17_REFERENCE_ONLY
