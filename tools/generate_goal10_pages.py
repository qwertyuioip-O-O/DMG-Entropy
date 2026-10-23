#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1];s=(r/'src/entropy_r19_scratch.cpp').read_text()
s='#include "goal10_op_table.hpp"\n'+s
for old,new in [('std::unordered_map<uint32_t,Opportunity> geom_op;','Goal10OpTable<Opportunity> geom_op;'),('mutable std::unordered_map<uint32_t,Opportunity> square_op;','mutable Goal10OpTable<Opportunity> square_op;')]:
 assert s.count(old)==1;s=s.replace(old,new)
(r/'src/entropy_g10pages.cpp').write_text(s)
for old,new,incold,incnew in [('r17_scratch.cpp','r17_g10pages.cpp','entropy_r19_scratch.cpp','entropy_g10pages.cpp'),('r18_scratch.cpp','r18_g10pages.cpp','r17_scratch.cpp','r17_g10pages.cpp'),('r19_scratch_model.hpp','r19_g10pages_model.hpp','r18_scratch.cpp','r18_g10pages.cpp'),('r19_scratch.cpp','r19_g10pages.cpp','r19_scratch_model.hpp','r19_g10pages_model.hpp')]:
 (r/'tools'/new).write_text((r/'tools'/old).read_text().replace(incold,incnew))
print('Generated paged opportunity table candidate directly from fixed anchor')
