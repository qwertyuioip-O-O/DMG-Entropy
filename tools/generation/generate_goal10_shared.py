#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2]
s=(r/'src/entropy_g10group.cpp').read_text()
start=s.index('    void observe(const std::array<uint16_t,256>& f,const V6Weights& model,unsigned n,unsigned chosen,bool adaptive,unsigned explicit_rate=0){',s.index('struct MidResidual'))
end=s.index('\n};',start)
old=s[start:end]
body=old[old.index('        for(unsigned k=0;k<MID_F;++k){'):]
body=body.replace('            int32_t expected=int32_t(divide(uint64_t(sums[k])*4096+model.total/2));','            int32_t expected=expectations[k];').replace('(f[chosen]&(1u<<k))','(chosen_flags&(1u<<k))')
prepared='    void observe_prepared(const std::array<int32_t,MID_F>& expectations,uint16_t chosen_flags,bool adaptive,unsigned explicit_rate){\n'+body
s=s[:end]+'\n'+prepared+s[end:]
needle='    const MidProfile r11profile=profile;'
pos=s.index(needle,s.index('void mid_observe_legacy'))
helper='    std::array<int32_t,MID_F> expected_cache{};\n    bool expected_ready=false;\n    auto shared_observe=[&](MidResidual& learner,bool adaptive,unsigned rate){\n#ifdef DMT_VERIFY_SHARED\n        auto oracle=learner;\n        oracle.observe(s.additional,s.coded,s.base.moves.size,index,adaptive,rate);\n#endif\n        if(!expected_ready){\n            std::array<uint32_t,MID_F> sums{};\n            for(unsigned i=0;i<s.base.moves.size;++i){\n                unsigned fs=s.additional[i];\n                while(fs){unsigned k=std::countr_zero(fs);fs&=fs-1;sums[k]+=s.coded.weights[i];}\n            }\n            const ExactDivisor divide(s.coded.total);\n            for(unsigned k=0;k<MID_F;++k)\n                expected_cache[k]=int32_t(divide(uint64_t(sums[k])*4096+s.coded.total/2));\n            expected_ready=true;\n        }\n        learner.observe_prepared(expected_cache,s.additional[index],adaptive,rate);\n#ifdef DMT_VERIFY_SHARED\n        if(learner.theta!=oracle.theta || learner.rest!=oracle.rest || learner.seen!=oracle.seen || learner.steps!=oracle.steps)\n            throw std::runtime_error("shared observation state mismatch");\n#endif\n    };\n'
s=s[:pos]+helper+s[pos:]
s=s.replace('ctx.mid.observe(s.additional,s.coded,s.base.moves.size,index,','shared_observe(ctx.mid,')
s=s.replace('ctx.context_residual[s.context].observe(\n            s.additional,s.coded,s.base.moves.size,index,false,','shared_observe(ctx.context_residual[s.context],false,')
(r/'src/entropy_g10shared.cpp').write_text(s)
for name in ['r17_g10group.cpp','r18_g10group.cpp','r19_g10group_model.hpp','r19_g10group.cpp']:
 (r/'tools/native'/name.replace('g10group','g10shared')).write_text((r/'tools/native'/name).read_text().replace('g10group','g10shared'))
