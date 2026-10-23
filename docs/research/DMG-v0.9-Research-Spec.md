# DMG-Entropy v0.9 연구 및 구현 명세

2026-10-20 · 분석 대상: 첨부 DMG-Entropy-Cpp-v0.8.zip

## 판정 요약과 증거 범위

**첫 v0.9 는 새 특징을 많이 붙이는 코덱이 아니라, 공유 공격 커널·합법수 빠른 경로·착수에 의한 관계 차분·작은 정수 학습기로 구성할 것을 권고한다.** 현재 DMG8 을 같은 CDF로 더 빨리 실행하는 작업을 먼저 분리한다. 그 다음 별도 DMG9 프로필에서 압축 모델을 바꾼다.

이번 연구에서 실제로 한 일은 소스 분석, 기존 23 게임의 재압축·복원, gprof 계측, 특징 통계, 정확한 공격 커널 및 계산 재사용 시제품 제작이다. 수천 개의 새 인간 기보는 확보하지 않았다. 3.9 bit/반수의 독립 달성이나 v0.7 처리량 회복을 입증하지 않았다. 아래의 미래 성능은 가설이고, 숫자는 측정값·설계 상한·목표를 구분한다.

최종 보고서는 요청한 A～H 순서를 따른다. 뒤의 재현 부록에는 실제 시험한 변경과 검증 범위를 기록했다.

## A. v0.8 병목: 소스와 측정

### A1. 호출 구조와 중복

| 위치: src/entropy.cpp | 실제 처리 | 낭비 또는 개선 지점 |
|---|---|---|
| AttackCache, 약 251 행 | 모든 상대 기물을 순회하고 ray를 칸별 스캔 | 조회 수요와 무관하게 전체 지도를 생성 |
| candidates, 약 400 행 | 의사합법수·baseline·체크 여부 계산 | 이후 v8 에서 합법성·일부 관계를 재계산 |
| direct_check_bonus, 약 380 행 | 후보마다 복사·가상착수·상대 왕 공격 검사 | v8_relations에서 체크 여부를 다시 계산 |
| v8_relations, 약 1175 행 | 후보마다 자기·상대 전체 공격 지도 | 상대 지도는 목적지 한 칸 조회에만 사용 |
| v8_step, 약 1281 행 | 모든 후보의 flag, A/E, joint, uniform, exact 계산 | joint 단독 모드에도 불필요한 전문가가 생성됨 |
| V8Learn::observe, 약 1237 행 | 24 개 특징의 joint/base 기대값·잔차·통계 갱신 | joint 모드에 예측상 불필요한 A/E 통계도 유지 |
| v8_observe, 약 1345 행 | 패턴·정확 상태 사전 갱신 | exact를 사용하지 않는 단독 모드에서도 계속 저장 |
| V7Patterns 키 생성 | 매 후보에 phase·기물수 등을 재계산 | 현재 판과 ply에 공통인 계산을 공유 가능 |
| Position::apply_checked, rmc.cpp 약 348 행 | pseudo_destinations 재검사와 내부 Position 복사 | 이미 생성된 후보를 검사하는 v8 경로에서 반복 |

Position 복사가 무조건 주범이라는 결론은 아니다. 실제 프로파일과 제거 실험을 함께 봐야 한다. 상태 복사보다 전체 공격 생성·중복 체크·정수 나눗셈이 더 비쌀 수 있다.

### A2. 재현 환경과 함수 수준 프로파일

