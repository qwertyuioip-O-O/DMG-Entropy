// Reuse the immutable R17 implementation; no old archive semantics change.
#define DMT_R17_REFERENCE_ONLY
#include "r17_g10phase.cpp"
#undef DMT_R17_REFERENCE_ONLY
#include "../../src/dmt18_prior.hpp"
using Table18=std::array<int16_t,672>;
struct Config18{int rate,cap,decay;};
constexpr Config18 configs18[10]={{0,0,0},{1,8,6},{1,16,6},{1,32,8},{1,64,10},{2,16,6},{2,32,8},{2,64,10},{1,32,6},{0,0,0}};
Table18 load18(const char*path){if(std::string(path)=="builtin")return dmt18_prior_v1;Table18 a{};if(std::string(path)=="zero")return a;std::ifstream f(path);int v;for(auto&x:a){if(!(f>>v)||v< -128||v>128)fail("table18");x=int16_t(v);}if(f>>v)fail("table18 extra");return a;}
uint32_t id18(const Table18&t){std::vector<uint8_t>b;for(int v:t){b.push_back(uint8_t(v));b.push_back(uint8_t(uint16_t(v)>>8));}return v6_crc(b.data(),b.size());}
struct Context18{PriorContext core=context(2,dmt17_prior_v1);std::array<int16_t,168> delta{};std::array<int32_t,168> remainder{};unsigned seen=0;};
unsigned bucket18(uint32_t flags){return unsigned(bool(flags&(1u<<4)))+2*unsigned(bool(flags&(1u<<2)));}
PriorStep step18(const Position&p,const Move*prev,const Move*own,unsigned ply,unsigned stage,Context18&c,unsigned mode,const Table18&t,bool active){
 auto s=step(p,prev,own,ply,stage,c.core,2,dmt17_prior_v1);
 if(mode==9&&active&&stage==1){std::array<uint64_t,256>raw{};for(unsigned i=0;i<s.base.moves.size;++i){int score=0;unsigned offset=168*bucket18(s.base.flags[i]);for(auto k:s.r12_features[i])score+=t[offset+k];score=std::clamp(score,-128,128);raw[i]=uint64_t(s.coded.weights[i])*v8_exp2_lut[score+128];}s.coded=v6_quantize(raw,s.base.moves.size);}
 return s;
}
void observe18(Context18&c,const Position&p,const PriorStep&s,unsigned ix,unsigned ply,unsigned stage,unsigned mode){
 if(stage!=1){observe(c.core,p,s,ix,ply,stage,2,dmt17_prior_v1);return;}
 auto cfg=configs18[mode];if(cfg.rate){
  std::array<uint32_t,168>sums{};std::array<bool,168>actual{};for(unsigned i=0;i<s.base.moves.size;++i)for(auto k:s.r12_features[i])sums[k]+=s.coded.weights[i];for(auto k:s.r12_features[ix])actual[k]=true;
  const ExactDivisor divide(s.coded.total);for(unsigned k=0;k<168;++k){int expected=int(divide(uint64_t(sums[k])*4096+s.coded.total/2));c.remainder[k]+=cfg.rate*((actual[k]?4096:0)-expected);int change=c.remainder[k]/4096;c.remainder[k]-=change*4096;c.delta[k]=int16_t(std::clamp(int(c.delta[k])+change,-cfg.cap,cfg.cap));}
  if((++c.seen&((1u<<cfg.decay)-1))==0)for(auto&d:c.delta){if(d>0)--d;else if(d<0)++d;}
  for(unsigned k=0;k<168;++k)c.core.r12[1].theta[k]=int16_t(dmt17_prior_v1[k]+c.delta[k]);
 }
 mid_observe_legacy(c.core,p,s,ix,ply,stage,MidProfile::R10ComboTact4Full);
}
V6Archive encode18(const Games&games,unsigned mode,const Table18&t,MidDiagnostics*diag=nullptr,std::ostream*examples=nullptr){
 if(mode>9||games.size()>100000)fail("mode/count18");bool active=std::any_of(t.begin(),t.end(),[](int v){return v!=0;});if(diag)*diag={};Context18 c;ArithmeticEncoder coder;uint64_t total=0;
 for(const auto&g:games){if(g.size()>1000000||(total+=g.size())>300000000)fail("length18");Position p=Position::standard();Move prev{},own[2]{};bool hp=false,ho[2]={};for(unsigned ply=0;ply<g.size();++ply){unsigned stage=V7Patterns::phase(p,ply),side=p.side;auto s=step18(p,hp?&prev:nullptr,ho[side]?&own[side]:nullptr,ply,stage,c,mode,t,active);unsigned ix=index_of(s.base.moves,g[ply]);
  if(examples&&stage==1){auto put=[&](unsigned v){examples->put(char(v));examples->put(char(v>>8));};put(s.base.moves.size);put(ix);for(unsigned i=0;i<s.base.moves.size;++i){put(s.coded.weights[i]);for(auto k:s.r12_features[i])put(168*bucket18(s.base.flags[i])+k);}}
  mid_record(diag,stage,s,ix);coder.put(v6_before(s.coded,ix),s.coded.weights[ix],s.coded.total);observe18(c,p,s,ix,ply,stage,mode);p.apply_unchecked(g[ply]);own[side]=g[ply];ho[side]=true;prev=g[ply];hp=true;
 }}auto payload=total?coder.finish():EntropyPayload{};std::vector<uint8_t>b={'D','1','8','P',1,uint8_t(mode)};auto id=id18(t);for(int k=0;k<4;++k)b.push_back(uint8_t(id>>(8*k)));v6_varint(b,uint32_t(games.size()));for(auto&g:games)v6_varint(b,uint32_t(g.size()));v6_varint(b,payload.bit_count);uint64_t framing=b.size()+4;b.insert(b.end(),payload.bytes.begin(),payload.bytes.end());auto crc=v6_crc(b.data(),b.size());for(int k=0;k<4;++k)b.push_back(uint8_t(crc>>(8*k)));return {std::move(b),payload.bit_count,framing};
}
Games decode18(const std::vector<uint8_t>&b,const Table18&t){
 if(b.size()<16||std::string(b.begin(),b.begin()+4)!="D18P"||b[4]!=1||b[5]>9)fail("header18");uint32_t crc=0,id=0;for(int k=0;k<4;++k){crc|=uint32_t(b[b.size()-4+k])<<(8*k);id|=uint32_t(b[6+k])<<(8*k);}if(crc!=v6_crc(b.data(),b.size()-4)||id!=id18(t))fail("CRC/model18");size_t off=10;unsigned count=v6_unvar(b,off);if(count>100000)fail("count18");std::vector<uint32_t>lens;uint64_t total=0;for(unsigned j=0;j<count;++j){auto n=v6_unvar(b,off);if(n>1000000||(total+=n)>300000000)fail("length18");lens.push_back(n);}auto bits=v6_unvar(b,off);if(off+4>b.size()||(uint64_t(bits)+7)/8!=b.size()-off-4||(total&&!bits))fail("payload18");ArithmeticDecoder coder(b.data()+off,bits);Context18 c;Games games;bool active=std::any_of(t.begin(),t.end(),[](int v){return v!=0;});
 for(auto len:lens){Position p=Position::standard();Move prev{},own[2]{};bool hp=false,ho[2]={};std::vector<Move>g;for(unsigned ply=0;ply<len;++ply){unsigned stage=V7Patterns::phase(p,ply),side=p.side;auto s=step18(p,hp?&prev:nullptr,ho[side]?&own[side]:nullptr,ply,stage,c,b[5],t,active);uint32_t before=0;auto ix=v6_select(s.coded,s.base.moves.size,coder.target(s.coded.total),before);coder.take(before,s.coded.weights[ix],s.coded.total);auto m=s.base.moves.a[ix].m;observe18(c,p,s,ix,ply,stage,b[5]);p.apply_unchecked(m);own[side]=m;ho[side]=true;prev=m;hp=true;g.push_back(m);}games.push_back(std::move(g));}return games;
}
#ifndef DMT_R18_REFERENCE_ONLY
int main(int argc,char**argv){try{
 if(argc<4)fail("export BUNDLE OUT | eval BUNDLE TABLE MODES | speed BUNDLE TABLE MODE | pack BUNDLE TABLE MODE OUT | unpack FILE TABLE OUT | test BUNDLE TABLE");std::string cmd=argv[1];
 if(cmd=="unpack"){if(argc!=5)fail("args");std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});auto games=decode18(b,load18(argv[3]));std::ofstream out(argv[4]);for(auto&g:games){for(auto m:g)out<<to_uci(m)<<' ';out<<'\n';}if(!out)fail("write18");return 0;}
 auto games=read_games(argv[2]);uint64_t plies=0;for(auto&g:games)plies+=g.size();if(cmd=="export"){std::ofstream f(argv[3],std::ios::binary);encode18(games,0,{},nullptr,&f);if(!f)fail("export18");return 0;}auto table=load18(argv[3]);
 if(cmd=="test"){unsigned checks=0;auto reject=[&](const auto&b,const auto&t){try{decode18(b,t);}catch(const std::exception&){++checks;return;}fail("accepted18");};for(const auto&g:std::vector<Games>{Games{},Games{{}},Games{parse_moves("e2e4")},games})for(unsigned m=0;m<=9;++m){auto a=encode18(g,m,table);if(decode18(a.bytes,table)!=g)fail("roundtrip18");++checks;auto bad=a.bytes;bad.back()^=1;reject(bad,table);auto wrong=table;wrong[0]=wrong[0]==128?127:wrong[0]+1;reject(a.bytes,wrong);for(size_t n=0;n<a.bytes.size();++n)if(n<20||n+8>=a.bytes.size())reject(std::vector<uint8_t>(a.bytes.begin(),a.bytes.begin()+n),table);}
 auto a=encode18(games,0,table),b=encode(games,2,dmt17_prior_v1);if(a.payload_bits!=b.payload_bits||!std::equal(a.bytes.begin()+a.framing_bytes-4,a.bytes.end()-4,b.bytes.begin()+b.framing_bytes-4,b.bytes.end()-4))fail("R17 identity18");++checks;auto z0=encode18(games,0,{}),z9=encode18(games,9,{});if(z0.payload_bits!=z9.payload_bits||!std::equal(z0.bytes.begin()+z0.framing_bytes-4,z0.bytes.end()-4,z9.bytes.begin()+z9.framing_bytes-4,z9.bytes.end()-4))fail("zero18");++checks;std::cout<<"PASS "<<checks<<" checks; modes0..9, empty, CRC, truncation, wrong-table, R17 identity\n";return 0;}
 if(cmd=="blocks"){std::cout<<"block_size,mode,files,bits,bytes,mid_n,mid_cdf,roundtrip\n"<<std::setprecision(12);for(unsigned size:{1u,10u,30u,120u,unsigned(games.size())}){if(!size)continue;for(unsigned m:{0u,9u}){uint64_t bits=0,bytes=0,n=0,files=0;double loss=0;for(size_t begin=0;begin<games.size();begin+=size){Games g(games.begin()+begin,games.begin()+std::min(games.size(),begin+size));MidDiagnostics d;auto a=encode18(g,m,table,&d);if(decode18(a.bytes,table)!=g)fail("blocks18");++files;bits+=a.payload_bits;bytes+=a.bytes.size();n+=d.phase[1].moves;loss+=d.phase[1].logloss;}std::cout<<size<<','<<m<<','<<files<<','<<bits<<','<<bytes<<','<<n<<','<<(n?loss/n:0)<<",PASS\n"<<std::flush;}}return 0;}
 if(cmd=="pack"){if(argc!=6)fail("args");auto a=encode18(games,std::stoul(argv[4]),table);if(decode18(a.bytes,table)!=games)fail("pack18");std::ofstream f(argv[5],std::ios::binary);f.write((char*)a.bytes.data(),a.bytes.size());if(!f)fail("write18");return 0;}
 if(cmd=="speed"){if(argc!=5)fail("args");unsigned m=std::stoul(argv[4]);auto a=encode18(games,m,table);if(decode18(a.bytes,table)!=games)fail("warm18");auto t0=std::chrono::steady_clock::now();for(int k=0;k<3;++k)if(encode18(games,m,table).bytes!=a.bytes)fail("determinism18");auto t1=std::chrono::steady_clock::now();for(int k=0;k<3;++k)if(decode18(a.bytes,table)!=games)fail("decode18");auto t2=std::chrono::steady_clock::now();std::cout<<m<<','<<plies<<','<<std::chrono::duration<double>(t1-t0).count()/3<<','<<std::chrono::duration<double>(t2-t1).count()/3<<'\n';return 0;}
 if(cmd!="eval"||argc!=5)fail("eval args");std::cout<<"mode,games,plies,bits,bytes,mid_n,mid_cdf,open_cdf,end_cdf,roundtrip\n"<<std::setprecision(12);std::stringstream modes(argv[4]);std::string text;while(std::getline(modes,text,',')){unsigned m=std::stoul(text);MidDiagnostics d;auto a=encode18(games,m,table,&d);if(decode18(a.bytes,table)!=games||encode18(games,m,table).bytes!=a.bytes)fail("eval18");std::cout<<m<<','<<games.size()<<','<<plies<<','<<a.payload_bits<<','<<a.bytes.size()<<','<<d.phase[1].moves;for(unsigned stage:{1,0,2})std::cout<<','<<(d.phase[stage].moves?d.phase[stage].logloss/d.phase[stage].moves:0);std::cout<<",PASS\n"<<std::flush;}
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

#endif // DMT_R18_REFERENCE_ONLY
