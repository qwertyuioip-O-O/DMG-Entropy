#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <stdexcept>
#include <iostream>
#include <vector>
using namespace rmc;
static void must(bool b,const char *what){if(!b)throw std::runtime_error(what);}
int main(){
 try{
    must(unsigned(MID_DEFAULT_PROFILE)==234,"main profile is not 234");
    auto g=parse_moves("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 ");
    std::vector<std::vector<Move>> games={g,g};
    auto def=mid_encode(games), exp=mid_encode(games,MidProfile::R10ComboTact4Full);
    must(def.bytes==exp.bytes,"implicit versus explicit default diverged");
    must(def.bytes[0]=='D'&&def.bytes[1]=='M'&&def.bytes[2]=='T'&&def.bytes[3]=='A'&&def.bytes[5]==234,"bad default header");
    must(mid_decode(def.bytes)==games,"default roundtrip failed");
    for(unsigned id: {119u,182u,231u,234u}){
        auto arc=mid_encode(games,MidProfile(id));
        must(mid_decode(arc.bytes)==games,"old/alternative model roundtrip failed");
        must(arc.bytes[5]==id,"profile header mismatch");
    }
    auto corruption=def.bytes;corruption.back()^=1;
    bool rejected=false;try{(void)mid_decode(corruption);}catch(...){rejected=true;}
    must(rejected,"CRC corruption was not detected");
    std::cout<<"main234: default/explicit identical, roundtrip, old profiles, CRC PASS\n";
 }catch(const std::exception& e){std::cerr<<"main234 FAIL: "<<e.what()<<"\n";return 1;}
}
