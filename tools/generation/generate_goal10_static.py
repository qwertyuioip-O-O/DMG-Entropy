#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[2];s=(r/'src/experiments/goal10/rmc_g10magic.cpp').read_text()
for old,new,count in [('uint64_t bit(int sq)', 'constexpr uint64_t bit(int sq)',1),('uint64_t mask_for_offset(', 'constexpr uint64_t mask_for_offset(',1),('static const auto table = []','static constexpr auto table = []',2)]:
 assert s.count(old)==count,(old,s.count(old));s=s.replace(old,new)
(r/'src/experiments/goal10/rmc_g10static.cpp').write_text(s)
