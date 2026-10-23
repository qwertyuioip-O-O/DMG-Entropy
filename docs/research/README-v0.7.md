# DMG-Entropy v0.7 (C++20)

**체스 UCI 기보의 학습 모델 없는 무손실 압축 실험 프로젝트.** 기존 v0.6의 DMG6 파일과 알고리즘을 보존하고, 새로운 **DMG7** 스트림과 크로스 포지션 패턴 사전/모델 경쟁을 추가했습니다.

## 이번 버전의 결론

- 동일한 포지션에서만 통계를 공유하던 사전에 더해, 서로 다른 포지션에서 같은 *이동 패턴*을 공유하는 사전을 구현했습니다.
- 단순 패턴 출현 빈도를 부호화하는 방식은 실패했습니다. 따라서 각 패턴의 **실제 선택 빈도와 기존 코덱이 기대했던 선택 확률의 누적값**을 비교하는 `opportunity-square` 방식을 도입했습니다.
- 기존 23게임/1,718반수 표본에서 `opportunity-square`가 **4.1240 bit/반수, 923바이트**로 기존 v0.6의 정확한 포지션 사전 **4.2346 bit/반수, 947바이트**보다 작았습니다.
- `opportunity-competition`과 `selective-competition`은 정확한 포지션 사전 + 새 사전을 포함한 **온라인 모델 혼합**입니다. 각각 4.1380 / 4.1385 bit/반수여서 이번 표본에서는 단독 사전보다 약간 나빴습니다.
- `auto`는 여러 모델을 **각각 실제 압축**한 뒤 가장 작은 전체 파일을 고릅니다. 인코딩은 훨씬 느리지만 디코딩은 선택된 모델 한 개만 수행합니다. 결과 파일은 DMG6 또는 DMG7일 수 있습니다.

## 컴파일과 테스트

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## CLI

```sh
# 여러 대국을 묶어 압축 (표준 초기 배치, 공백 구분 UCI 수순)
./build/dmg_v7 pack auto games.dmg7 examples/heldout/*.uci

# DMG6/DMG7 자동 인식, 각 게임을 game_N.uci로 추출
./build/dmg_v7 unpack games.dmg7 unpacked/

# 동일 데이터셋과 CPU에서 전체 모드 비교
./build/dmg_v7_bench examples/heldout/*.uci
./build/dmg_v7_bench --random
./build/dmg_v7_bench --repeat 4 examples/heldout/*.uci
```

### DMG7 인코딩 모드

`baseline`, `exact`, `geometry`, `square`, `combined`, `competition`, `switching`, `opportunity-geometry`, `opportunity-square`, `opportunity-competition`, `selective-competition`, `auto`.

`competition` = 기본/균등/계층형/정확한 포지션/기하 패턴/칸 패턴 6종 혼합.  
`opportunity-competition` = 위 모델 6종 + 기회 보정 2종, 총 8종 온라인 경쟁.  
`selective-competition` = 기본/균등/정확한 포지션/기회 보정 칸 패턴 4종만 경량 경쟁.  
`auto` = 앞서 열거한 모든 v0.7 모드와 v0.6 모드를 전체 파일 크기로 비교하고 최종 승자 **한 개만** 저장.

## 패턴 기억의 정확한 의미

`geometry`는 기물 종류, 자신의 진영 기준 전진 방향, 이동 벡터(좌우 대칭 정규화), 포획/승격/대략적 게임 단계를 사전 키로 만듭니다. `square`는 기물 종류, 진영 기준 정규화한 출발·도착 칸, 포획/승격/단계를 키로 만듭니다. 두 사전 모두 체스판 전체가 같지 않아도 공통 통계를 사용할 수 있습니다.

기회 보정은 `expected(pattern)`에 매번 가능한 후보의 기존 확률을 누적하고, `actual(pattern)`에 실제 선택을 누적합니다. 다음 착수의 점수는 `(actual + prior)/(expected + prior)`로 보정하되 1/3~3배로 제한합니다. 인코더와 디코더는 동일한 정수 계산과 과거에 복원된 수순만 사용합니다. 이는 **사전 학습이나 모델 파일이 없다는 뜻**이지 통계/확률을 사용하지 않는다는 뜻은 아닙니다.

## 저장 포맷

DMG7 헤더: magic `DMG7`, version 1, model id, 게임 수 varint, 각 게임의 반수 varint, payload bits varint, 이어서 공통 산술 부호화 스트림, CRC32. `auto`는 파일 크기 최적을 위해 DMG6을 선택할 수도 있습니다. `v7_decode()`는 두 magic을 모두 처리합니다. 모든 게임은 표준 초기 배치로 시작합니다.

**지원 범위:** 합법적 UCI 이동 수순(승격/캐슬링/앙파상 포함), 게임 경계와 순서, 정확한 무손실 복원. **미지원:** PGN 주석/태그/변형, Chess960, 임의 FEN 시작, 게임 단위 랜덤 액세스, 스트리밍 거대 아카이브 메모리 제한.

## 평가 자료

`results-v0.7/`의 CSV 4종과 `RESULTS-v0.7.md` 참고. 무작위 48게임은 균등하게 합법 이동을 선택했고, 반복 4회는 동일 기보 데이터의 인위적인 복제로 압축 사전 이득을 과장합니다. 23게임 표본과 4게임 표본은 이전 버전 개발 과정에서도 사용되었으므로, 새로운 대규모 독립 검증이라고 주장해서는 안 됩니다.
