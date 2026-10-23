#pragma once
#include <cstdint>
namespace rmc::detail {
// For d>1, reciprocal=floor(2^32/d). x<2^32 implies the estimate is
// at most one below floor(x/d); one unsigned remainder correction is exact.
inline uint32_t r16_divide_u32(uint32_t x,uint32_t d,uint32_t reciprocal){
    const uint32_t q=uint32_t((uint64_t(x)*reciprocal)>>32);
    return q+uint32_t(x-q*d>=d);
}
} // namespace rmc::detail
