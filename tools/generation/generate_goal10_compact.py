#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2]
s=(r/'src/entropy_g10pages.cpp').read_text().replace('goal10_op_table.hpp','goal10_compact_table.hpp').replace('Goal10OpTable<Opportunity>','Goal10CompactOpTable<Opportunity>')
(r/'src/entropy_g10compact.cpp').write_text(s)
for old,new,incold,incnew in [('r17_g10pages.cpp','r17_g10compact.cpp','entropy_g10pages.cpp','entropy_g10compact.cpp'),('r18_g10pages.cpp','r18_g10compact.cpp','r17_g10pages.cpp','r17_g10compact.cpp'),('r19_g10pages_model.hpp','r19_g10compact_model.hpp','r18_g10pages.cpp','r18_g10compact.cpp'),('r19_g10pages.cpp','r19_g10compact.cpp','r19_g10pages_model.hpp','r19_g10compact_model.hpp'),('test_goal10_pages.cpp','test_goal10_compact.cpp','goal10_op_table.hpp','goal10_compact_table.hpp')]:
 s=(r/'tools/native'/old).read_text().replace(incold,incnew)
 if new=='test_goal10_compact.cpp':s=s.replace('Goal10OpTable','Goal10CompactOpTable')
 (r/'tools/native'/new).write_text(s)
print('Generated compact paged opportunity candidate')
