#!/usr/bin/env python3
"""Generate isolated direct-output experiment from frozen originals."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/entropy.cpp').read_text()
args='const Position& p,const Move* previous,const Move* own_previous,\n                 unsigned ply,unsigned stage,MidProfile profile,const MidContext& ctx'
call='p,previous,own_previous,ply,stage,profile,ctx'
for name,end in [('mid_step_legacy','void mid_observe_legacy'),('mid_step','void mid_observe(')]:
 start=s.index('MidStep '+name+'('+args+')');stop=s.index(end,start)
 # Only transform the function itself, preserving interstitial declarations.
 brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 original=s[start:i];body=original[original.index('{')+1:-1]
 if name=='mid_step_legacy':
  assert body.count('    MidStep result;')==1;body=body.replace('    MidStep result;','    // Caller supplies a freshly initialized result; no temporary MidStep.')
 else:
  body=body.replace('return mid_step_legacy('+call+');','{mid_step_legacy_into(result,'+call+');return;}')
  old='auto result=mid_step_legacy(p,previous,own_previous,ply,stage,ctx.config.base?MidProfile::R11Joint40Rate2:MidProfile::R10ComboTact4Full,ctx);'
  assert old in body;body=body.replace(old,'mid_step_legacy_into(result,p,previous,own_previous,ply,stage,ctx.config.base?MidProfile::R11Joint40Rate2:MidProfile::R10ComboTact4Full,ctx);')
 body=body.replace('return result;','return;')
 transformed='void '+name+'_into(MidStep& result,'+args+'){'+body+'}\nMidStep '+name+'('+args+'){MidStep result;'+name+'_into(result,'+call+');return result;}'
 s=s[:start]+transformed+s[i:]
(r/'src/entropy_r19_inplace.cpp').write_text(s)
s=(r/'tools/r17_prior.cpp').read_text().replace('../src/entropy.cpp','../src/entropy_r19_inplace.cpp')
old='PriorStep s;static_cast<MidStep&>(s)=mid_step(p,prev,own,ply,stage,MidProfile::R16SourceModel,c);';assert old in s
s=s.replace(old,'PriorStep s;mid_step_into(s,p,prev,own,ply,stage,MidProfile::R16SourceModel,c);')
(r/'tools/r17_inplace.cpp').write_text(s)
s=(r/'tools/r18_prior.cpp').read_text().replace('"r17_prior.cpp"','"r17_inplace.cpp"');(r/'tools/r18_inplace.cpp').write_text(s)
s=(r/'tools/r19_model.hpp').read_text().replace('"r18_prior.cpp"','"r18_inplace.cpp"')
old='PriorStep s;static_cast<MidStep&>(s)=mid_step_legacy(p,prev,own,ply,1,MidProfile::R10ComboTact4Full,c.core);';assert old in s
s=s.replace(old,'PriorStep s;mid_step_legacy_into(s,p,prev,own,ply,1,MidProfile::R10ComboTact4Full,c.core);')
(r/'tools/r19_inplace_model.hpp').write_text(s)
s=(r/'tools/r19_prior.cpp').read_text().replace('"r19_model.hpp"','"r19_inplace_model.hpp"');(r/'tools/r19_inplace.cpp').write_text(s)
print('Generated isolated direct-output pipeline; originals unchanged')
