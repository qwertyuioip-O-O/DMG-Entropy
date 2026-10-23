# DMG-Entropy 국면별 압축 진단 및 Phase-R1 실험 보고서

작성일: 2026-10-20. **실제 실행한 C++20 구현의 측정 결과**. 코덱은 DMG8-Fast를 메인 기준선으로 유지하고 신규 모델은 별도 `DMP1` 실험 포맷으로 보존했다.

## 결론

**오프닝·미들게임 모두에서 3.89 bit/반수 이하를 안정적으로 만드는 목표에는 아직 실패했다.** 기존 23게임의 엔드게임은 3.703, 오프닝 4.008, 미들게임 4.069 (각 실제 CDF 이상적 로그길이 평균)이다. 기존 4게임에서도 오프닝·미들게임이 4비트를 넘었다. 새 독립 인간 대국 수천게임을 수집하거나 평가한 것은 아니다.

새 모델 중 23게임의 전체 파일이 가장 작았던 것은 **opening-mixture-endgame**: payload 6,847bit / 1,718수 = **3.98545 bit/수**, 총 893B. 기준선 DMG8-Fast는 6,857bit = 3.99127 bit/수, 895B. 차이는 고작 10bit이고 다른 4게임에서는 오프닝 성능이 악화했으므로 **메인 승격 근거가 아니다**.

## 국면 구분과 측정 방법

실제 기존 코드의 `V7Patterns::phase` 정의를 수정 없이 사용했다. `ply<24 && material_phase>=18`이면 opening, 그렇지 않고 `material_phase<=8`이면 endgame, 나머지는 middlegame. 게임마다 판과 이전 수만 초기화하고, 온라인 학습과 기회 사전은 아카이브에서 계속 축적한다. 자료에는 불법수나 비표준 초기 FEN이 없다.

국면별 지표는 **선택된 실제 수에 대해 `Σ−log₂(CDF빈도/CDF총질량)`을 합산한 후 각 국면 반수로 나눈 값**이다. 산술 부호기의 실제 payload bitstream은 게임/국면별로 물리적으로 독립되지 않으므로 한 국면에 실제 payload의 정확한 bit를 단순 배분한 것이 아니다. 반면 전체 payload_bits/총bytes는 실제 파일로 압축·복원한 실측값이다. 모델 가중치 전달이 없는 archive cold start 설정.

### A. 기존에 반복 사용된 23게임 / 1718반수 (개발 표본)

| 모델 | 실제 payload bpp | 총 파일 B | Opening 이상적 bpp | Middle 이상적 bpp | End 이상적 bpp | 인코드 수/s |
|---|---:|---:|---:|---:|---:|---:|
|DMG8-Fast|3.9913|895||4.008|4.069|3.703|87,325|
|legal-opportunity|4.0943|917||4.066|4.189|3.841|182,374|
|relation-ae|4.0506|907||4.023|4.138|3.821|86,412|
|relation-mixture|3.9936|895||3.991|4.081|3.709|75,149|
|shared|3.9913|895||4.008|4.069|3.703|93,551|
|residual-1|3.9930|895||4.009|4.073|3.699|80,782|
|residual-2|3.9965|896||4.010|4.080|3.695|89,789|
|residual-4|3.9994|896||4.011|4.087|3.689|92,999|
|residual-6|4.0000|896||4.012|4.089|3.685|88,484|
|endgame-only|3.9889|894||4.009|4.070|3.683|99,103|
|opening-mixture-endgame|3.9854|893||3.998|4.070|3.683|90,747|

DMG8-Fast의 국면별 반수는 opening 550, middle 893, end 275. 각각 3.89 이하 목표를 충족하려면 opening 약 0.118, middle 약 0.179 bit/수 감소가 더 필요하다. 엔드는 이미 3.89 이하이다.

