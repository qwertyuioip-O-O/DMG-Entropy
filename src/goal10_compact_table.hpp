#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
// Promotion-zero keys use a dense directory; uncommon keys retain their full ID.
template<class Value> class Goal10CompactOpTable {
    static constexpr uint32_t shift=8,page_size=1u<<shift,key_limit=1u<<21;
public:
    struct Cell { Value second{}; };
private:
    struct Page { std::array<Cell,page_size> cells{};std::array<uint64_t,4> used{}; };
    using Directory=std::vector<std::unique_ptr<Page>>;
    Directory ordinary,promoted;
    static void check(uint32_t key){if(key>=key_limit)throw std::out_of_range("goal10 compact opportunity key");}
    static Directory clone(const Directory& source){
        Directory result(source.size());for(std::size_t i=0;i<source.size();++i)if(source[i])result[i]=std::make_unique<Page>(*source[i]);return result;
    }
public:
    Goal10CompactOpTable()=default;
    Goal10CompactOpTable(const Goal10CompactOpTable& other):ordinary(clone(other.ordinary)),promoted(clone(other.promoted)){}
    Goal10CompactOpTable& operator=(const Goal10CompactOpTable& other){if(this!=&other){Goal10CompactOpTable copy(other);ordinary.swap(copy.ordinary);promoted.swap(copy.promoted);}return *this;}
    Goal10CompactOpTable(Goal10CompactOpTable&&) noexcept=default;
    Goal10CompactOpTable& operator=(Goal10CompactOpTable&&) noexcept=default;
    Value& operator[](uint32_t key){
        check(key);const bool promo=key&7;const uint32_t index=promo?key:(key>>3);
        auto& directory=promo?promoted:ordinary;
        if(directory.empty())directory.resize((promo?key_limit:(key_limit>>3))>>shift);
        auto& p=directory[index>>shift];if(!p)p=std::make_unique<Page>();
        const uint32_t i=index&(page_size-1);p->used[i>>6]|=uint64_t(1)<<(i&63);return p->cells[i].second;
    }
    const Cell* find(uint32_t key)const{
        check(key);const bool promo=key&7;const uint32_t index=promo?key:(key>>3);
        const auto& directory=promo?promoted:ordinary;if(directory.empty())return nullptr;
        const auto& p=directory[index>>shift];if(!p)return nullptr;
        const uint32_t i=index&(page_size-1);return (p->used[i>>6]&(uint64_t(1)<<(i&63)))?&p->cells[i]:nullptr;
    }
    const Cell* end()const{return nullptr;}
};
