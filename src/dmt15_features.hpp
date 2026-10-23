#pragma once
#include "dmt14_features.hpp"
#include <algorithm>
namespace rmc::detail {
// A knight's Chebyshev distance is always2; a king's is1 or2 (castling).
// These12 distance slots cannot occur for those pieces in standard legal play.
inline constexpr std::array<unsigned,4> r15_center_bank{104,105,107,108};
inline constexpr std::array<unsigned,8> r15_distance_bank{109,110,111,136,139,140,141,142};
// Pawn center has 4 slots, distance has 8. Split modes partition those slots;
// they do not add coefficients or a further probability distribution.
inline std::array<unsigned,2> r15_categories(const Position& p,Move m,
        const R14PawnFiles& files,unsigned mode,bool capture){
    const int x=m.to&7,y=m.to>>3;
    const std::array<unsigned,2> geometry{
        unsigned(std::min(3,std::min(std::abs(x-3),std::abs(x-4))+std::min(std::abs(y-3),std::abs(y-4)))),
        unsigned(std::max(std::abs(x-int(m.from&7)),std::abs(y-int(m.from>>3))))};
    if(mode==6||((mode==1||mode==8)&&capture))return geometry;
    auto out=r14_categories(p,m,files);
    if(mode<2)return out;
    const bool support=out[0]&1,isolated=out[1]&1,passed=out[1]&4;
    const bool source_open=!(files[p.side][m.from&7]&uint8_t(~(1u<<(m.from>>3))));
    if(mode>=7){
        if(mode==9&&capture)out={unsigned(source_open)+2*unsigned(passed),unsigned(isolated)+2*unsigned(bool(out[1]&2))+4*unsigned(support)};
        return out;
    }
    if(mode==2||mode==4||mode==5)
        out[0]=capture?2+unsigned(mode==5?support:source_open):unsigned(support);
    if(mode==3||mode==4||mode==5)
        out[1]=capture?4+unsigned(mode==5?source_open:support)+2*unsigned(passed):unsigned(isolated)+2*unsigned(passed);
    return out;
}
inline bool r15_capture_key(unsigned mode,unsigned key){
    if(mode>=7){
        for(auto x:r15_center_bank)if(x==key)return true;
        for(auto x:r15_distance_bank)if(x==key)return true;
        return false;
    }
    return ((mode==2||mode==4||mode==5)&&key>=50&&key<52)||
           ((mode==3||mode==4||mode==5)&&key>=100&&key<104);
}
} // namespace rmc::detail
