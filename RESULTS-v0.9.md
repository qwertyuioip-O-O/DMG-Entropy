# DMG-Entropy v0.9 — 실제 구현 및 측정 결과

2026-10-20. 기준 자료: DMG-Entropy-Cpp-v0.8 및 사용자가 제공한 `DMG-Entropy-v0.9-Research-Spec.md`.

## 판정

**속도 개선은 성공했다. 압축률 목표는 아직 미달성이다.**

- DMG8-Fast는 기존 DMG8 관계 공동 학습의 **CDF/바이너리를 그대로 유지**하면서 엔코딩/디코딩을 가속했다.
- 새로운 DMG9 관계 차분 프로토타입은 무손실 왕복이 가능하지만, 실전 개발용 23게임에서는 기존 DMG8-Fast보다 **12 payload bit 증가**했다.
- DMG9 `auto`는 기존 DMG8의 `auto` 후보와 두 DMG9 실험 모델을 실제 인코딩해 가장 작은 **전체 파일**을 선택한다. 인코딩은 매우 느려지고 디코딩은 선택된 모델만 수행한다.
- 새 인간 기보 대규모 독립 검증은 **수행되지 않았다**. 기존 23개 자료는 반복해서 사용한 개발용 표본이다.

## 원본-vs-최적화 동일 조건 속도

원본 v0.8과 DMG8-Fast를 동일 환경에서 각각 소스부터 `g++ -std=c++20 -O3 -DNDEBUG`로 빌드했다. 동일 23게임·1718반수에 대해 5회 trial, trial당 4회 archive cold 재실행, 시간 중앙값. 파일 I/O 및 텍스트 파싱은 측정에서 제외. 단일 스레드.

| 빌드 | Payload bits | 총 bytes | Encode Mply/s | Decode Mply/s |
|---|---:|---:|---:|---:|
| 원본 v0.8 | 6857 | 895 | 0.057744 | 0.058351 |
| DMG8-Fast | 6857 | 895 | 0.097195 | 0.101677 |

**인코딩 1.68배, 디코딩 1.74배.** 다른 머신, 코어 배치 또는 긴 archive에서의 동일 배율을 보장하지 않는다. 별도 재실행에서는 Fast의 인코딩 측정값이 약 0.098~0.100 Mply/s 수준으로 변동했다.

### 골든 바이너리

- 23게임 DMG8-Fast joint stream SHA256: `cd8985086de6597f1abee0b6ca1ab938d4b85eac93db0da15ac38c548a86275b`
- 원본 연구 보고서의 DMG8 sha256과 일치. 골든 파일: `examples/golden/human23-v8-fast.dmg8`.
- 새 DMG9 `delta-opportunity` 골든: SHA256 `93d0c2285a4a48ca55edcff19624a9b6093bd18cafe2b61b81ec43cde920ce8b`; `examples/golden/human23-delta-opportunity.dmg9`.

## DMG9-R1 실험 구조

새 바이너리는 `DMG9` magic/version 1을 사용. 원본 DMG6/7/8 decoder는 변경하지 않음.

- 표준 체스 시작 위치, 모든 합법 후보, from/to/승격 순서 고정.
- 16개 특징: 출발 칸의 위험, 목적지 위험/지원, 포획/재포획/가치, 체크, Q/R/minor 대상의 공격·지원 상태 차분 (각 -1/0/+1), 승격, 캐슬링, 지난 자기 착수 도착 칸에서 다시 이동 여부.
- 연구 명세의 지속적인 piece ID 추적 대신 마지막 특징은 **저비용 칸 연속성 근사**다. 공격 변화 역시 살아남은 칸/기물 종류의 기하학적 공격 관계를 비교하는 첫 프로토타입이며 연구 명세의 완전한 identity-tracking 구현이 아니다.
- 기존 전술 가중치에 선택 가능한 v7식 기회 보정 사전의 341~3072 정수 ratio를 곱한다.
- 학습: `theta[16]` 정수 [-32,+32], 잔차 Q12, 관측 및 CDF 기준 기대 특징의 차이, 64 수마다 shrink. exp2 고정 LUT / 64틱/비트. 정수 largest-remainder로 정확히 16384 total CDF, 모두 양의 질량.
- `delta-opportunity`: 사전+관계 차분; `delta-no-pattern`: 사전 제외; `auto`: v8-auto와 위 2개를 실제 완전 인코딩 후 선택.
- 포맷은 간단한 DMG9 header + 게임 길이 varint + 공통 arithmetic bitstream + CRC32. **상위 연구 보고서에서 제안한 청크·전달모델·인덱스형 전체 컨테이너는 아직 구현하지 않았다.**

## 실전 개발 표본: 23게임, 1718반수

