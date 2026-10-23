#pragma once
#include "rmc.hpp"
#include <array>
#include <cstdlib>
namespace rmc::detail {
inline unsigned r16_direction(const Position&p,Move m){
    const int dx=int(m.to&7)-int(m.from&7),dy=(int(m.to>>3)-int(m.from>>3))*(p.side?-1:1);
    const unsigned code=unsigned(((dy>0)-(dy<0)+1)*3+(dx>0)-(dx<0)+1);
    return code>4?code-1:code;
}
inline std::array<uint8_t,5> r16_keys(const Position&p,Move m,
        const std::array<uint8_t,5>&old,unsigned layout,unsigned context,
        bool capture,bool attacked,bool supported){
    auto keys=old;const unsigned piece=unsigned(std::abs(int(p.board[m.from]))-1);
    if(layout==1||layout==5){
        keys={uint8_t(m.from^(p.side?56:0)),
              uint8_t(64+(layout==1?unsigned(old[0]):piece*8+r16_direction(p,m))),
              uint8_t(112+context*12+piece*2+unsigned(capture)),
              uint8_t(136+unsigned(old[1])-48),
              uint8_t(160+unsigned(attacked)+2*unsigned(supported)+4*unsigned(capture))};
    }else if(layout>=2&&layout<=4){
        const unsigned value=layout==2?unsigned(m.from&7):layout==3?unsigned(p.side?7-(m.from>>3):m.from>>3):r16_direction(p,m);
        keys[3]=uint8_t(96+piece*8+value);
    }
    return keys;
}
} // namespace rmc::detail