DMG8-Fast의 `P(actual)<1/64` 긴 꼬리 비율: opening 10.36%, middle 11.42%, end 5.45%. 해당 수의 이상적 비용 합은 각 411.9/751.5/104.6bit다. 중반이 손실의 핵심이며 낮은 확률을 받은 착수의 패턴 분석 가치가 크다. 다만 고위험 착수에 과도하게 확률을 주면 다른 수의 평균 손실이 늘 수 있다.

### B. 별도로 보관한 기존 4게임 / 496반수 (이미 개발에 사용한 표본)

| 모델 | 실제 payload bpp | 총 파일 B | Opening 이상적 bpp | Middle 이상적 bpp | End 이상적 bpp | 인코드 수/s |
|---|---:|---:|---:|---:|---:|---:|
|DMG8-Fast|4.0625|271||4.247|4.343|3.414|103,213|
|relation-mixture|4.0827|273||4.274|4.364|3.429|89,777|
|residual-2|4.0565|271||4.250|4.338|3.402|107,172|
|residual-6|4.0504|271||4.255|4.331|3.383|108,717|
|endgame-only|4.0544|271||4.247|4.343|3.382|111,167|
|opening-mixture-endgame|4.0605|271||4.283|4.343|3.382|107,285|

이 그룹에서는 opening-mixture-endgame이 23게임 오프닝과 달리 opening을 악화시켰다. 모델 선택에서 *독립 데이터*의 필요성이 확인된다. 이 4게임도 새로운 검증 셋이 아님.

### C. 합법수 균등 무작위 16게임 / 1024반수 (진단용)

| 모델 | 실제 payload bpp | 총 파일 B | Opening 이상적 bpp | Middle 이상적 bpp | End 이상적 bpp | 인코드 수/s |
|---|---:|---:|---:|---:|---:|---:|
|DMG8-Fast|5.1426|688||4.985|5.235|—|99,453|
|relation-mixture|4.8613|652||4.764|4.918|—|79,425|
|residual-2|5.1396|687||4.976|5.235|—|87,837|
|endgame-only|5.1426|688||4.985|5.235|—|92,449|
|opening-mixture-endgame|5.0664|678||4.778|5.235|—|86,351|

이 데이터는 짧은 64반수 길이로 구성되어 endgame이 0개다. 따라서 랜덤 집합의 엔드게임 성능은 주장하지 않는다. 임의수로 발생한 cross entropy를 인간 분포의 일반화 결과로 해석하지 않는다.

## 구현한 알고리즘

### 1. 국면별 잔차 학습 (신규 DMP1 포맷)

DMG8-Fast의 24차원 관계 특징, 기준 기회 보정 사전, 모든 합법수, 정수 CDF를 그대로 재사용한다. 동일한 global 온라인 관계 가중치 θ와 별도로 **국면별 δ[3][24]**를 둔다. 후보 점수 계산에서 `score(m)=clamp(Σ_(활성 f)[θ_f+δ_phase,f], −128,128)`을 사용한다. θ는 기존 DMG8 학습 규칙으로, δ는 선택된 특징과 현재 예측분포의 기대 특징 사이의 Q12 차이를 정수 잔차 누산으로 학습한다. δ는 ±24로 제한, 해당 국면의 관측 64개마다 1tick 감쇠한다. `residual-1/2/4/6`은 국면별 학습률 실험군이다. 사전 학습 가중치 없음; encoder/decoder가 같은 현재 아카이브 prefix에서 동일하게 갱신한다.

### 2. 조건부 국면 선택

- `endgame-only`: 국면 δ 학습은 endgame에서만 켜고 opening/middle에는 원래 joint 적용
- `opening-mixture-endgame`: opening에서는 기존 5전문가 mixture, middle에서는 기존 joint, endgame에서는 joint + endgame δ(강도 6)

