#include "../src/goal10_compact_table.hpp"
#include <unordered_map>
#include <random>
#include <iostream>
struct Value {uint32_t expected=0,actual=0;bool operator==(const Value&)const=default;};
int main(){
 Goal10CompactOpTable<Value> fast;std::unordered_map<uint32_t,Value> ref;std::mt19937 rng(20261023);uint64_t checks=0;
 auto check=[&](uint32_t k){auto a=fast.find(k);auto b=ref.find(k);if((a==fast.end())!=(b==ref.end())||(a&&a->second!=b->second))throw std::runtime_error("page table mismatch");++checks;};
 auto* stable=&fast[7];ref[7]={};stable->expected=123;ref[7].expected=123;
 for(unsigned i=0;i<100000;++i){uint32_t k=rng()&((1u<<21)-1);check(k);auto delta=rng()%1024;
  if(i%3){fast[k].expected+=delta;ref[k].expected+=delta;}else{fast[k].actual+=delta;ref[k].actual+=delta;}
  check(k);if(&fast[7]!=stable||stable->expected!=123)throw std::runtime_error("pointer invalidation");
 }
 Goal10CompactOpTable<Value> copy=fast;Goal10CompactOpTable<Value> assigned;assigned=copy;
 for(const auto&[k,v]:ref){if(copy.find(k)->second!=v||assigned.find(k)->second!=v)throw std::runtime_error("copy mismatch");++checks;}
 copy[7].expected=456;if(fast[7].expected!=123)throw std::runtime_error("copy alias");
 bool rejected=false;try{fast[1u<<21];}catch(const std::out_of_range&){rejected=true;}if(!rejected)throw std::runtime_error("range check");
 Goal10CompactOpTable<Value> moved=std::move(copy);if(moved[7].expected!=456)throw std::runtime_error("move mismatch");
 std::cout<<"PASS "<<checks<<" map differential checks; stable pointers, zero presence, deep copy, move, key bounds\n";
}
