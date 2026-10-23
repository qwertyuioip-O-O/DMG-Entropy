#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1];s=(r/'src/entropy_g10group.cpp').read_text()
assert s.count('Goal10OpTable<Opportunity>')==2
s=s.replace('goal10_op_table.hpp','goal10_compact_table.hpp').replace('Goal10OpTable<Opportunity>','Goal10CompactOpTable<Opportunity>');(r/'src/entropy_g10combo.cpp').write_text(s)
for old,new,incold,incnew in [('r17_g10group.cpp','r17_g10combo.cpp','entropy_g10group.cpp','entropy_g10combo.cpp'),('r18_g10group.cpp','r18_g10combo.cpp','r17_g10group.cpp','r17_g10combo.cpp'),('r19_g10group_model.hpp','r19_g10combo_model.hpp','r18_g10group.cpp','r18_g10combo.cpp'),('r19_g10group.cpp','r19_g10combo.cpp','r19_g10group_model.hpp','r19_g10combo_model.hpp'),('test_goal10_group.cpp','test_goal10_combo.cpp','entropy_g10group.cpp','entropy_g10combo.cpp')]:
 (r/'tools'/new).write_text((r/'tools'/old).read_text().replace(incold,incnew))
print('Generated grouped-attacks + compact-pages combination; native update division retained')