원래 DMG8의 expert posterior·exact 사전 경로는 opening 때만 갱신하고 다른 국면에는 joint state를 유지한다. 서로 다른 국면이라도 archive 수준의 global θ와 기회 사전은 공유하므로, endgame 변화가 이후 게임의 opening 결과에 간접적으로 영향을 줄 수 있다. 모델의 조건부 전환은 판·수순 길이에서 결정되므로 추가 분기 플래그는 필요 없지만, 새 **format ID와 profile ID**는 압축 헤더에 포함한다.

### 3. DMG8 불변성 확인

`shared` 실험 프로필은 δ=0으로 동작한다. 동일 개발 입력에서 기존 DMG8-Fast와 같은 payload bit count, per-ply CDF 로그손실, framing byte 수를 재현했다. 서로 다른 `DMP1`/`DMG8` 헤더 및 CRC 때문에 **전체 파일 byte hash는 다르다**. 기존 DMG6/7/8/9 decoder·encoder는 수정하지 않았으며 회귀 테스트가 통과했다.

## 속도와 무손실 검증

- 위 수/s는 단일 실행 환경의 `Release` 빌드, 동일 입력과 같은 모델로 3개 trial×각 3회 cold 재압축의 *중앙값*이다. I/O 및 텍스트 파싱 제외. 짧은 표본/공유 VM에서 편차가 있다. 별도 지난 보고서의 97K와 직접적인 상수 속도 비교는 부적절.
- CTest **6/6** 통과 (기존 5 + 신규). 빈 archive, 23 인간 게임, 무작위 합법 수순, 바이트 동일 재인코딩, CRC 손상 검출 및 기존 DMG8 복원.
- `-fsanitize=address,undefined` 신규 phase 테스트 성공 (`ASAN_OPTIONS=detect_leaks=0`). Rust/WASM 교차 실행은 미수행.
- `DMP1` 새 magic, version=1, 1바이트 profile ID, 게임 길이 ULEB128, 산술 payload, CRC32. decoder는 magic/profile/CRC/길이를 검사한다. PGN 주석/태그/원본 공백 복원은 범위 밖.

## 재현 방법

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/dmg_phase_bench examples/heldout/*.uci > results-phase/human23.csv
./build/dmg_phase_bench examples/human/*.uci > results-phase/human4.csv
./build/dmg_phase_bench --random > results-phase/random16.csv
./build/dmg_phase pack opening-mixture-endgame archive.dmp examples/heldout/*.uci
./build/dmg_phase unpack archive.dmp output_uci/
```

## 다음 연구 권고

1. **기존 DMG8-Fast를 메인 유지.** 신규 모델의 10bit 이익과 2B의 파일 감소는 작고, 국면별 결과가 자료군 사이에서 불안정하다.
2. 우선 middle 4.069→≤3.89, opening 4.008→≤3.89가 필요. middle의 long-tail 수 10%대가 매우 비싼 부분이므로 `P(actual)<1/64` 사례의 유형을 체계적으로 분리하고 가능한 한 cheap한 재포획·강제 체크 응수·교환 손익 등의 예측 모델을 소규모 ablation으로 실험.
3. 현재의 23+4게임은 충분치 않다. 여러 기력·시간제어·현대 오프닝을 포함한 **독립 인간 게임 최소 수천개**를 확보하고 개별 국면의 재현성을 검증해야 한다. Lichess 공개 표준 대국 아카이브는 외부 자료원 후보지만 **이번에는 다운로드/실험하지 못했다**.
4. 오프닝·미들게임·엔드게임 각 평균 ≤3.89는 동일한 압축 포맷과 사전 고정한 국면 규칙으로 측정한다. 데이터셋 특성에 따라 근본적으로 불가능할 수도 있으므로 보장은 하지 않는다. 자료를 보면 오프닝/미들게임이 현재 집중 투자 대상이다.
5. 독립 데이터에서 유의미한 개선이 검증되기 전까지 변경된 압축 프로필을 배포 기본값으로 삼지 않는다.
