#!/usr/bin/env python3
"""Summarize alternating paired timings; percent refers to throughput."""
import argparse,csv,json,statistics
from pathlib import Path
r=Path(__file__).resolve().parents[1];out=r/'benchmarks-r13'
ap=argparse.ArgumentParser();ap.add_argument('--suffix',default='');ap.add_argument('--label',default='선택용 153판/10,270수');ap.add_argument('--directory',type=Path,default=out);args=ap.parse_args();out=args.directory
rows=list(csv.DictReader((out/f'paired-speed{args.suffix}.csv').open()));assert len(rows)==22
metrics={}
for column in ['encode_wall_s','decode_wall_s','encode_cpu_s','decode_cpu_s']:
    pairs=[]
    for round in range(11):
        xs={int(x['mask']):float(x[column]) for x in rows if int(x['round'])==round}
        pairs.append((xs[-1]/xs[95]-1)*100)
    metrics[column]={'median_paired_throughput_change_pct':statistics.median(pairs),'min_pct':min(pairs),'max_pct':max(pairs),'candidate_faster_rounds':sum(x>0 for x in pairs),'rounds':len(pairs),'all_paired_pct':pairs}
(out/f'speed-summary{args.suffix}.json').write_text(json.dumps(metrics,indent=2))
lines=['# R13 속도 실측','',
       args.label+'. 같은 O3 빌드에서 250과 251/mask95를 비교했다.',
       '11라운드에서 순서를 번갈아 실행하고 각 항목은 인코드/디코드 2회 평균 시간이다.',
       '측정 전에 왕복 복원을 확인했고, 측정 중에도 인코드 결정성과 디코드 결과를 확인했다.',
       '컴파일·다른 벤치마크·회귀 테스트는 측정 전에 모두 종료했다.',
       '표의 변화율은 각 라운드 처리량 비율의 중앙값이며 단일 환경 측정이다.','',
       '| 지표 | 처리량 변화 중앙값 | 라운드 최솟값~최댓값 | 후보가 빠른 라운드 |',
       '|---|---:|---:|---:|']
for name,col in [('인코드 wall','encode_wall_s'),('디코드 wall','decode_wall_s'),('인코드 CPU','encode_cpu_s'),('디코드 CPU','decode_cpu_s')]:
    x=metrics[col];lines.append(f"| {name} | {x['median_paired_throughput_change_pct']:+.2f}% | {x['min_pct']:+.2f}% ~ {x['max_pct']:+.2f}% | {x['candidate_faster_rounds']}/11 |")
lines+=['',f'원시 측정은 paired-speed{args.suffix}.csv, 각 비율은 speed-summary{args.suffix}.json에 있다.',
        '초기 4회 호출은 speed-select.csv에 따로 남겼다. 편차가 컸으므로 그 결과만으로 판정하지 않았다.',
        '차이가 작고 실행 환경 변동이 있으므로 여러 하드웨어에서 재검증하기 전 보편적 속도 개선으로 단정하지 않는다.']
(out/f'speed-results{args.suffix}.md').write_text('\n'.join(lines)+'\n');print('\n'.join(lines))
