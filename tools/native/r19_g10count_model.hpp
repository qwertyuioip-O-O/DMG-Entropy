#pragma once
#define DMT_R18_REFERENCE_ONLY
#include "r18_g10count.cpp"
#undef DMT_R18_REFERENCE_ONLY
#include "../../src/dmt19_exp.hpp"
#include "../../src/dmt19_prior.hpp"
using Table19=std::array<int16_t,720>;
Table19 load19(const char*path){Table19 a{};if(std::string(path)=="builtin")return dmt19_prior_v1;if(std::string(path)=="zero")return a;std::ifstream f(path);int v;for(auto&x:a){if(!(f>>v)||v< -256||v>256)fail("table19");x=int16_t(v);}if(f>>v)fail("table19 extra");return a;}
uint32_t id19(const Table19&t){std::vector<uint8_t>b;for(int v:t){b.push_back(uint8_t(v));b.push_back(uint8_t(uint16_t(v)>>8));}return v6_crc(b.data(),b.size());}
unsigned response19(uint32_t f,unsigned piece){return piece*8+unsigned(bool(f&(1u<<5)))+2*unsigned(bool(f&(1u<<1)))+4*unsigned(bool(f&(1u<<8)));}
unsigned direct19(unsigned destination,unsigned piece){return piece*8+(destination&7);}
// Reproduce exactly the R14 layout0 keys, directly on the upstream joint CDF.
PriorStep base19(const Position&p,const Move*prev,const Move*own,unsigned ply,Context18&c){
 PriorStep s;mid_step_legacy_into(s,p,prev,own,ply,1,MidProfile::R10ComboTact4Full,c.core);
 auto files=detail::r14_pawn_files(p);
 for(unsigned i=0;i<s.base.moves.size;++i){auto m=s.base.moves.a[i].m;unsigned piece=unsigned(abs_piece(p.board[m.from])-1),rank=p.side?7-(m.to>>3):(m.to>>3);int x=m.to&7,y=m.to>>3;unsigned center=unsigned(std::min(3,std::min(std::abs(x-3),std::abs(x-4))+std::min(std::abs(y-3),std::abs(y-4))));unsigned distance=unsigned(std::max(std::abs(x-int(m.from&7)),std::abs(y-int(m.from>>3))));auto f=s.base.flags[i];auto&k=s.r12_features[i];k={uint8_t(piece*8+rank),uint8_t(48+piece*4+center),uint8_t(72+piece*4+unsigned(bool(f&(1u<<2)))+2*unsigned(bool(f&(1u<<3)))),uint8_t(96+piece*8+distance),uint8_t(144+piece*4+unsigned(bool(f&(1u<<4)))+2*unsigned(bool(f&(1u<<8))))};if(piece==0||piece==3){auto categories=detail::r14_categories(p,m,files);k[1]=uint8_t(48+piece*4+categories[0]);k[3]=uint8_t(96+piece*8+categories[1]);}}
 return s;
}
PriorStep step19(const Position&p,const Move*prev,const Move*own,unsigned ply,unsigned stage,Context18&c,unsigned mode,const Table19&t,bool){
 if(mode==0||stage!=1)return step18(p,prev,own,ply,stage,c,9,dmt18_prior_v1,true);
 auto s=base19(p,prev,own,ply,c);std::array<uint64_t,256>raw;
 for(unsigned i=0;i<s.base.moves.size;++i){unsigned offset=168*bucket18(s.base.flags[i]);int score=0;
  if(mode==1){int global=0,local=0;for(auto k:s.r12_features[i]){global+=dmt17_prior_v1[k];local+=dmt18_prior_v1[offset+k];}global=std::clamp(global,-128,128);local=std::clamp(local,-128,128);raw[i]=uint64_t(s.coded.weights[i])*v8_exp2_lut[global+128]*v8_exp2_lut[local+128];}
  else{for(auto k:s.r12_features[i])score+=t[offset+k];unsigned piece=unsigned(abs_piece(p.board[s.base.moves.a[i].m.from])-1);if(mode==3)score+=t[672+response19(s.base.flags[i],piece)];if(mode==4)score+=t[672+direct19(s.base.moves.a[i].m.to,piece)];score=std::clamp(score,-256,256);raw[i]=uint64_t(s.coded.weights[i])*dmt19_exp[score+256];}
 }s.coded=v6_quantize(raw,s.base.moves.size);return s;
}
void observe19(Context18&c,const Position&p,const PriorStep&s,unsigned ix,unsigned ply,unsigned stage,unsigned){observe18(c,p,s,ix,ply,stage,9);}
void analyze19(const Games&games){
 Context18 c;std::array<uint64_t,8>n{};std::array<double,8>loss{};uint64_t checked=0;
 for(const auto&g:games){Position p=Position::standard();Move prev{},own[2]{};bool hp=false,ho[2]={};for(unsigned ply=0;ply<g.size();++ply){unsigned stage=V7Patterns::phase(p,ply),side=p.side;auto s=step18(p,hp?&prev:nullptr,ho[side]?&own[side]:nullptr,ply,stage,c,9,dmt18_prior_v1,true);auto ix=index_of(s.base.moves,g[ply]);if(stage==1){auto z=base19(p,hp?&prev:nullptr,ho[side]?&own[side]:nullptr,ply,c);if(s.base.moves.size!=z.base.moves.size)fail("oracle count19");for(unsigned i=0;i<z.base.moves.size;++i){if(s.r12_features[i]!=z.r12_features[i]||s.base.moves.a[i].m!=z.base.moves.a[i].m||s.base.flags[i]!=z.base.flags[i])fail("oracle key19");++checked;}unsigned piece=unsigned(abs_piece(p.board[g[ply].from])-1),sig=response19(s.base.flags[ix],piece)%8;++n[sig];loss[sig]+=std::log2(double(s.coded.total)/s.coded.weights[ix]);}observe18(c,p,s,ix,ply,stage,9);p.apply_unchecked(g[ply]);own[side]=g[ply];ho[side]=true;prev=g[ply];hp=true;}}
 std::cout<<"signature,count,mean_bits\n"<<std::setprecision(12);for(unsigned k=0;k<8;++k)std::cout<<k<<','<<n[k]<<','<<(n[k]?loss[k]/n[k]:0)<<'\n';std::cerr<<"PASS "<<checked<<" candidate key/order/flag oracle comparisons\n";
}
