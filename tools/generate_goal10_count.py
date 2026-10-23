#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1];s=(r/'src/entropy_g10group.cpp').read_text()
a=s.index('std::array<uint16_t,256> r4_rank_features(');b=s.index('\n// DMT5:',a)
old=s[a:b];new=old.replace('r4_rank_features(','r4_rank_features_count(',1)
x=new.index('    std::sort(');y=new.index('    unsigned processed=0;',x)
new=new[:x]+'''    // Counting scatter creates contiguous score runs, avoiding comparator sorting.
    std::array<unsigned,42> count{},offset{};
    for(unsigned i=0;i<n;++i)++count[unsigned(order[i].score+15)];
    unsigned cursor=0;
    for(int k=41;k>=0;--k){offset[k]=cursor;cursor+=count[k];}
    std::array<RankItem,256> sorted;
    for(unsigned i=0;i<n;++i)sorted[offset[unsigned(order[i].score+15)]++]=order[i];
    std::copy_n(sorted.begin(),n,order.begin());
'''+new[y:]
new=new.replace('if(score[i]<-64||score[i]>64)','if(score[i]<-15||score[i]>26)')
# Keep the reference only in verification builds.
wrapper='''
std::array<uint16_t,256> r4_rank_features(const Position& p,const V8Step& base,const std::array<uint16_t,256>& tactics){
    auto result=r4_rank_features_count(p,base,tactics);
#ifdef DMT_VERIFY_COUNT
    if(result!=r4_rank_features_reference(p,base,tactics))fail("counted rank mismatch");
#endif
    return result;
}
'''
s=s[:a]+'#ifdef DMT_VERIFY_COUNT\n'+old.replace('r4_rank_features(','r4_rank_features_reference(',1)+'\n#endif\n'+new+wrapper+s[b:]
(r/'src/entropy_g10count.cpp').write_text(s)
for name in ['r17_g10group.cpp','r18_g10group.cpp','r19_g10group_model.hpp','r19_g10group.cpp']:
 (r/'tools'/name.replace('g10group','g10count')).write_text((r/'tools'/name).read_text().replace('g10group','g10count'))