| 방식 | Payload bits | Payload bit/ply | 전체 bytes | Total bit/ply | Encode Mply/s | Decode Mply/s |
|---|---:|---:|---:|---:|---:|---:|
| DMG8-Fast relation-joint | 6857 | 3.991269 | 895 | 4.167637 | 0.097195 | 0.101677 |
| DMG9 delta-opportunity | 6869 | 3.998254 | 896 | 4.172293 | 0.085705 | 0.085646 |
| DMG9 delta-no-pattern | 7100 | 4.132712 | 925 | 4.307334 | 0.091426 | 0.092225 |
| DMG9 auto | 6857 | 3.991269 | 895 | 4.167637 | 0.007156 | 0.102982 |

**새 관계 차분 모델은 이 개발 표본에서 개선되지 않았다.** 모델이 기존 24개 관계 모델보다 적은 특징을 사용한다고 반드시 효율적인 것은 아니다.

## 무작위 합법 수순: 16게임, 1024반수

| 방식 | Payload bit/ply | Total bytes | Total bit/ply | Encode Mply/s |
|---|---:|---:|---:|---:|
| DMG8-Fast joint | 5.142578 | 688 | 5.375000 | 0.096033 |
| DMG9 delta-opportunity | 5.221680 | 698 | 5.453125 | 0.080599 |
| DMG9 delta-no-pattern | 5.253906 | 702 | 5.484375 | 0.087144 |
| DMG9 auto (v6/v7/v8/v9 탐색) | 4.861328 | 652 | 5.093750 | 0.006749 |

모델 혼합을 포함한 기존 옵션이 무작위 합법 착수에서 더 유리함. 무작위 과정의 정보이론적 엔트로피가 3.9bit라는 의미는 아님.

## 동일 4개 개발 게임 3회 반복: 12게임, 1488반수

| 방식 | Payload bit/ply | Total bytes | Total bit/ply |
|---|---:|---:|---:|
| DMG8-Fast joint | 3.780242 | 735 | 3.951613 |
| DMG9 delta-opportunity | 3.806452 | 739 | 3.973118 |
| DMG9 delta-no-pattern | 4.153898 | 804 | 4.322581 |
| DMG9 auto | 2.494624 | 495 | 2.661290 |

극단적으로 중복된 *인공* 데이터에서 기존 포지션 사전 방식의 장점을 재확인한 것. 일반적인 인간 기보 압축률로 해석하면 안 된다.

## 추가로 구현·기각한 실험: 후보 공격 관계 증분 합성

현재판의 기물별 기하학 공격 집합을 한 번 캐시하고, 착수로 바뀌는 칸과 같은 선에 있는 sliding pieces만 다시 평가하는 `V9SharedAttacks`를 구현했다. EP/캐슬링/승격을 포함하고, `RMC_VERIFY_ATTACKS=ON`에서 full-map oracle 비교를 통과했다.

하지만 23게임에서 DMG9 사전+차분 모델의 인코딩 약 0.0857→0.0814 Mply/s로 오히려 느려졌다. dirty slider 탐색/bitwise union 관리의 비용이 이득을 초과했기 때문일 수 있다. 이 원인 분석은 추정이며 microprofile 증명은 아니다.

따라서 **기본값 OFF**, `-DDMG9_INCREMENTAL_ATTACKS=ON` 실험 옵션으로 보존했다. 동일 binary 및 oracle 테스트 통과 외에 범용 속도 이익은 주장하지 않는다.

## 검증

- CMake/CTest **5개 테스트 전체 통과**.
- `dmg_v9_tests`: 실전 23게임, 합법 무작위 12게임, 빈 archive, 결정론적 byte-output, 원본 재생, CRC32 손상 검출, legacy DMG7/8 복원.
- `RMC_VERIFY_ATTACKS=ON`·`RMC_VERIFY_LEGAL_FAST=ON`·증분 옵션 ON의 oracle 빌드에서 V8/V9 검증 통과.
- ASan + UBSan 빌드의 v8/v9 테스트 통과 (`ASAN_OPTIONS=detect_leaks=0`). LeakSanitizer와 cross-platform Rust/WASM/ARM 테스트는 미수행.
- 골든 sample의 고정 byte 스트림 비교 통과.

## 미완료 및 다음 작업

1. **독립적인** 다양한 현대 인간 기보 수천~수만 게임 수집. 개발 중 사용한 23게임으로 3.9bit 범용성을 주장할 수 없음.
2. DMG8-Fast의 byte 호환 경로를 유지한 상태에서 incremental attack의 실제 병목을 세분화해 더 적은 메모리 쓰기/dirty 검사로 실험.
3. delta 모델의 low-cost SEE, 위험 구간, 학습률/꼬리비용 ablation. 실시간 처리량보다 압축 이익이 작으면 중단.
4. 정확한 piece ID, 보드 overlay, chunk framing, bounded pattern table, Rust/WASM golden CDF 검증.
5. 3.9 **payload**와 3.9 **total**을 별도 목표로 유지. 현재 개발표본은 둘 다 미달성.

실측 상세 CSV: `results-v0.9/*.csv`.
