#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
// Bounded integer-key opportunity table. Page addresses never move.
template<class Value> class Goal10OpTable {
    static constexpr uint32_t shift=8, page_size=1u<<shift, key_limit=1u<<21;
public:
    struct Cell { Value second{}; };
private:
    struct Page { std::array<Cell,page_size> cells{}; std::array<uint64_t,4> used{}; };
    std::vector<std::unique_ptr<Page>> pages;
    static void check(uint32_t key){if(key>=key_limit)throw std::out_of_range("goal10 opportunity key");}
public:
    Goal10OpTable()=default;
    Goal10OpTable(const Goal10OpTable& other){
        if(!other.pages.empty()){pages.resize(other.pages.size());for(size_t i=0;i<pages.size();++i)if(other.pages[i])pages[i]=std::make_unique<Page>(*other.pages[i]);}
    }
    Goal10OpTable& operator=(const Goal10OpTable& other){if(this!=&other){Goal10OpTable copy(other);pages.swap(copy.pages);}return *this;}
    Goal10OpTable(Goal10OpTable&&)=default;
    Goal10OpTable& operator=(Goal10OpTable&&)=default;
    Value& operator[](uint32_t key){
        check(key);if(pages.empty())pages.resize(key_limit>>shift);
        auto& p=pages[key>>shift];if(!p)p=std::make_unique<Page>();
        const uint32_t i=key&(page_size-1);p->used[i>>6]|=uint64_t(1)<<(i&63);
        return p->cells[i].second;
    }
    const Cell* find(uint32_t key)const{
        check(key);if(pages.empty())return nullptr;
        const auto& p=pages[key>>shift];if(!p)return nullptr;
        const uint32_t i=key&(page_size-1);return (p->used[i>>6]&(uint64_t(1)<<(i&63)))?&p->cells[i]:nullptr;
    }
    const Cell* end()const{return nullptr;}
};