- CPU: 가상화 환경에서 표시된 AMD EPYC 9V74. 단일 스레드 프로세스.
- g++ 13.3.0, C++20, 비계측 처리량 빌드는 `-O3 -DNDEBUG`, `-march=native` 미사용.
- 첨부 examples/heldout/*.uci 23 게임을 파일명 정렬 순서로 하나의 archive에 넣음. 총 1,718 반수.
- 처리량은 archive를 매번 cold state에서 다시 실행해 trial당 16 회,7 개 trial의 중앙값. 각 실행의 byte 동일성·복원 일치도 확인함. 입력 텍스트 읽기는 타이밍 밖.
- 별도 `-pg` 빌드로 함수 계측. 이 빌드의 속도를 정상 처리량과 비교하지 않음. perf 실행파일은 이 환경에 없었으므로 하드웨어 카운터를 측정하지 않았음.

원본 joint의 gprof flat self-time은 v8_step 약 57.2%, square_attacked 약 11.2%, candidates 약 4.2%, v7_opportunity 약 4.2%, apply_checked 약 2.9%, v8_observe 약 2.9%였다. v8_step에는 인라인된 관계·공격·확률 계산이 포함되므로 **57.2% 전체를 공격 지도 시간이라고 부를 수 없다.** 계측 오버헤드·컴파일러 인라인·표본화 오차가 있는 구조 진단이며 정확한 phase 분해를 대신하지 않는다. 산술 부호기 자체는 이 프로파일의 큰 항목이 아니었다.

### A3. 실제 시험한 정확 최적화

1. 모드별로 쓰지 않는 전문가의 계산·정확사전 저장을 생략하고 A/E의 특징별 비율을 한 번만 계산.
2. 기물별 knight/king/pawn 테이블과 방향별 ray·첫 blocker 검색으로 같은 공격 마스크 생성. Magic/PEXT는 사용하지 않음.
3. 후보 후 상대 전체 지도를 만들지 않고 목적지 한 칸의 square_attacked를 질의.
4. 핀을 한 번 구하고, 비체크·비왕·비앙파상·비핀 후보는 자기 왕 검사를 생략. 체크 중 후보는 기존 candidates의 엄격 필터 결과를 사용. 왕·앙파상·핀·캐슬링은 정확한 검사 경로 유지.
5. 패턴 키·phase 계산 재사용 및 ‘공격받는 대상이 두 개 이상인가’ 질의의 불필요한 정확 popcount 제거를 추가 시험.

각 단계는 같은 수학적 특징·CDF를 재현하는 시제품이다. 모델을 바꾼 것이 아니므로 압축률 개선은 0 이며, DMG8 byte 호환 최적화로 평가한다.

| 구현 | Payload bit | 총bytes | 인코딩 Mply/s | 디코딩 Mply/s |
|---|---:|---:|---:|---:|
| v0.7 기준, 측정 전/후 | 7,085 | 923 | 0.2163 / 0.2072 | 0.2203 / 0.2098 |
| v0.8 원본 joint | 6,857 | 895 | 0.05352 | 0.05486 |
| 불필요 전문가 제거·비율 hoist | 6,857 | 895 | 0.06299 | 0.06312 |
| 위 변경 + ray 공격 커널 | 6,857 | 895 | 0.07852 | 0.07877 |
| 위 변경 + 단일 칸 질의·핀 빠른 경로 | 6,857 | 895 | 0.09005 | 0.09060 |
| 위 변경 + 키·phase 재사용 | 6,857 | 895 | **0.09895** | **0.09904** |

표의 수치는 모두 실제 결과다. 같은 VM의 작은 자료이며 서버 부하·코어 이동의 영향을 포함한다. 기준 v7 을 전후에 측정해 약 4～5% 변동도 확인했다. 최종 약 0.099 수준의 시제품은 원본보다 인코딩 약 1.85 배·디코딩 약 1.81 배 빨랐지만, **v0.7 처리량에는 아직 도달하지 않았다.** 새 지표를 원래 보고서의 다른 빌드 속도와 직접 섞지 않는다.

기존 DMG8 테스트를 실행했고, 공격 커널을 64 칸의 기존 square_attacked oracle와 비교하는 검증을 켰다. 빠른 후보검사도 기존 apply_checked와 합법 여부·결과 상태를 비교하는 검증을 켜서 테스트했다. 여왕 승격·앙파상·캐슬링·무작위 기보·동결된 DMG8 golden·DMG6/7 dispatch가 포함된다. 전체 23 게임에서 원본과 최적화 joint 파일의 SHA256 은 동일했다. 이는 모든 합법 포지션의 수학적 완전검증이나 Rust/ARM 검증을 대신하지 않는다.

장기 목표3.5 bit/반수는 별도 가설로 유지한다. 현재 개발 payload 대비 약0.49127 bit/반수의 추가 감소가 필요하며, 독립 자료에서의 현재 기준값부터 다시 측정해야 한다.

## B. 압축 모델의 근본적 약점

### B1. 관계 변화와 판 전체 상태가 섞여 있다

bit10～12 는 착수 후 자기 편 전체가 공격하는 상대 queen/rook/minor 존재 여부다. bit13～15 는 자기 queen/rook/minor 중 하나라도 수비받는지다. bit23 은 이동 기물의 fork가 아니라 **자기 편 전체가 공격하는 고가치 상대 기물이 둘 이상인지**다. bit9 역시 엄밀한 발견 체크 검사가 아니라 checking과 이동 종류 조건의 조합이다.

이 특징들이 쓸모없다는 뜻은 아니다. 다만 이번 23 게임에서 bit15 는 1,028 개 포지션에서 모든 후보에 1 이었고, 후보를 구별한 포지션은 522 개였다. 다른 곳에서 학습한 계수가 현재 모든 후보에 공통인 점수로 들어갈 수 있다. ‘수비된 기물이 이미 많다’보다 ‘이번 수가 어떤 위험·수비·공격 관계를 바꿨는가’가 더 직접적인 표현일 수 있다.

### B2. 실제 모델은 단순한 무제한 softmax가 아니다

현재 θ는[-48,48], 합산 점수는[-128,128]로 잘리고 고정 LUT와 CDF 양자화를 거친다. 52,653 개 합법 후보 중 4,470 개, 약 8.49%의 합산 점수가 제한 범위 밖이었다. 학습은 잘림의 미분을 적용하지 않고 모든 활성 특징에 selected-minus-expected를 적용한다. 따라서 개념식의 정확한 gradient descent라기보다 안정화를 위한 정수 근사 학습이다.

공통 특징 점수는 잘리지 않는 정확 softmax에서는 상쇄되지만, 현재 합산 잘림과 LUT·빈도 평활화에서는 그대로 상쇄된다고 가정하면 안 된다. 공통점수 제거는 새 모델 실험으로 분류한다. 기존 DMG8 에서 조용히 바꾸면 호환성이 깨진다.

### B3. 위험과 전술 가치의 해상도가 부족하다

공격받는지·수비받는지는 교환의 손익과 다르다. 작은 기물에게 공격받는 여왕과 왕에게 형식적으로 공격받지만 잡힐 수 없는 기물은 다르다. 저비용 SEE 구간·공격자 가치·노출선·왕 안전 변화가 단순한 새 좌표 가점보다 우선한다. 깊은 엔진의 전수 탐색은 초기 v0.9 에서 제외한다.

### B4. 학습과 전문가 사이의 중복

joint는 이미 v7 기회보정 분포를 기반으로 삼는다. 여기에 같은 baseline·A/E·joint를 평균하면 독립 정보원을 결합하는 것과 다르다. 개수만 늘려서는 이익이 보장되지 않는다. 현재 joint의 θ·사전은 archive 전체에 공유되고 mixture는 1/64 share를 쓰며, reset 모드는 α만 게임마다 초기화한다. 이를 새 기능처럼 다시 제안하지 않는다.

### B5. 자료 분포와 목표

23 게임 및 과거 4 게임·무작위·반복자료는 모두 개발자료다. 서로 다른 입력 순서·기력·시간제어·국면에서 같은 수치가 유지된다고 볼 수 없다. 3.99127 에서 payload3.9 까지 필요한 감소는 약 0.09127 bit/반수, 현재 표본 전체로 156.8 bit 정도다. 하지만 총길이는 4.16764 라서,895 바이트에서 총 3.9 이하를 만들려면 이 표본에서는 837 바이트 이하가 필요하다. 둘은 다른 목표다.

## C. 최상위 개선안 12 개

분류: S는 속도 중심, P는 확률·일반화 중심, X는 파이프라인 변경이다. 정확 최적화의 정보이론적 이익은 0 이라고 명시한다. 미래의 처리량이나 bits/ply를 수치로 예측하지 않는다.

| 안 | 축 | 압축 잠재력 | 속도 잠재력 | 난도 / 주요 위험 |
|---|---|---|---|---|
| 1. 정확한 국소 공격 차분 | S | 같은CDF면 0 | 높음, 미측정 핵심 | 높음 / 다중 공격 제거 |
| 2. 합법수 직접 생성·공유 검증 | S | 같은후보면 0 | 높음 | 중상 / 왕·EP·핀 |
| 3. 모드 특화와 계산 프레임 | S | 같은CDF면 0 | 일부 실측 | 낮음 / 숨은 학습 의존성 |
| 4. 수요별 공격 커널·데이터 배치 | S | 같은마스크면 0 | 일부 실측 | 중간 / CPU별 차이 |
| 5. 착수 원인 중심의 작은 관계 차분 | P | 높음 가설 | 특징 축소 시 유리 | 중간 / 정보 제거 |
| 6. 제한 SEE와 위험 순위 | P | 높음 가설 | 계산 추가 | 중간 / 인간 실수 과신 |
| 7. 신뢰도별 정수 학습 | P | 중간 가설 | 단순법과 비슷하거나 추가 | 중간 / 적응 지연 |
| 8. 자기 이전 수의 작은 문맥 | P | 중간 가설 | 작은 추가비용 | 낮중 / 희소 문맥 |
| 9. 잔차 공동 교정과 조건부 사전 escape | P | 중간 가설 | 전문가 축소 시 유리 | 중간 / 중복·과신 |
| 10. 싼 루트·선택된 묶음만 정밀 계산 | X | 미정 | 높은 상한 가능 | 중상 / 루트 질량 오류 |
| 11. archive 내부 teacher→작은 전송 student | X | 큰배치에서 유망 | decoder에 유리 | 중상 / 모델비용 |
| 12. encoder 탐색→COPY·짧은 프로그램 | X | 반복·강제 수순에 유망 | 매크로 적용 시 유리 | 중간 / 선택·예외 비용 |

### C1. 정확한 국소 공격 차분

**대상·알고리즘·양쪽 동작.** 후보마다 두 전체 공격 지도를 재생성하는 비용을 줄인다. 현재 판에서 기물별 공격집합 A_i, 방향 ray, 정규화된 기물ID를 준비한다. 후보가 바꾸는 칸 집합C와 이동·포획 기물, C와 같은 선에 있는 old/new slider를 dirty로 잡는다. dirty 기물의 공격만 새 점유 상태로 계산한다. 이동하지 않은 knight·king·pawn의 기하학적 공격은 점유에 의존하지 않으므로 그대로 쓸 수 있다. decoder도 후보마다 같은 계산을 한다.

**메모리·연산·C++ 구조.** `attack[32]`, `piece_id[64]`, `count[2][64]`, `union[2]`, `MoveDelta`, 되돌리기 journal. 또는 아래E의OR range-table로 count 쓰기를 피한다. 단순 dirty 검색은O(|C|Q), Q는 slider 수, 재계산은dirty의 ray 수에 비례한다. 최악에는 전체 재계산과 비슷해지고 추가 관리비가 든다.

**정보·속도.** 같은 공격 의미와CDF를 보존하면bit 개선은 0 이다. 계산량을 ‘전체 기물×후보’에서 ‘실제로 영향을 받는 기물×후보’로 줄이는 것이 목적이다. 일반 구조를 비슷한 규칙으로 대충 업데이트하는 것은 정확 최적화가 아니다.

**규격·악화·대조·분류.** 기존 OR에서 제거할 mask를 단순AND NOT 하면 다른 기물의 중복 공격까지 지우므로 잘못이다. EP의 세 칸·castle의 네 칸·promotion 기물종류를 포함한다. 열린 slider 판에서는 비용이 커질 수 있다. 원본 full-map과 모든후보의 공격mask·24flag·CDF·archive bytes를 비교한다. 증분 공격/역공격 인덱스는 기존 기술이고 이질적 질의와OR 제외를 결합하는 것은 설계 조합이다. 실제 delta 시제품 성능은 이번에 측정하지 않았다.

### C2. 합법수 직접 생성과 검증 공유

**대상·알고리즘·양쪽 동작.** candidates→apply_checked→다시pseudo생성·복사·왕 검사 경로를 바꾼다. 한 번 구한 checkers·pinned·pin-line·evasion mask로 보통 수를 직접 거른다. 비체크·비핀·비왕·비EP는 바로 통과한다. 단일체크는 checker포획/선차단, 이중체크는 왕 경로, 왕은 실제 post-occupancy 공격 검사다. 특수수는 느린 정확 경로를 먼저 유지한다. 양쪽은 같은 source/to/promotion 순서로 열거한다.

**메모리·연산·C++ 구조.** checkers/pinned64bit, pin-line[64], 고정 후보배열과 드문 초과용 vector. 한 번의 왕 ray 검사 + 실제 출력 후보 규모. 전체 왕 검사는 후보 전체가 아니라 왕·특수·미확인 경우에 집중된다. candidate마다 큰 Position 복사 대신 점유/기물집합 delta나Undo를 쓴다.

**정보·속도.** 합법집합·baseline 점수·순서가 같으면bit 이익 0. 중복 검증·이동생성·복사를 제거한다. 기존 baseline의 체크 가점까지 관계Frame의 결과를 재사용하는 것이 중요하다.

**규격·악화·대조·분류.** 현재 baseline은 불법 pseudo까지 포함한 raw합이 임계값을 넘으면 scaling할 수 있다. 단순히 legal부터 점수를 계산하면 드문 상태에서 구DMG8 과다를 수 있다. legacy는 원래 scaling을 재현하거나, 엄격한 upper bound로 scaling 불가능함을 증명한 빠른 경로와 원본 fallback을 둔다. DMG9 는 legal집합에서 처음부터 정의한다. 체크·특수수가 많은 자료는 이익이 줄 수 있다. legal_moves oracle·특수FEN·perft·기존CDF에 대해 비교한다. 알려진 체크/핀 기반 생성이고 Stockfish 원문에도 유사한 구조가 있다.[R1] 코드는 복사하지 않고 DMG의 공격 의미·순서를 유지해 구현한다.

### C3. 모드 특화와 공통 계산 프레임

**대상·알고리즘·양쪽 동작.** joint 단독에서 쓰지 않는 A/E·exact·uniform을 계산하지 않고 정확 사전 저장도 생략한다. A/E의 특징별 비율, phase, material, pattern key는 한 번 계산한다. 후보 key·flag·질량을 PlyFrame에 보관해 학습에서 재사용한다. decoder도 동일한 모드 함수를 실행한다.

**상태·연산·자료구조.** 모드별 state와 key[M]·feature_mass[F] 등의 수 KiB 배열이다. 후보별 특징 나눗셈을 특징별 한 번으로 줄이고, 불필요한 exact 상태의 archive 크기 비례 성장도 막는다. 템플릿 모드 또는 명시 함수로 의존성을 드러낸다. 향후 예측에 쓰이는 상태인지 확인한 뒤 삭제해야 한다.

**정보·속도.** 같은 모드를 archive 전체에서 쓰면 bit 이익은0이고 실행시간만 줄어든다. 이번 시제품에서 실제 개선을 측정했다. 모드를 도중에 바꾸면 생략한 상태가 필요해질 수 있으므로 별도 규격이 필요하다.

**규격·악화·대조·분류.** 코드 복제로 I-cache가 불리하거나 작은 자료에서 배열 초기화가 이익을 상쇄할 수 있다. 같은 모드의 CDF·bytes, 모든 모드의 회귀를 대조한다. 불변식 hoist·dead-state 제거·메모이제이션이라는 알려진 최적화의 적용이다.

### C4. 수요별 공격 커널과 데이터 배치

**대상·알고리즘·양쪽 동작.** 전체 공격 union과 한 칸의 attackers 질의를 다른 API로 둔다. v8의 상대 지도는 목적지 한 칸 질의로 교체하고, 자기 편의 여러 대상 관계에는 union/delta를 쓴다. 비슬라이더 테이블·집합 shift, portable ray, magic, PEXT를 같은 mask를 내는 교체 가능한 backend로 둔다.

**상태·연산·자료구조.** 시험한 portable 테이블은 약6 KiB다. 더 큰 테이블은 L1/L2 비용을 따로 측정한다. move/flag/key/weight의 SoA는 dot·기대값 순회를 돕지만, Move만 읽는 경로는 AoS가 나을 수 있다. 희소 bitmask 순회와 dense SIMD를 실제 후보 수에서 비교한다.

**정보·속도.** 같은 mask이면 bit 이익은0이다. 한 칸 수요에 전체 지도를 만들지 않는 절약은 명령어 교체보다 큰 구조 변화다. 값이0/1/2이상인지만 필요하면 정확 popcount 대신 `x!=0`, `x&(x-1)`을 쓸 수 있다.

**규격·악화·대조·분류.** BMI2 지원이 PEXT 우위를 보장하지 않는다. Intel·AMD 세대·ARM·WASM별 microbenchmark와 실코덱을 함께 측정한다.[R2] 희소·작은 후보에서는 SIMD가 손해일 수 있다. backend별 mask·CDF·bytes 일치가 첫 조건이다. 기존 저수준 기법이며 portable ray와 수요 분리는 이번에 실측했다.

### C5. 착수가 바꾼 관계를 작은 벡터로 표현

**대상·알고리즘·양쪽 동작.** 판 전체의 공격·수비 존재 여부 대신, 이전 관계와 후보 후 관계 사이의 변화를 사용한다. 살아남은 상대 기물에 대한 위협 변화, 자기 고가치 기물의 수비 변화, 목적지 위험·지원, 체크, 이전 자기 기물의 연속 이동을16개 이하의 이진/삼진 특징으로 만든다. 양쪽은 같은 SharedFrame과 MoveDelta에서 계산한다.

**상태·연산·자료구조.** 후보별 양성/음성 두 bitmask와16개 계수, 공유 대상 집합이다. 공격 차분 뒤에는 소수의 AND·OR·개수 부호 비교면 된다. 전체 그래프 동형성이나 고차원 텐서는 사용하지 않는다.

**정보·속도.** 새 위협을 만든 수와 기존 위협을 방치한 수를 구별하면서 다른 좌표에서도 학습을 공유한다. 특징 수와 중복 계산을 줄일 수 있지만 기존 정보를 버릴 위험도 있다. 같은q의 엔트로피가 저절로 줄어드는 것은 아니다.

**규격·악화·대조·분류.** 공통 특징을 제거해도 v8의 잘림·LUT에서는 CDF가 같지 않으므로 DMG9 프로필을 쓴다. 장기 계획이나 위협을 유지하는 수에서 악화할 수 있다. 원래24flag/일부 삭제/16개 절대 특징/동일 용량 차분/차분+SEE를 비교한다. 관계·차분은 알려진 개념이며 이 최소 표현의 효과는 조합 가설이다.

### C6. 제한 SEE와 위험 구간

**대상·알고리즘·양쪽 동작.** 공격·수비의 유무를 교환의 실제 순손익으로 보완한다. 목적지 공격자에서 최소가치 기물을 차례로 선택하고 점유·x-ray를 갱신하는 제한 static exchange 계산을 쓴다. 결과는 음/0/양 또는 작은 손실 구간으로 만든다. capture·위험 목적지에만 적용하고 나머지 값은0으로 정의한다. 양쪽은 같은 gate와 최대 교환 횟수를 쓴다.

**상태·연산·자료구조.** 역공격 질의, 작은 gain[32], 점유 delta다. 교환 사슬 L에 대해 O(L×공격자 갱신)이며 전체 게임 트리가 아니다. threshold SEE는 남은 손익 구간 전체가 한 분류에 들어갈 때만 정확하게 조기 종료한다.

**정보·속도.** 좋은 포획과 손해 보는 교환의 확률을 구별한다. 기존 공격 커널을 재사용하지만 계산이 추가되므로 절약 bits/CPU를 측정한다.

**규격·악화·대조·분류.** 기하학적 SEE는 핀·왕 포획을 근사할 수 있다. 이는 모델 근사이며 합법수를 제거하는 규칙으로 쓰지 않는다. 희생·실수·조용한 계획에서 악화한다. binary 위험/material 차이/SEE 구간/같은 CPU 예산의 얕은 탐색을 비교한다. SEE는 기존 기술이고 인간 선택의 온라인 교정과 결합하는 것이 연구다. 엔진 강도와 인간 예측은 다르다.[R3]

### C7. 변별력·신뢰도별 정수 학습

**대상·알고리즘·양쪽 동작.** 동일 학습률과 감쇠의 과신·진동을 줄인다. 특징 질량과 분산 `Eφ²−(Eφ)²`를 모으고 노출·분산의 정수 bucket으로 학습률과 축소를 선택한다. 관측 후 갱신하며 처음에는 기존 고정 학습률을 유지한 대조부터 만든다.

**상태·연산·자료구조.** θ[F], residual[F], 선택적 variance_acc와 노출 count다. F=16이면 추가 상태는 수백 바이트다. 희소 flag에서 기대질량을 한 번 누적한다. 선택 수에 없는 특징도 다른 후보의 기대질량이 있으면 갱신해야 한다.

**정보·속도.** 희귀 특징의 과신과 분포 변화의 진동을 줄이는 것이 목표다. 후보를 전혀 구별하지 않는 특징의 기대 차는0이지만, 구형 호환을 유지하려면 잔차·반올림·감쇠까지 보존해야 한다. AdaGrad/FTRL이라는 이름만으로 bit가 줄지는 않는다.[R4]

**규격·악화·대조·분류.** 새 학습률은 DMG9에만 쓴다. sqrt는 정수 isqrt/LUT로 고정한다. 작은 dense 모형에서는 FTRL의 희소성 이익보다 추가 상태·나눗셈이 클 수 있어 가벼운 분산 bucket을 먼저 검증한다. 누적 신뢰도는 빠른 분포 변화에 늦게 반응할 수 있다. fixed-step/노출/분산/FTRL을 같은 특징에서 비교한다. 학습법은 기존 기술이며 정수 코덱 결합은 가설이다.

### C8. 이전 자기 수의 작은 문맥 잔차

**대상·알고리즘·양쪽 동작.** t−2의 자기 기물ID·목표 영역·최근 포획을 기억해 연속 행동을 표현한다. 전역θ에 노출 신뢰도로 축소한 작은 문맥 잔차를 더하고 최종 계수 범위를 제한한다. 양쪽은 판과 복원 이력에서 문맥을 정한다.

**상태·연산·자료구조.** 백흑별 이전 기물·목표와4～8문맥×16계수 정도다. 수백 바이트～수 KiB와 후보별 작은 정수 연산이다. 기물ID는 승격에도 유지하고 포획 후 재사용하지 않는다.

**정보·속도.** 정확한 포지션 재방문 없이 자기 차례 간 정보를 쓴다. 기존 이동 방향 빈도와 달리 현재 선택의 조건부 확률 잔차를 교정한다. 작은 계산비로 동시 개선을 노릴 수 있다.

**규격·악화·대조·분류.** 게임 경계에서 이력을 reset한다. Elo·시간제어는 착수열에서 알 수 없으므로 무료 문맥으로 쓰지 않는다. 강제 응답·계획 변경·희소 문맥에서 악화한다. 판만/t−1/t−2/문맥 잔차를 같은 용량으로 비교한다. 짧은 문맥 모형은 기존 기술, 관계 잔차의 효과는 조합 가설이다.

### C9. 중복 전문가 축소와 조건부 사전 escape

**대상·알고리즘·양쪽 동작.** joint에 이미 들어 있는 v7 baseline을 다시 희석하기보다, 기회보정×관계 잔차를 하나의 모형으로 쓴다. exact 재방문의 증거가 충분하면 known move에 질량을 주고 escape 뒤에는 그 집합을 제외한 분포를 쓴다. 양쪽이 관측 수와 pre-update 확률로 같은 상태를 갱신한다.

**상태·연산·자료구조.** 상대 log-odds 계수 몇 개, 방문 신뢰도, 작은 outgoing count다. 쓰지 않는 expert·사전을 제거할 수 있다. 여러 증거를 곱하는 경우 공동 교정이 필요하며 초기R1은 복잡한 곱을 넣지 않는다.

**정보·속도.** 재방문과 관계가 보완적일 때 이익을 내고 중복 모델의 계산을 줄인다. 선택·escape 비용은 실제 분포에 들어간다. 실패 비용을 무료로 만들거나 기존 fixed-share를 새 이름으로 제안하는 것이 아니다.

**규격·악화·대조·분류.** 기본 전문가를 자기 손실로 학습하는 것과 혼합 손실을 미분하는 것은 다르다. 후자는 책임도까지 필요하다. 재방문이 없는 자료에서 exact 비용, 무작위에서 인간 잔차가 불리하다. 같은 전문가의 기존 평균/축소 평균/학습 잔차/escape를 비교한다. context mixing·escape는 기존 기술이며 중복 해소 설계가 조합이다.[R5]

### C10. 싼 루트와 선택된 묶음만 정밀 계산 ★

**대상·알고리즘·양쪽 동작.** 모든 후보의 비싼 특징을 계산하는 전제를 바꾼다. 합법수를 cheap score로 정렬해 작은 bucket으로 분할한다. 먼저 싼 root 분포로 bucket을 부호화한다. decoder가 bucket을 안 뒤 그 안의 후보만 관계·SEE로 정밀 계산하고 조건부 수를 푼다.

**상태·연산·자료구조.** group_offsets, cheap[M], expensive[B], 별도 root/leaf 학습 상태다. 비용은 O(M×싼 특징+B×정밀 특징). B와 정렬·빈 bucket 정책은 프로필에 고정한다. leaf의 조건부 기대값은 관측된 bucket 안에서 계산한다. 기존 expensive full 모델을 shadow 학습하면 계산 절약이 사라진다.

**정보·속도.** `P(m)=P(g)P(m|g)`인 새 모형이다. 비싼 정보를 leaf에만 쓰는 효율이 root 예측 손실을 넘어야 한다. 기존 full-softmax의 집계 질량을 계산하지 않고 같은 CDF를 공짜로 얻는 방법은 아니다.

**이전 실패·규격·대조·분류.** 이동 종류별 고정 계층의 반복이 아니라 계산 예산을 제한하는 cheap bucket과 조건부 학습이다. DMG9 전용이다. root가 좋은 수의 묶음을 놓치면 악화한다. flat/과거 type 계층/새 root-leaf에서 root 손실과 절약 시간을 분해한다. 조건부 계산에는 선례가 있으나 GPU 적응 softmax의 성과를 이 CPU 코덱의 예측에 쓰지 않는다.[R6]

### C11. 무거운 encoder teacher와 작은 전송 student ★

**대상·알고리즘·양쪽 동작.** encoder가 현재 archive를 여러 번 분석해 관계·SEE·선택적 탐색을 teacher로 쓴다. 실제 수와 teacher 분포를 cheap feature의 작은 정수 선형모형·결정표에 맞춰 구조와 계수를 전송한다. decoder는 teacher를 실행하지 않고 student만 쓴다. 외부 학습 파일은 없다.

**상태·연산·자료구조.** encoder의 다중 pass·훈련 행렬과 decoder의 수십 계수/leaf, 희소 index·정수 모형 serializer다. 최종 평가도 양자화된 학생과 실제 출력으로 한다. 학생이 선생의 분포를 완전히 재현한다고 가정하지 않는다.

**정보·속도.** 재사용 가능한 작은 설명으로 비싼 추론을 대체하면 비대칭이 생긴다. 목적은 `L(model)+L(data|model)`. 매 수의 최선 index를 보내는 것은 기보를 다시 보내는 비용과 비슷할 수 있다.

**규격·악화·대조·분류.** 32개 int8 계수만 단순 저장해도256bit여서 현재 payload 목표까지 남은156.8bit보다 크다. 작은 자료와 복잡한 학생에서 악화한다. 고정 초기/online/전송/전송+잔차를 배치 크기별로 비교하고 모든 모형비를 포함한다. MDL·증류는 기존 기술, archive 내부 정수 학생의 압축 효과는 가설이다.[R7]

### C12. encoder 탐색을 COPY·복원 프로그램으로 변환 ★

**대상·알고리즘·양쪽 동작.** encoder가 연속 수순·교환·강제 응답을 찾아 RAW 대신 COPY(reference,offset,length)나 정해진 script를 고른다. decoder는 이미 복원된 참조 창과 작은 인터프리터로 재생한다. 참조·길이·매개변수·예외는 모두 부호화한다.

**상태·연산·자료구조.** bounded window, suffix/hash index, script 목록과 RAW 모델이다. encoder는 buffer에서 토큰 최소비용 경로를 찾을 수 있다. 단순DP는 비용이 선택된 parse가 아닌 원시 착수 prefix에만 의존할 때만 유효하다. decoder는 출력 반수에 비례하며 공유 이력·모델 갱신을 유지한다.

**정보·속도.** 긴 수순의 전체 설명비가 일반 착수 합보다 작으면 bit와 후보 확률 계산을 함께 줄인다. 여러 수를 묶었다는 사실만으로 이익이 생기지는 않는다. RAW 세금·토큰 비용을 모두 포함한다.

**규격·악화·대조·분류.** castling·EP·promotion 상태가 달라도 전개가 실제로 합법이고 원문 수순과 같아야 한다. hash 뒤 실제 수순을 비교한다. 중복이 적고 패턴이 짧으면 악화한다. 반복/부분중복/중복제거 인간 자료를 분리하고 같은RAW와 비교한다. LZ·문법·프로그램 부호는 기존 원리이며 상태 검증형 조합의 효과는 가설이다.

## D. 가장 유망한 세 가지와 개발 우선순위

1. **C3+C4 의 정확한 모드 특화·수요별 공격 계산.** 즉시 적용할 최우선 패키지다. byte를 유지한 속도 개선을 이번에 관측했고, 단독joint의 불필요한 사전 성장도 제거한다. 압축 목표와 독립적으로 개발 가치가 있다.
2. **C1+C2 의 공유 공격 상태·합법수 커널.** 후보마다 전체 상태를 다시 분석하는 구조 자체를 제거한다. 위험은 크지만 다음 속도 회복의 가장 직접적인 축이다. 참조 구현과의 차분 검증을 전제로 한다.
3. **C5 의 작은 관계 차분에 C6/C7 을 독립적으로 검증한 모델.** 관계 수를 늘리는 대신 기존의 거친 위험 정보를 바꾸는 방향이다. 실제 인간 기보에서 새 정보를 얻을 가능성과 CPU비용의 균형을 우선했다.

비대칭 설계 중에는C11 을 우선한다. 다만 작은 23 게임에서 모형 전송비를 회수하기 어려우므로 큰 독립archive를 확보한 뒤 연구한다. C10 은 속도 잠재력이 크지만root 질량의 예측손실이 새 위험이라 초기 표준 프로필로 정하지 않는다.

## E. v0.9 통합 아키텍처와 바이너리 규격

### E1. 호환 최적화와 새 모델을 분리

- **DMG8-Fast:** 공격 의미·24flag·정수 계산·CDF·reset을 유지한다. 원본 파일과 byte 일치를 요구하며 출력은DMG8이다.
- **DMG9-R1:** 전용 legal generator, 공유 관계 Frame,16개 차분 특징과 한 번의 정수 정규화를 쓴다. 모델이 다르므로DMG9다. SEE·신뢰도·문맥·선택적 정밀화는 별도 프로필에서 검증한다.

기존 DMG6·7·8 decoder를 최신 학습 규칙으로 바꾸지 않는다. decode_any가 magic을 읽고 동결된 규칙에 dispatch한다. 같은 mask를 만드는 backend 개선과 모델 변경은 다른 commit으로 관리한다.

### E2. 정확한 공격 차분

현재 공격은 기하학적 공격이다. ray는 첫 blocker를 포함하고 끝나며 자기 기물이 있는 칸도 포함한다. pinned 기물의 공격과 합법 이동을 혼동하지 않는다.

각 기물의 이전 공격을 A_i, 바뀐 칸 집합을 C, 새 점유를 O'라 하자.

1. 보통 수는 C={from,to}, EP는 포획 폰 칸을 추가하고 castle은 왕·룩의 네 칸을 포함한다. promotion의 종류 변경도 MoveDelta에 쓴다.
2. 이동·포획·승격 기물ID를 dirty에 넣는다.
3. 이전 판과 새 판의 slider 중 C의 칸과 종류에 맞게 가로·세로·대각선으로 정렬된 기물을 추가한다. 가림을 무시한 안전한 superset부터 구현한다.
4. dirty 중 새 판에 살아 있는 기물의 공격만 O'에서 재계산한다.
5. 나머지의 이전 공격과 새 공격을 OR한다.

움직이지 않은 비slider의 기하학적 공격은 점유에 독립적이다. slider의 공격 변화에는 자기 ray 위의 점유 변화가 필요하므로 C와 정렬되지 않으면 그대로다. 이동·종류 변화도 dirty이므로 이 집합은 충분하다. king safety는 별도로 엄격하게 판정한다.

**중복 공격을 정확히 보존해야 한다.** 첫 구현은 count[2][64]와 undo journal로 이전 기여를 빼고 새 기여를 더한다. union에서 old mask를 단순 AND NOT하면 다른 기물의 중복 공격까지 지워서 틀린다.

쓰기 비용이 크면 기물ID 순서의 A_i에 range-OR sparse table을 만든다. dirty ID 사이의 변하지 않은 구간을 각각 O(1) 질의하고 합쳐 OR_except(dirty)를 O(|dirty|+1)에 얻는다. OR의 멱등성 때문에 겹치는 두 구간 질의도 가능하다. 여기에 dirty의 새 공격을 합친다. 같은 dirty 집합에서는 제외된 이전 union을 재사용할 수 있지만, 목적지가 다른 새 공격까지 재사용하면 안 된다.

상대 공격에서 목적지 한 칸만 필요하면 전체 상대 union 대신 attackers_to(to,O',enemy_sets')를 쓴다. 자기 union과 고가치 대상 집합은 delta로 구한다. SEE가 필요할 때만 추가 역공격을 계산한다.

EP·castle·promotion은 처음에 기존 full recomputation으로 보낸다. 빠른 경로와 결과가 같으므로 추가 bit가 필요 없다. dirty가 큰 경우에도 고정된 기준으로 full 경로를 선택할 수 있다. 실행시간을 재서 서로 다른 모델을 암묵적으로 선택해서는 안 된다.

### E3. 초기16개 특징 프로필

7～12는 감소/동일/증가의 -1/0/+1이고 나머지는0/1이다. 모든 계수는0에서 시작한다.

| 번호 | 정의 |
|---|---|
| 0 | 이동 전 출발칸이 상대에게 공격받음 |
| 1 | 이동 후 목적지가 상대에게 공격받음 |
| 2 | 이동 후 목적지가 자기 편에게 수비됨 |
| 3 | 포획, EP 포함 |
| 4 | 포획이며 직전 상대 착수의 도착칸을 잡음 |
| 5 | 포획 대상 가치가 이동 기물 이상; SEE 프로필에서는 고정 SEE 부호로 교체 |
| 6 | 이동 후 상대 왕을 공격함 |
| 7～9 | 살아남은 상대 queen/rook/minor의 공격 대상 개수 변화 부호 |
| 10～12 | 자기 queen/rook/minor 중 수비되는 기물 개수 변화 부호 |
| 13 | 승격 |
| 14 | 캐슬링 |
| 15 | 이번 기물ID가 직전 자기 착수의 기물ID와 같음 |

공격은 위의 기하학적 의미다. 잡힌 상대 기물은 before/after 대상에서 모두 제외해 포획을 ‘위협 상실’로 오해하지 않게 한다. 자기 기물의 승격은 종류별 집합 변화에 반영한다. ID는 초기 occupied 칸 오름차순으로 부여하고 재사용하지 않는다.

이 목록은 최적성 주장이 아니라 작은 ablation의 출발점이다. validation 이후 profile ID를 동결한다. 폰·전개·위치 정보는 baseline에 있으므로 처음에는 중복을 줄인다. 온라인 저차원 factor embedding도 가능하지만 작은16계수보다 반드시 작거나 안정적이지 않으며, cold start와 비선형 학습 비용 때문에 초기 우선순위에서 제외한다.

### E4. 확률과 갱신의 정수 규칙

**기초질량.** legal 후보의 structural/tactical w와 v7형 기회보정 ratio를 곱한다. 중간 baseline을 CDF로 양자화한 뒤 다시 joint를 양자화하는 대신 `b_raw=w×ratio`에서 관계 잔차를 곱해 한 번만 정규화한다. 중간 양자화의 평활화가 사라지므로 반드시 개선된다고 가정하지 않는다.

**점수.** F≤16, φ∈{-1,0,1}, θ∈[-32,32]tick,64tick/bit다. s=Σθφ는[-512,512]이며 끝에서 다시 자르지 않는다. d_i=s_i−min(s)는[0,1024]. n=d/64, r=d%64로 나누고 `raw_i=b_raw_i×LUT[r]×2^n`을 계산한다. LUT는 v8의 첫64개와 같은 고정 정수 상수 등으로 동결하며 libm을 쓰지 않는다.

w∈[1,512], ratio∈[341,3072], LUT≤2048로 정하면 raw<2^48, raw×16384<2^62다. 표준 시작에서 한쪽 기물 실체는16개 이하이고, 과도하게 from/to/promotion을 세어도 후보4096개라는 느슨한 상한을 쓸 수 있다. 합도<2^60이다. 보통256개는 스택에, 초과는4096개까지 확장 저장한다. 특정218개 구성 예를 증명된 전체 상한으로 간주하지 않는다.

**CDF.** T=16384. 각 후보에1을 주고 floor((T−M)raw_i/sum_raw)를 더한다. 나머지는 잔여값 내림차순, 동점은 후보순서로 나눠 합을 정확히 T로 맞춘다. 모든 곱의 상한은 위 범위에 든다. M=1이면 bit는 생략해도 관측·이력은 갱신한다.

**학습.** 기본R1은 Q12 잔차를 사용한다. 기대값은 갱신 전 실제 joint CDF의 양성 질량−음성 질량이다. signed round는 최근접·절반은0에서 먼 쪽, update 나눗셈은0쪽 절삭이다.

`res_f += 6 × (4096φ_f(y) − round(4096×mass_f/T))`

`u=trunc0(res_f/4096); res_f−=4096u; θ_f=clip(θ_f+u,−32,32)`

매64관측마다 θ를1tick씩0으로 감쇠한다. 신뢰도 학습은 별도 프로필이다. 이 역시 정수화·투영을 포함한 근사 학습이며 실수 gradient와 완전히 같다고 주장하지 않는다.

### E5. 자료구조와 메모리

- Position: board[64], bitboards, occupancy, side, castling, EP, 기물ID·왕칸.
- SharedFeatures: attack[32], 선택적 reverse_attackers[64], checkers/pins, 이전 대상 집합, phase, OR table.
- PlyFrame: 후보·key·양/음 feature mask·tactical mass·raw·CDF·feature mass·MoveDelta. 보통 수KiB～수십KiB이며4096후보의 별도 상한도 계산한다.
- OnlineModel:16계수·잔차와 선택적 분산/노출. 수백 바이트. 게임 경계에서 자기수 이력만 reset한다.
- PatternTable: 예컨대2^18개×16B=4MiB인2-way 고정용량 표. 완전 key,actual,expected,stamp를 저장한다. lookup은 읽기 전용이고 갱신 때 고정 hash·이전 갱신 stamp·동점 way 순서로 축출한다. stamp를 관측 ply 번호로 정의하면 후보 수에 따른 증가를 피할 수 있다.
- 공유 공격표: 시험한 portable ray는 약6KiB. magic/PEXT 표는 별도 계상한다.

학습 중 축출될 수 있는 slot pointer를 여러 삽입에 걸쳐 재사용하지 않는다. key를 캐시하고 갱신 시 다시 조회한다. saturation·감쇠 시점을 고정한다. 첫 비교는 v7의 saturation을 유지하고, bounded table 효과를 따로 측정한다. legacy의 무제한 표를 임의 축출하면 같은 파일을 복원할 수 없다.

### E6. 형식과 fallback

```text
"DMG9" | container_version:u8=1 | flags:u8
model_profile_id:u16LE | model_revision:u16LE
archive_game_count:canonical_ULEB128 | chunk_count:canonical_ULEB128
for each chunk:
    game_count | game_lengths[]
    model_description_length | optional_model_description
    payload_bit_length:u64-varint | payload_bytes
    CRC32-IEEE(chunk fields + payload)
optional_index_length | optional_index
```

R1은 flags=0, optional model/index 길이=0으로 시작한다. 알려지지 않은 flag/profile/revision은 거부한다. 이후 student·macro는 별도 profile로 정의한다. profile은 후보순서(from,to,promotion q/r/b/n), 특징, LUT, 학습, CDF, 사전, reset, 산술 부호기까지 식별한다. signed metadata는 ZigZag canonical varint로 정한다. payload는 논리 산술 bit를 byte 안에서 LSB-first로 저장한다. 예약bit·varint 최대길이·overlong 거부·padding도 규격화한다.

청크는 처음에 게임 경계에서만 나누고 모든 모델·사전을 cold reset한다. 게임 경계에서는 판·이력만 reset한다. 공유 state가 남았는데 payload만 나누면 독립 청크가 아니다. checkpoint를 전송하면 그 비용도 포함한다.

동일 결과의 정확 fast/slow 경로 전환에는 bit가 필요 없다. 서로 다른 모델 중 최종 파일이 작은 것을 선택하는 encoder-auto는 선택된 magic/profile를 반드시 기록한다. CPU시간이나 메모리 주소에 따른 암묵적 모델 전환은 금지한다. 손상 파일은 실패로 끝내며 다른 모델로 추측 복원하지 않는다.

### E7. 정확한 지연 계산의 범위

θ=0인 특징은 현재 점수에는 필요 없지만 다음 학습의 기대값에는 필요할 수 있다. 실제 수의 특징만 계산하거나 그런 특징을 영구 생략하면 기존 학습 state를 재현하지 못한다. 계수와 갱신 모두 비활성인 특징만 정확히 생략할 수 있다.

부분 raw 질량의 상·하한으로 정수CDF의 모든 관련 반올림과 선택 구간이 이미 확정됨을 증명하면 조기 종료할 여지는 있다. 그러나 정규화 분모가 모든 후보에 의존하므로 보통의 ‘상위 후보만 계산’은 그 증명이 아니다. 초기 구현에서는 동일 모델의 마법 같은 lazy 정규화보다 C10의 명시적인 새 분포를 별도 실험한다.

## F. 구현 가능한 의사코드

M은합법후보수, Q는slider수, d는dirty 기물수, F≤16 이다. 함수별로순수한판계산과학습state를구분한다.

### F1. generate_candidates(): 한 번의 체크·핀 분석

```text
generate_candidates(P):
    king = P.king_square[P.side]
    checkers, pinned, pin_line = analyze_king_rays_and_leapers(P, king)
    moves = SmallMoveVector(stack_capacity=256, hard_limit=4096)
    for from in increasing_own_squares(P):
        destinations = pseudo_attack_or_pawn_destinations(P, from)
        for to in increasing_squares(destinations):
            for promotion in canonical_promotion_order(P, from, to):
                m = Move(from,to,promotion)
                if king_move(m) or ep(m) or castle(m):
                    if not strict_legal_with_post_occupancy(P,m): continue
                else:
                    if double_check(checkers): continue
                    if single_check(checkers) and to not in capture_or_block_mask: continue
                    if from in pinned and to not in pin_line[from]: continue
                moves.append(m)
    return moves
```

EP는항상특수경로로검사한다. king 검사에서는 출발 왕을 제거하고 목적지 포획도 반영한 점유를 쓴다. castle은출발·통과·도착판의정확한공격의미를쓴다. 첫시제품은비체크일반경로만최적화하고체크중전체를strict fallback해도된다. 시간은왕분석+후보열거+특수수검사이며,학습state에의존하지않는다.

### F2. compute_shared_features(): 현재 판의 공통 계산

```text
compute_shared_features(P,H,moves):
    S.phase = material_phase_once(P,H.ply)
    S.occ = union(P.occupied)
    for live piece id:
        S.attack[id] = geometric_attacks(type, color, square, S.occ)
    S.union = union_per_color(S.attack)
    S.range_or = build_or_sparse_table_if_selected_backend(S.attack)
    S.before_targets = valuable_attack_and_support_targets(P,S.union)
    S.previous = H.previous_opponent_and_own_moves
    S.legality = previously_computed_check_pin_frame
    S.pattern_key_prefix = phase_and_color_common_fields
    return S
```

O(Npiece×공격계산+Npiece log Npiece)에서OR표는선택적이다. 이동후모델학습state를읽거나갱신하지않는다. 단순구현은매ply다시만들고, 실제착수후의증분유지를나중에추가한다. 후보간공유만으로도재사용이생긴다.

### F3. compute_move_delta_features(): 정확한 후보 차분

```text
compute_move_delta_features(P,H,m,S,profile):
    D = make_small_overlay(P,m)             # from/to/capture/ep/rook/promotion
    if special_case_not_optimized(D):
        after = strict_reference_position(P,m)
        U_own = full_geometric_union(after, P.side)
    else:
        dirty = moved_captured_and_promoted_ids(D)
        dirty |= old_and_new_sliders_aligned_with(D.changed_squares)
        U_own = S.range_or.union_excluding(dirty, P.side)
        for id in dirty that is alive_after and owned_by(P.side):
            U_own |= geometric_attacks_after(P,D,id)
    danger_to = attackers_to_after(P,D,m.to,opponent) != empty
    after_targets = target_sets_after(P,D,U_own)
    phi = fixed_16_relations(S.before_targets,after_targets,D,H,danger_to)
    if profile.SEE_enabled and deterministic_SEE_gate(D,danger_to):
        phi[5] = bounded_SEE_class(P,D)
    return phi, checking_from(U_own), D
```

시간은 O(|C|Q+d×ray계산+F)이며 SEE 비용은 별도다. 보통 경로는 P를 수정하지 않는 overlay를 쓴다. dirty는 충분히 큰 안전한 집합으로 만들고, 특수수는 같은 결과의 느린 경로로 보낸다.

### F4. predict_probabilities(): 한 번의 정규화

```text
predict_probabilities(P,H,moves,S,model,profile):
    for each m in canonical order:
        phi[m], check[m], delta[m] = compute_move_delta_features(...)
        w[m] = clip(structural_tactical_weight_using_shared_frame(...),1,512)
        key[m] = pattern_key_using_cached_phase(P,m,S.phase)
        ratio[m] = frozen_opportunity_rule(model.patterns.lookup(key[m]))
        s[m] = dot(model.effective_theta,phi[m])       # no end clipping
    s_min = min(s)
    for each m:
        d = s[m]-s_min
        raw[m] = (w[m]*ratio[m]*EXP_FRAC[d % 64]) << (d / 64)
    cdf = positive_largest_remainder_quantize(raw,total=16384)
    masses = accumulate_signed_feature_masses(phi,cdf)
    return PlyFrame(moves,phi,w,key,cdf,masses,delta)
```

특징이 준비됐다면 시간은 O(MF+M log M)이다. log 항은 최대 나머지 배분을 단순 정렬한 비용이며, 정수 bucket/selection과 비교할 수 있다. 빨라진다는 이유로 근사 나눗셈의 반올림을 바꾸지 않는다. 예측 중 학습 state는 읽기 전용이다.

### F5. update_online_model(): 관측 후에만 갱신

```text
update_online_model(model,frame,selected,H):
    for f in active_or_required_features:
        expectation_Q12 = signed_round(frame.mass[f]*4096 / 16384)
        observed_Q12 = frame.phi[selected][f]*4096
        model.residual[f] += 6*(observed_Q12-expectation_Q12)
        u = trunc_toward_zero(model.residual[f]/4096)
        model.residual[f] -= 4096*u
        model.theta[f] = clip(model.theta[f]+u,-32,32)
    model.seen += 1
    if model.seen mod 64 == 0: decay_theta_one_tick_toward_zero()
    update_pattern_opportunities_in_canonical_order(frame.w,frame.key,selected)
    update_optional_experts_from_preupdate_predictions_only()
```

시간은 O(F+전체 후보의 pattern 갱신)이다. 관측된 특징만 갱신하는 설계가 아니다. 다른 후보에 있는 특징의 음성 기대값도 처리한다. 모든 후보에 일정한 특징은 기대 차가 0 이지만 residual과 정기 decay를 잘못 생략하지 않는다.

### F6. encode_move() / decode_move(): 같은 Frame으로 동기화

```text
encode_move(P,H,model,y,coder):
    moves = generate_candidates(P)
    S = compute_shared_features(P,H,moves)
    frame = predict_probabilities(P,H,moves,S,model,profile)
    selected = locate_exact_UCI_move(y,frame.moves)       # absent -> input error
    coder.encode(frame.cdf[selected],frame.total)
    update_online_model(model,frame,selected,H)
    commit_move_and_piece_ids(P,frame.delta[selected])
    update_history(H,y)

decode_move(P,H,model,coder):
    moves = generate_candidates(P)
    S = compute_shared_features(P,H,moves)
    frame = predict_probabilities(P,H,moves,S,model,profile)
    selected = coder.decode_index(frame.cdf,frame.total)
    y = frame.moves[selected]
    update_online_model(model,frame,selected,H)
    commit_move_and_piece_ids(P,frame.delta[selected])
    update_history(H,y)
    return y
```

양쪽은 같은 함수를 호출하며 주된 차이는 index를 얻는 방식이다. 후보 생성·특징·확률 계산이 시간을 지배한다. coder 비용은 구간 갱신과 출력 bit 수에 의존한다. 게임 길이는 header로 알고 있으며 mate만으로 종료를 추측하지 않는다.

## G. 개발 로드맵과 독립 검증

### G1. 구현 순서와 수정 파일

| 단계 | 구현·파일 | 검증 | 기대하는 정보 방향 / 채택 기준 |
|---|---|---|---|
| 0 | tools/profile_v8.cpp, tools/bench_matrix.cpp 추가; 원본 decoder와 golden 동결 | 원본 23 게임·기존 4 게임·무작위·반복의payload/bytes 재현 | 새로운 압축 주장은 하지 않음. 계측 부하와 정상 처리량 분리 |
| 1 | src/entropy.cpp의모드특화·비율/키hoist·단일칸질의; 이후 src/legacy_dmg8.cpp로분리 | 기존모든모드·golden·원본과CDF/bytes일치 | bit불변이며시간·메모리이익이재현되면채택;이번시제품이출발점 |
| 2 | src/attacks.hpp/.cpp, src/movegen.hpp/.cpp, src/position_delta.hpp 추가 | 각후보의legacy full-map/oracle·특수FEN·perft·sanitizer | 실제속도가좋은backend만채택. 빠르더라도한칸불일치면기각 |
| 3 | src/shared_features.*와OR-exclusion 또는count journal | before/after모든공격·대상집합과undo복원검증 | 후보간공유가큰자료에서시간절감. 관리비가더크면한칸질의/전체fast-map유지 |
| 4 | src/model_v9.*;16 개차분·융합정규화·정수profile | original24/동일 16 절대/16 차분/중간양자화유무를독립대조 | 관계변화의일반화가독립validation에서이익을내야함 |
| 5 | src/see.*와신뢰도학습을별도profile로추가 | capture/위험대조·동일CPU예산·긴부호꼬리 | 줄인bits와추가CPU가Pareto개선일때만결합 |
| 6 | include/dmg_v9.hpp, src/codec_v9.cpp, src/format_v9.cpp | 전체파일왕복·독립청크·legacydispatch·잘못된profile거부 | model/reset/index비용모두포함. 버전별decode규칙고정 |
| 7 | rust/core, rust/model, rust/format 및WASM wrapper | 매반수상태·flag·raw·CDF hash와최종bytes의golden비교 | C++/Rust/native/WASM에서동일한파일을교차복원 |
| 8 | encoder/student_fit.* 또는encoder/token_parse.* | 모형/토큰bytes까지포함한배치규모별대조 | C11/12 가총길이와decoder시간의이익을함께낼때별도모드채택 |

이파일명은v0.9 의 제안 구조이며 해당 파일들이 정식 코덱으로 이미 구현됐다는 뜻은 아니다. 이번실제로만든것은별도실험용src복사본·patch·bench·diagnostic이다. 최적화와새모형의commit을분리하면CDF불일치의원인을추적하기쉽다.

### G2. 수천～수만 게임의 독립 평가

Lichess 공식 표준 rated PGN은 다양한 기력·시간제어의 인간 기보 수집원으로 사용할 수 있다.[R8] 이번에는 자료 페이지를 조사했으며 새 대량 기보를 다운로드하지 않았다. 실제 수집 시 표준 초기판·유효 UCI·bot/변형 제외 정책과 변환 코드 버전을 기록한다.

예시 규모는 개발10,000게임, validation5,000게임, 최종 독립30,000게임이다. 예산이 작으면 수천 게임의 넓은 표본으로 먼저 가설을 탈락시킨다. Elo·시간제어·연대·길이를 층화하되 실제 사용 분포의 가중치는 평가 전에 고정한다.

정확 게임 중복과 거의 같은 긴 수순은 그룹으로 분리한다. 공통 오프닝 위치와 중복 게임은 다르다. 같은 개국이 재등장하는 실사용 평가와 다른 개국군으로 분리한 일반화 평가를 별도로 보고한다. 엔진·고반복 자료는 인간 평균에 섞지 않는다.

무작위는 여러 seed로 진단한다. 이론적 기준은 매번 독립 균등 선택하는 확률과정과 정지 규칙으로 정의한다. 고정 seed PRNG 한 파일의 짧은 생성 프로그램을 그 확률과정의 엔트로피와 동일시하지 않는다.

단일게임/16/256/4096게임 등의 archive를 각각 cold start한다. 같은 archive의 이미 복원한 게임으로 배우는 것은 허용된 online 압축이다. 개발 자료의 학습 계수를 평가 초기값으로 넘기지 않는다. C11은 현재 평가 archive에서 모델을 맞출 수 있지만 그 모델 전체를 파일에 넣는다.

### G3. 유형별 실패 예상

| 조건 | 가능한 실패 | 우선 대응·평가 |
|---|---|---|
| 낮은 기력·짧은 시간 | 전술 위험 모형이 실수를 과도하게 벌함 | 온라인 교정·양의floor·작은 균등guard; 꼬리의 총bit |
| 높은 기력·긴 시간 | 얕은 위험 검사로 조용한 계획을 놓침 | 자기수 문맥·관계 변화; 탐색은 encoder-only부터 |
| 드문 오프닝 | 좌표 사전의 불확실성 | 강한prior·관계 일반화·cold 평가 |
| 포지셔널 중반 | 전술flag의 정보가 적음 | 작은 모형으로 축소·SEE gate |
| 엔드게임 | 왕·폰 중심, 다른 국면θ의 잘못된 전이 | 판에서 알 수 있는 소수 문맥과 축소 잔차 |
| 짧은 게임 | 초기 적응·헤더 비중 | batch·작은 모형; 모든 짧은 게임의 총3.9는 보장하지 않음 |
| 긴 게임 | 일부 게임이 반수 평균을 지배 | 게임별 분포·게임 평균·반수 가중평균 동시 보고 |
| 엔진 대국 | 인간 실수prior가 맞지 않음 | 별도 평가·선택 비용 포함 |
| 반복 archive | exact가 강하고 관계 계산이 불필요 | exact/COPY 전체 비용 대조 |
| 서로 다른 게임 | graph·COPY·전송 모델비가 큼 | online compact를 강한 대조로 유지 |

예를 들어 material_phase≤8을 엔드게임, 그 외0～19반수를 오프닝, 나머지를 중반으로 고정하면 분석 구간이 겹치지 않는다. 원게임의 상태를 유지한 국면별 점수와 새FEN부터의 독립 압축은 다른 실험이다. 후자는 FEN 전송비를 낸다. Elo·시간제어 metadata는 평가용이며 decoder가 공짜로 아는 입력이 아니다.

### G4. 필수 측정과 채택 규칙

1. 분석용 log loss, 실제 정수CDF의 `Σ−log2(freq/total)`, payload bit 수를 구별한다. 모델·토큰 매개변수·padding·index·checksum·헤더를 포함한 총bytes도 별도 기록한다.
2. v7 기회보정, v8 legal/joint/mixture/auto, DMG8-Fast, 새 프로필을 같은 자료·머신·빌드·thread 수로 비교한다. 동시 실험을 섞지 않고 실행 순서를 교차한다.
3. 정상 처리량과 계측 빌드를 분리한다. warmup·충분한 실행시간·고정 코어를 사용하고 I/O·파싱 포함 여부를 명시한다. VM의 스케줄링·클럭 변동도 기록한다.
4. generate/shared/special legality/delta/baseline-pattern/exp-quantize/mass-update/coder/commit/allocation을 ply나block 단위로 계측한다. 후보마다 시계를 읽어 시간을 왜곡하지 않는다.
5. perf가 있으면 cycles·instructions·branches·branch-misses·L1/LLC misses와 call graph를 수집한다. 없으면 gprof·coarse timer·allocation count를 쓴다. 인라인으로 묶인 항목을 별도 함수의 시간으로 오해하지 않는다.
6. peak RSS와 명시적 table/stack/heap 용량을 함께 보고한다. 작은23게임에서의 RSS가 대형DB의 메모리 상한을 증명하지 않는다.
7. 특징 ablation은 재학습한 실제 파일 길이와 시간을 잰다. 고정θ에서 한 특징을0으로 만드는 것만으로 대체하지 않는다. 상관된 특징은 묶음 ablation도 한다.
8. 비용은 절약bits/추가CPU와 bits/ply 대 decode throughput의 Pareto 경계로 본다. 공통 계산을 공유하는 최적화들의 이익을 단순 합산하지 않는다.
9. 게임별 압축률 분포, 중앙값·꼬리, 전체 반수 가중평균과 게임 동일가중평균, 국면·기력·시간제어별 결과를 낸다.
10. archive 학습은 게임 간 의존성을 만든다. 반수 독립 bootstrap을 피하고 독립 archive/block의 대응된 차를 쓴다. 순서 재표집 시 두 코덱을 같은 순서로 다시 실행한다.
11. validation에서 선정·동결한 뒤 최종 독립 자료를 평가한다. 결과를 보고 튜닝을 재개하면 그 자료는 이후 개발용이다.

최소 실용 개선량과 속도·메모리 예산도 사전 결정한다. 예컨대0.02～0.05bit/반수의 순이익을 탐지할 정밀도를 계획할 수 있지만 이는 예상 성능이 아니다. 충분히 정밀한 평가에서 이익이0 이하이거나 최소량을 배제할 만큼 작으면 해당 특징군·예산을 기각한다. 불확실성이 크면 가설 전체의 반증이 아니라 판정 유보다.

### G5. 이식과 정확성

C++ signed overflow를 Rust release wrap으로 바꾸지 않는다. hash만 의도적인 unsigned wrapping을 쓰고 확률·빈도·길이는 checked 연산 또는 증명된 범위에서 계산한다. signed division은0쪽 절삭, round는 별도 함수로 동결한다. 음수shift나 플랫폼별 intrinsic의 암묵적 의미에 의존하지 않는다.

LUT·후보 순서·key packing·saturation/감쇠·reset·나머지 tie-break·bit 순서를 golden으로 검증한다. unordered_map 순회·주소·platform hash는 모형 선택에 쓰지 않는다. BMI2/SIMD/ARM/WASM backend는 같은 mask를 내야 한다. unsafe 자체는 속도의 증거가 아니다.

특수수 golden에는 EP 제거칸·castle 룩·승격4종과 잡는 승격·핀·발견체크·이중체크·왕 출발칸 제거·corner rook 포획의 권리 소멸을 포함한다. 긴 반복·빈 archive/game·합법수 하나·표 용량 초과·count 감쇠·reset 경계도 넣는다. perft는 flag/CDF 차분 검사를 대신하지 않는다. 매반수 state·flags·raw·CDF hash와 최종 bytes를 C++/Rust/native/WASM에서 교차 비교한다. 모델이 근사여도 원시 착수열의 복원은 정확해야 한다.

## H. 네 질문에 대한 연구 결론

**1. 약 3.4 배 속도 저하를 상당 부분 회복할 현실적인 방법은?**

수요별 공격 질의, 불필요한 expert·사전 제거, 특징별 비율과 pattern key의 hoist, 체크·핀 공유가 즉시 효과를 보였다. 이번 정확 시제품은 같은 6,857bit·895bytes에서 약 0.099M 수/초였다. 원본 약 0.054 대비 약 1.8 배다. 아직 같은 환경 v7 의 약 0.21 에는 못 미친다. 다음 큰 축은 공유 per-piece 공격과 dirty-slider 차분, baseline 체크 정보 재사용, 직접 legal 생성이다. 추가 배수는 실측 전 보장하지 않는다.

**2. 3.991 이하를 다양한 인간 기보에서 재현할 가능성이 가장 높은 알고리즘은?**

작은 관계 차분 모형에 온라인 기회보정 prior를 두고, 저비용 SEE 위험 구간과 신뢰도별 학습을 각각 검증하는 방향이다. 상태 간 일반화와 실제 전술 위험을 추가하면서 24 개 거친 절대 flag의 중복을 줄인다. 첫 독립 실험은 특징 수 증가보다 같은 용량의 절대 표현 대 차분 표현이어야 한다. 현 자료로 재현 가능성이나 3.9 달성을 수치로 확정할 수 없다.

**3. 가중치 개선보다 더 큰 전환은 가능한가?**

가능하다. 선택된 bucket만 정밀 계산하는 분해 코덱, 비싼 encoder teacher를 싼 전송 student로 바꾸는 두 부분 부호, 원시수 대신 COPY·생성 프로그램을 보내는 구조는 파이프라인을 바꾼다. 이들은 각각 root 예측 손실·모형 전달비·토큰 및 예외 비용을 낸다. 그 비용을 포함해 이겨야 한다.

**4. 제한된 시간에 가장 먼저 만들 구체적인 v0.9 는?**

첫 commit은 DMG8-Fast의 byte 호환 최적화다. 다음 commit은 별도 DMG9-R1 로 공유 공격 Frame, 직접 legal 생성, 16 개 관계 차분, bounded 정수 점수, 한 번의 CDF 양자화, 작은 online 학습을 만든다. 깊은 탐색·거대 문맥·다수 전문가·외부 가중치는 초기 범위에서 제외한다. 새 자료에서 각 부품의 정보 이익과 실행시간 이익이 남을 때만 SEE·신뢰도·비대칭 모드를 결합한다.

## 출처

아래 원문·공식 구현은 일반 기법의 근거이며 제안한 DMG9의 bits/ply나 처리량을 증명하지 않는다. 이번 패치의 ray·핀 코드는 DMG용으로 작성했고 Stockfish 코드를 복사하지 않았다.

- R1. Stockfish 공식 move generation·position 원문. 체크·핀·점유에 따른 합법성 분리의 참고: https://github.com/official-stockfish/Stockfish/blob/master/src/movegen.cpp ; https://github.com/official-stockfish/Stockfish/blob/master/src/position.cpp
- R2. Intel Intrinsics Guide 및 uops.info 논문. 명령어 의미·microarchitecture 비용과 측정의 구분: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html ; https://arxiv.org/abs/1810.04610
- R3. McIlroy-Young et al., Aligning Superhuman AI with Human Behavior: Chess as a Model System: https://arxiv.org/abs/2006.01855
- R4. Duchi et al., Adaptive Subgradient Methods for Online Learning and Stochastic Optimization: https://www.jmlr.org/papers/v12/duchi11a.html ; McMahan et al., Ad Click Prediction: a View from the Trenches: https://research.google/pubs/ad-click-prediction-a-view-from-the-trenches/
- R5. Koolen & de Rooij, Universal Codes from Switching Strategies: https://arxiv.org/abs/1311.6536 ; Mahoney, Data Compression Explained: https://mattmahoney.net/dc/dce.html
- R6. Grave et al., Efficient softmax approximation for GPUs: https://proceedings.mlr.press/v70/grave17a.html
- R7. Hinton et al., Distilling the Knowledge in a Neural Network: https://research.google/pubs/distilling-the-knowledge-in-a-neural-network/. 여기서는 외부 신경망을 쓰지 않는 작은 정수 학생을 제안했다.
- R8. Lichess 공식 공개 데이터: https://database.lichess.org/
- 계측 참고: GCC Instrumentation Options: https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html

## 재현 부록: 실제 검증한 DMG8-Fast 패치

이 부록은 **새 DMG9 모델 구현이 아니라 기존 DMG8 출력이 동일한 최적화 시제품**이다. 첨부 v0.8 원본을 별도 폴더에 풀고 아래 diff를 src/entropy.cpp에 적용한다. 원본 파일을 보존하고 비교한다. 모든 개선은 최종 cached 변형에 누적돼 있다.

시험한23게임 joint의 원본과 최종 파일 SHA256:

`cd8985086de6597f1abee0b6ca1ab938d4b85eac93db0da15ac38c548a86275b`

- Payload 6,857bit, total895bytes, payload3.99127 bit/반수, total4.16764 bit/반수.
- 마지막 throughput: encode0.0989526, decode0.0990354 Mply/s. 원본은0.0535242/0.0548579. 일반화된 배수가 아니라 앞의 머신·자료·빌드·측정법에 한정된다.
- `RMC_VERIFY_ATTACKS`는 각 생성 공격mask를64개 square_attacked 질의와 비교한다.
- `RMC_VERIFY_LEGAL_FAST`는 빠른 후보검사를 apply_checked의 판·권리·bitboard와 비교한다.
- 기존 v8_tests 통과. ASan+UBSan은 별도 결과를 아래 검증 메모에 명시한다. ARM/Rust/WASM은 이번에 실행하지 않았다.

중요한 함수 계약: 패치의 v09_known_pseudo에서 in_check=true를 빠르게 통과하는 이유는 **그 앞의 기존 candidates(EntropyCheckSignalLight)가 체크 중 후보를 이미 엄격하게 필터했기 때문**이다. 그 필터를 제거하면서 이 helper를 그대로 사용하면 안 된다. DMG9의 직접 legal generator는 별도의 검증된 계약을 사용한다.

모드 중간 전환도 지원하지 않는다. joint 단독에서 저장하지 않은 exact state를 갑자기 사용하는 프로토콜은 새로 정의해야 한다. 현재는 모드가 archive 동안 고정돼 있으므로 제거가 안전하다.

### 적용 diff

```diff
--- a/src/entropy.cpp
+++ b/src/entropy.cpp
@@ -248,59 +248,54 @@
 // Attack squares of the opponent computed ONCE per board state, not O(candidates).
 // All occupied squares are included as defended squares, unlike pseudo_destinations.
 // Each ray stops at the first blocker (and includes that occupied square).
+struct FastAttackTables {
+    uint64_t ray[64][8]{},knight[64]{},king[64]{},pawn[2][64]{};
+    constexpr FastAttackTables() {
+        constexpr int d[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
+        constexpr int n[8][2]={{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
+        for(int sq=0;sq<64;++sq) {
+            int f=sq&7,r=sq>>3;
+            for(int k=0;k<8;++k) {
+                int nf=f+d[k][0],nr=r+d[k][1];
+                if(unsigned(nf)<8&&unsigned(nr)<8)king[sq]|=uint64_t(1)<<(nr*8+nf);
+                while(unsigned(nf)<8&&unsigned(nr)<8){ray[sq][k]|=uint64_t(1)<<(nr*8+nf);nf+=d[k][0];nr+=d[k][1];}
+                nf=f+n[k][0];nr=r+n[k][1];
+                if(unsigned(nf)<8&&unsigned(nr)<8)knight[sq]|=uint64_t(1)<<(nr*8+nf);
+            }
+            for(int c=0;c<2;++c)for(int df:{-1,1}){
+                int nf=f+df,nr=r+(c?-1:1);
+                if(unsigned(nf)<8&&unsigned(nr)<8)pawn[c][sq]|=uint64_t(1)<<(nr*8+nf);
+            }
+        }
+    }
+};
+inline constexpr FastAttackTables fast_attack_tables{};
 struct AttackCache {
     uint64_t attacked_mask=0;
     explicit AttackCache(const Position& p) {
-        static constexpr int knight[8][2]={{1,2},{2,1},{-1,2},{-2,1},
-                                           {1,-2},{2,-1},{-1,-2},{-2,-1}};
-        static constexpr int king[8][2]={{1,0},{-1,0},{0,1},{0,-1},
-                                         {1,1},{-1,1},{1,-1},{-1,-1}};
-        uint64_t pieces=p.occupied[p.side^1];
-        while(pieces) {
+        auto& t=fast_attack_tables;int side=p.side^1;
+        uint64_t pieces=p.occupied[side],occ=p.occupied[0]|p.occupied[1];
+        while(pieces){
             int from=std::countr_zero(pieces);pieces&=pieces-1;
             int kind=abs_piece(p.board[from]);
-            int f=from&7,r=from>>3;
-            if(kind==1) {
-                int nr=r+(p.side?1:-1); // opponent white if p.side black
-                if(nr>=0&&nr<8) {
-                    if(f>0)attacked_mask|=bit(nr*8+f-1);
-                    if(f<7)attacked_mask|=bit(nr*8+f+1);
+            if(kind==1){attacked_mask|=t.pawn[side][from];continue;}
+            if(kind==2){attacked_mask|=t.knight[from];continue;}
+            if(kind==6){attacked_mask|=t.king[from];continue;}
+            int lo=kind==3?4:0,hi=kind==4?4:8;
+            for(int dir=lo;dir<hi;++dir){
+                auto ray=t.ray[from][dir];auto blocks=ray&occ;
+                if(blocks){
+                    int near=(dir==0||dir==2||dir==4||dir==6)?std::countr_zero(blocks):63-std::countl_zero(blocks);
+                    ray &= ~t.ray[near][dir];
                 }
-                continue;
+                attacked_mask|=ray;
             }
-            if(kind==2) {
-                for(auto& o:knight) {
-                    int nf=f+o[0],nr=r+o[1];
-                    if(unsigned(nf)<8u&&unsigned(nr)<8u)attacked_mask|=bit(nr*8+nf);
-                }
-                continue;
-            }
-            if(kind==6) {
-                for(auto& o:king) {
-                    int nf=f+o[0],nr=r+o[1];
-                    if(unsigned(nf)<8u&&unsigned(nr)<8u)attacked_mask|=bit(nr*8+nf);
-                }
-                continue;
-            }
-            int lo=kind==3?4:0,hi=kind==4?4:8;
-            for(int i=lo;i<hi;++i) {
-                int nf=f+king[i][0],nr=r+king[i][1];
-                while(unsigned(nf)<8u&&unsigned(nr)<8u) {
-                    int sq=nr*8+nf;
-                    attacked_mask|=bit(sq);
-                    if(p.board[sq])break;
-                    nf+=king[i][0];nr+=king[i][1];
-                }
-            }
         }
 #ifdef RMC_VERIFY_ATTACKS
-        // Exhaustive 64-square oracle comparison on every generated position.
-        for(int sq=0;sq<64;++sq)
-            if(((attacked_mask&bit(sq))!=0)!=p.square_attacked(sq,p.side^1))
-                fail("computed attack map disagrees with chess oracle");
+        for(int sq=0;sq<64;++sq)if(((attacked_mask&bit(sq))!=0)!=p.square_attacked(sq,p.side^1))fail("fast attack mismatch");
 #endif
     }
-    bool attacked(int square) const {return (attacked_mask&bit(square))!=0;}
+    bool attacked(int sq)const{return(attacked_mask&bit(sq))!=0;}
 };
 int tactical_bonus(const Position& p, Move m, const Move* previous, AttackCache& cache) {
     int kind=abs_piece(p.board[m.from]);
@@ -1183,10 +1178,9 @@
     // Aggregate both attack maps once rather than repeatedly scanning every piece
     // for each relation. These maps include defended occupied squares.
     AttackCache our_after(after);                  // attacks by the moving side
-    Position flip=after;flip.side=p.side;
-    AttackCache their_after(flip);                 // attacks by the opponent
+    // Only one enemy square is needed; use the exact target query.
     const uint64_t our_attacks=our_after.attacked_mask;
-    const bool danger_to=their_after.attacked(m.to);
+    const bool danger_to=after.square_attacked(m.to,after.side);
     const bool supported=our_after.attacked(m.to);
     uint32_t flags=0;
     auto add=[&](int n,bool ok){if(ok)flags|=(1u<<n);};
@@ -1206,7 +1200,7 @@
     uint64_t enemy_targets=(after.piece_bits[after.side][1]|
         after.piece_bits[after.side][2]|after.piece_bits[after.side][3]|
         after.piece_bits[after.side][4]) & our_attacks;
-    const int attacked_valuable=pop(enemy_targets);
+    const int attacked_valuable=enemy_targets?((enemy_targets&(enemy_targets-1))?2:1):0;
     bool queen=(enemy_targets & after.piece_bits[after.side][4])!=0;
     bool rook=(enemy_targets & after.piece_bits[after.side][3])!=0;
     bool minor=(enemy_targets & (after.piece_bits[after.side][1]|
@@ -1275,24 +1269,83 @@
 struct V8Step {
     CandidateList moves;
     std::array<uint32_t,256> flags{};
+    std::array<uint32_t,256> square_keys{};
     std::array<V6Weights,V8_K> experts{};
     V6Weights coded;
 };
+uint64_t v09_pinned(const Position& p) {
+    int king=p.king_square(p.side);if(king<0)return ~uint64_t(0);
+    constexpr int d[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
+    uint64_t pinned=0;
+    for(int dir=0;dir<8;++dir){
+        int f=(king&7)+d[dir][0],r=(king>>3)+d[dir][1],block=-1;
+        while(unsigned(f)<8&&unsigned(r)<8){
+            int sq=r*8+f,piece=p.board[sq];
+            if(piece){
+                bool ours=(piece>0)==(p.side==0);
+                if(block<0){if(!ours)break;block=sq;}
+                else {int k=abs_piece(piece);if(!ours&&(k==5||k==(dir<4?4:3)))pinned|=bit(block);break;}
+            }
+            f+=d[dir][0];r+=d[dir][1];
+        }
+    }
+    return pinned;
+}
+bool v09_known_pseudo(const Position& p, Move m, Position& next,uint64_t pinned,bool in_check) {
+    int kind=abs_piece(p.board[m.from]);
+    bool ep=kind==1&&(m.from%8!=m.to%8)&&!p.board[m.to];
+    bool castle=kind==6&&std::abs(int(m.to)-int(m.from))==2;
+    bool ok;
+    if(castle){ok=next.apply_checked(m);}
+    else if(abs_piece(p.board[m.to])==6){ok=false;}
+    else {
+        next.apply_unchecked(m);
+        // The candidate builder already fully filtered checked positions.
+        if(in_check) ok=true;
+        else if(kind!=6&&!ep&&!(pinned&bit(m.from)))ok=true;
+        else {int king=next.king_square(p.side);ok=king>=0&&!next.square_attacked(king,p.side^1);}
+    }
+#ifdef RMC_VERIFY_LEGAL_FAST
+    Position oracle=p;bool expected=oracle.apply_checked(m);
+    if(ok!=expected || (ok&&(V6Learn::key(oracle)!=V6Learn::key(next)||oracle.occupied!=next.occupied||oracle.piece_bits!=next.piece_bits)))fail("fast legality mismatch");
+#endif
+    return ok;
+}
+V6Weights v09_cached_opportunity(const CandidateList& c,const Position& p,unsigned ply,const V7Patterns& db,std::array<uint32_t,256>& keys) {
+    const unsigned phase=V7Patterns::phase(p,ply);std::array<uint64_t,256> raw{};
+    constexpr uint64_t prior=4u*4096u;
+    for(unsigned i=0;i<c.size;++i){
+        Move m=c.a[i].m;unsigned kind=abs_piece(p.board[m.from]);
+        unsigned from=p.side?(m.from^56u):m.from,to=p.side?(m.to^56u):m.to;
+        unsigned cap=V7Patterns::capture(p,m,int(kind));
+        uint32_t key=(((((phase*8u+kind)*64u+from)*64u+to)*2u+cap)*8u+m.promotion);keys[i]=key;
+        auto it=db.square_op.find(key);uint64_t hits=it==db.square_op.end()?0:it->second.actual, exp=it==db.square_op.end()?0:it->second.expected;
+        uint64_t ratio=std::clamp((hits+prior)*1024u/(exp+prior),uint64_t(341),uint64_t(3072));
+        raw[i]=std::max<uint64_t>(1,uint64_t(c.a[i].w)*ratio);
+    }
+    return v6_quantize(raw,c.size);
+}
+void v09_cached_observe(V7Patterns& db,const CandidateList& c,const std::array<uint32_t,256>& keys,unsigned ix){
+    for(unsigned i=0;i<c.size;++i){unsigned e=unsigned((uint64_t(c.a[i].w)*4096u+c.total/2)/c.total);V7Patterns::saturating_add(db.square_op[keys[i]].expected,e);}
+    V7Patterns::saturating_add(db.square_op[keys[ix]].actual,4096u);
+}
 V8Step v8_step(const Position& p,const Move* prev,unsigned ply,
                V8Model mode,const V8Context& ctx){
     V8Step s;
     Model dummy;
+    const bool in_check=p.square_attacked(p.king_square(p.side),p.side^1);
+    const uint64_t pinned=in_check?0:v09_pinned(p);
     CandidateList pseudo=candidates(p,Mode::EntropyCheckSignalLight,prev,ply,dummy);
     // Reference mode only needs legal filtering, not expensive relation maps.
     if(mode==V8Model::LegalOpportunity){
         for(unsigned i=0;i<pseudo.size;++i){
             Position next=p;
-            if(!next.apply_checked(pseudo.a[i].m))continue;
+            if(!v09_known_pseudo(p,pseudo.a[i].m,next,pinned,in_check))continue;
             s.moves.a[s.moves.size++]=pseudo.a[i];
             s.moves.total+=pseudo.a[i].w;
         }
         if(!s.moves.size)fail("DMG8 terminal position");
-        s.experts[0]=v7_opportunity(s.moves,p,ply,ctx.patterns,false);
+        s.experts[0]=v09_cached_opportunity(s.moves,p,ply,ctx.patterns,s.square_keys);
         s.coded=s.experts[0];
         return s;
     }
@@ -1300,7 +1353,7 @@
     uint32_t running=0;
     for(unsigned i=0;i<pseudo.size;++i){
         Position next=p;
-        if(!next.apply_checked(pseudo.a[i].m))continue;
+        if(!v09_known_pseudo(p,pseudo.a[i].m,next,pinned,in_check))continue;
         auto item=pseudo.a[i];
         s.moves.a[s.moves.size]=item;
         s.flags[s.moves.size]=v8_relations(p,next,item.m,prev,pre_enemy,ply);
@@ -1310,27 +1363,34 @@
     if(!s.moves.size)fail("DMG8 move requested from a terminal position");
     s.moves.total=running;
     // A/B reference: same v0.7 opportunity model on a fully legal alphabet.
-    s.experts[0]=v7_opportunity(s.moves,p,ply,ctx.patterns,false);
+    s.experts[0]=v09_cached_opportunity(s.moves,p,ply,ctx.patterns,s.square_keys);
     std::array<uint64_t,256> ae{},joint{};
     constexpr uint64_t prior=4096u*12u;
+    const bool need_ae = mode != V8Model::RelationalJoint;
+    const bool need_mix = mode==V8Model::ExpertMixture || mode==V8Model::ExpertMixtureReset;
+    std::array<int32_t,V8_F> ae_factor{};
+    if(need_ae) for(unsigned f=0;f<V8_F;++f) {
+        auto ratio=std::clamp<uint64_t>(((ctx.relation.actual[f]+prior)*1024u)/(ctx.relation.expected[f]+prior),384u,2048u);
+        ae_factor[f]=int32_t(ratio)-1024;
+    }
     for(unsigned i=0;i<s.moves.size;++i){
         int32_t log_score=0,ae_add=0;
         uint32_t f=s.flags[i];
         while(f){unsigned ix=std::countr_zero(f);f&=f-1;
             log_score+=ctx.relation.theta[ix];
-            uint64_t a=ctx.relation.actual[ix], e=ctx.relation.expected[ix];
-            auto ratio=std::clamp<uint64_t>(((a+prior)*1024u)/(e+prior),384u,2048u);
-            ae_add+=int32_t(ratio)-1024;
+            if(need_ae) ae_add+=ae_factor[ix];
         }
         log_score=std::clamp(log_score,-128,128);
         ae_add=std::clamp(ae_add/3,-768,2048);
         joint[i]=uint64_t(s.experts[0].weights[i])*v8_exp2_lut[log_score+128];
         ae[i]=uint64_t(s.experts[0].weights[i])*unsigned(1024+ae_add);
     }
-    s.experts[1]=v6_quantize(ae,s.moves.size);
+    if(need_ae) s.experts[1]=v6_quantize(ae,s.moves.size);
     s.experts[2]=v6_quantize(joint,s.moves.size);
-    s.experts[3]=v6_uniform(s.moves.size);
-    s.experts[4]=v6_dict(s.moves,ctx.exact,V6Learn::key(p));
+    if(need_mix) {
+        s.experts[3]=v6_uniform(s.moves.size);
+        s.experts[4]=v6_dict(s.moves,ctx.exact,V6Learn::key(p));
+    }
     if(mode==V8Model::LegalOpportunity)s.coded=s.experts[0];
     else if(mode==V8Model::RelationalAE)s.coded=s.experts[1];
     else if(mode==V8Model::RelationalJoint)s.coded=s.experts[2];
@@ -1345,7 +1405,7 @@
 void v8_observe(V8Context& ctx,const Position& p,const V8Step& s,
                 unsigned ix,unsigned ply,V8Model mode){
     if(mode==V8Model::LegalOpportunity){
-        ctx.patterns.observe_opportunity(p,s.moves,ix,ply,false,true);
+        v09_cached_observe(ctx.patterns,s.moves,s.square_keys,ix);
         return;
     }
     ctx.relation.observe(s.flags,s.experts[2],s.experts[0],s.moves.size,ix);
@@ -1363,8 +1423,8 @@
             ctx.alpha[k]=(ctx.alpha[k]*63u+13107u)/64u;
         }
     }
-    ctx.patterns.observe_opportunity(p,s.moves,ix,ply,false,true);
-    ctx.exact.observe(V6Learn::key(p),s.moves.a[ix].m);
+    v09_cached_observe(ctx.patterns,s.moves,s.square_keys,ix);
+    if(mode==V8Model::ExpertMixture || mode==V8Model::ExpertMixtureReset) ctx.exact.observe(V6Learn::key(p),s.moves.a[ix].m);
 }
 } // anonymous
 const char* v8_name(V8Model m) {

```

### 동일한 처리량 측정 driver

입력은 이미 읽어 두고, cold 모델의 archive encode/decode를 trial당 반복한다. 초기 파일 쓰기는 타이밍 밖이며 결과 동일성 검사는 타이밍에 포함된다. CPU cache까지 cold라는 뜻은 아니다.

```cpp
#include "dmg_v8.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
using Games=std::vector<std::vector<rmc::Move>>;
int main(int argc,char**argv){
 if(argc<5)return 2;
 int mode=std::stoi(argv[1]),reps=std::stoi(argv[2]);
 Games games;uint64_t n=0;
 for(int i=4;i<argc;++i){std::ifstream f(argv[i]);if(!f)return 3;std::string s{std::istreambuf_iterator<char>(f),{}};games.push_back(rmc::parse_moves(s));n+=games.back().size();}
 auto enc=[&]{return mode==7?rmc::v7_encode(games,rmc::V7Model::OpportunitySquare):rmc::v8_encode(games,rmc::V8Model(mode));};
 auto original=enc();if(rmc::v8_decode(original.bytes)!=games)return 4;
 {std::ofstream f(argv[3],std::ios::binary);f.write((const char*)original.bytes.data(),original.bytes.size());}
 uint64_t sink=0;std::vector<double> es,ds;
 for(int trial=0;trial<7;++trial){
  auto a=std::chrono::steady_clock::now();
  for(int k=0;k<reps;++k){auto x=enc();if(x.bytes!=original.bytes)return 5;sink+=x.bytes.size();}
  auto b=std::chrono::steady_clock::now();
  for(int k=0;k<reps;++k){auto x=rmc::v8_decode(original.bytes);if(x!=games)return 6;sink+=x.size();}
  auto c=std::chrono::steady_clock::now();
  es.push_back(std::chrono::duration<double>(b-a).count());ds.push_back(std::chrono::duration<double>(c-b).count());
 }
 std::sort(es.begin(),es.end());std::sort(ds.begin(),ds.end());
 std::cout<<"mode="<<mode<<" plies="<<n<<" payload="<<original.payload_bits<<" bytes="<<original.bytes.size()<<" enc_Mply_s="<<n*reps/es[3]/1e6<<" dec_Mply_s="<<n*reps/ds[3]/1e6<<" repetitions="<<reps<<" trials=7 sink="<<sink<<"\n";
}

```

원본과 패치판을 같은 옵션으로 각각 컴파일한다. 예시 명령의 bench.cpp에는 위 driver를 저장한다.

```bash
g++ -std=c++20 -O3 -DNDEBUG -I DMG-Entropy-Cpp-v0.8/include -I DMG-Entropy-Cpp-v0.8/src bench.cpp DMG-Entropy-Cpp-v0.8/src/rmc.cpp DMG-Entropy-Cpp-v0.8/src/entropy.cpp -o bench
./bench 2 16 output.dmg8 DMG-Entropy-Cpp-v0.8/examples/heldout/*.uci
```

mode2는v8 joint, mode7은v7 opportunity-square다. 입력 순서는 shell에서 파일명 정렬을 확인한다. 원본과 패치판 output의 byte 비교·원본 decoder의 복원·특수수 검증을 별도로 수행한다. 이 짧은 driver는 peak RSS나 하드웨어 카운터까지 측정하는 최종 벤치마크는 아니다.

### 검증 메모

최종 cached 시제품의 v8_tests는 일반 빌드와 공격/합법성 oracle 검증 빌드에서 통과했다. ASan+UBSan 빌드도 `ASAN_OPTIONS=detect_leaks=0`으로 실행해 통과했다. 이 환경에서는 LeakSanitizer가 /proc/task 접근 제한으로 종료했으므로 leak 검사 통과를 주장하지 않는다. 이 sanitizer 실행은 위 throughput 표에 포함하지 않았다.

이번 보고서에서 미수행한 항목은 새 독립 인간 기보 수집·평가, dirty-slider/OR-exclusion의 실제 성능 검증, DMG9-R1 압축 결과, Rust/ARM/WASM 교차실행, 하드웨어 perf 카운터와 대형archive peak RSS 측정이다. 이를 기존 소스 재현과 혼동하지 않는다.
