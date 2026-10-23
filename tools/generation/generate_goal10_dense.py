#!/usr/bin/env python3
"""Apply exact compact opportunity pages to the retained index-rank build."""
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'src/experiments/goal10/entropy_g10index.cpp').read_text()
assert s.count('Goal10OpTable<Opportunity>')==2
s=s.replace('experiments/goal10/goal10_op_table.hpp','experiments/goal10/goal10_compact_table.hpp').replace('Goal10OpTable<Opportunity>','Goal10CompactOpTable<Opportunity>')
(r/'src/experiments/goal10/entropy_g10dense.cpp').write_text(s)
for name in ['r17_g10index.cpp','r18_g10index.cpp','r19_g10index_model.hpp','r19_g10index.cpp']:
 (r/'tools/native'/name.replace('g10index','g10dense')).write_text((r/'tools/native'/name).read_text().replace('g10index','g10dense'))
