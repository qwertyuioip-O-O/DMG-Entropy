# DMG-Entropy

[English](#english) | [한국어](#한국어)

<a id="english"></a>

## English

**A C++20 research codec for lossless chess move-sequence compression.**

DMG-Entropy models legal moves as they are played and entropy-codes the resulting sequence. This repository includes versioned formats, compatibility tests, reproducible tools, and research reports.

> **Current focus:** portable encoding and decoding speed, measured on ordinary, elite, and uniformly random legal games. This is research software, not yet a drop-in replacement for general PGN archives.

### What it stores

The codec preserves standard chess move sequences, including move order, game boundaries, castling, en passant, and promotion. It does **not** preserve PGN tags, comments, variations, whitespace or formatting, or arbitrary starting positions/FENs. It is not a PGN parser or a search-ready opening-book format.

The runtime model is deterministic and integer-based. Encoding and decoding use the same model updates, so a decoder can reconstruct the move stream without a pretrained neural model or external chess engine.

### Current benchmark

The latest three-cohort evaluation covers **15,000 games and 2,547,354 plies** (5,000 games per cohort). It reports complete framed archive size and single-core CLI throughput for the selected portable build.

| Game cohort | Framed bits/ply ↓ | Encode plies/s ↑ | Decode plies/s ↑ |
|---|---:|---:|---:|
| Ordinary human games | 3.475 | 78,025 | 77,816 |
| Elite games | 3.610 | 77,231 | 76,925 |
| Uniformly random legal games | 4.508 | 101,053 | 102,856 |

Measurements were run on one pinned logical CPU on an AMD EPYC 9V74 host. Results depend on the corpus, host, compiler, and CLI measurement path; they are not universal performance claims. The random cohort is a diagnostic distribution, not a proxy for human games.

See the [full cohort benchmark report](README-Cohorts-20261023-KO.md) for cohort definitions, data handling, commands, and limitations. The [current speed baseline](CURRENT-SPEED-BASELINE.md) documents the selected build and confirmation protocol. Compression figures include archive framing; older payload-only bits-per-ply figures are not directly comparable.

### Download and build

Download and extract the [DMT19 source and research bundle](DMG-Entropy-DMT19-Source.zip), then build and run the tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The bundle includes source code, tests, examples, benchmark tools, and selected reports. Raw game corpora, local benchmark binaries, and build outputs are excluded.

### Project contents

- **Versioned codecs and compatibility tests** — current and legacy formats with regression coverage.
- **Benchmark tools and reports** — cohort-level compression and speed measurements, plus controlled optimization experiments.
- **Research history** — model and implementation variants, including rejected candidates and measured trade-offs.

Start with the [cohort benchmark report](README-Cohorts-20261023-KO.md), then consult dated experiment reports for implementation details.

### Scope and limitations

- Input is a standard-start chess move sequence; the project does not currently encode the full PGN document.
- No claim is made that DMG-Entropy beats every chess-specific or general-purpose compressor. Comparisons require matching the corpus, framing, and measurement procedure.
- Speed and size vary with game distribution, hardware, compiler, and build configuration.
- Experimental formats and model choices are versioned; use the compatibility tests and format-specific tools when exchanging archives.

### License

See the source bundle for the licenses and notices that apply to the included code.

---

<a id="한국어"></a>

## 한국어

**체스 기보의 수순을 손실 없이 압축하는 C++20 연구용 코덱입니다.**

DMG-Entropy는 체스에서 가능한 합법 수를 진행 순서에 따라 모델링하고, 그 수순을 엔트로피 부호화합니다. 이 저장소에는 버전별 압축 형식, 호환성 테스트, 재현 가능한 도구와 연구 보고서가 포함되어 있습니다.

> **현재 연구 초점:** 일반인 기보, 상위권 선수 기보, 합법 수 중 무작위로 생성한 기보에서 인코딩·디코딩 속도를 측정합니다. 범용 PGN 압축기를 대체하는 완성품이 아니라 연구용 소프트웨어입니다.

### 저장하는 데이터

표준 시작 포지션에서 진행한 체스 수순을 저장합니다. 수순의 순서와 대국 경계, 캐슬링, 앙파상, 프로모션을 보존합니다. 반면 PGN 태그, 주석, 변형 수순, 공백이나 서식, 임의의 시작 포지션/FEN은 보존하지 않습니다. 따라서 PGN 문서 파서나 검색용 오프닝 북 형식은 아닙니다.

실행 중 사용하는 모델은 결정론적 정수 연산을 기반으로 합니다. 인코더와 디코더가 같은 모델 갱신을 수행하므로, 사전 학습된 신경망 모델 파일이나 외부 체스 엔진 없이 수순을 복원합니다.

### 최신 벤치마크

가장 최근의 세 코호트 평가에는 **총 15,000개 기보와 2,547,354개 반수**가 포함되어 있습니다(코호트별 5,000개). 아래는 선택된 이식성 중심 빌드의 전체 아카이브 크기와 단일 코어 CLI 처리량입니다.

| 기보 분류 | 프레이밍 포함 비트/반수 ↓ | 인코딩 반수/초 ↑ | 디코딩 반수/초 ↑ |
|---|---:|---:|---:|
| 일반인 기보 | 3.475 | 78,025 | 77,816 |
| 상위권 선수 기보 | 3.610 | 77,231 | 76,925 |
| 합법 수 무작위 기보 | 4.508 | 101,053 | 102,856 |

측정은 AMD EPYC 9V74 호스트에서 논리 CPU 하나를 고정해 수행했습니다. 결과는 사용한 기보, 호스트, 컴파일러와 CLI 측정 방식에 따라 달라지며 모든 환경에서 같은 성능을 보장하지 않습니다. 무작위 기보는 진단용 분포이며 인간의 기보를 대표하지 않습니다.

코호트 정의, 데이터 처리, 실행 명령과 한계는 [전체 벤치마크 보고서](README-Cohorts-20261023-KO.md)를 참고하세요. 선택된 빌드와 확인 절차는 [현재 속도 기준선](CURRENT-SPEED-BASELINE.md)에 정리되어 있습니다. 표의 압축률은 아카이브 프레이밍을 포함합니다. 과거 보고서의 페이로드만 계산한 비트/반수 수치와는 직접 비교할 수 없습니다.

### 다운로드 및 빌드

[DMT19 소스·연구 묶음](DMG-Entropy-DMT19-Source.zip)을 내려받아 압축을 푼 뒤, 다음 명령으로 빌드하고 테스트를 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

묶음에는 소스 코드, 테스트, 예제, 벤치마크 도구와 일부 연구 보고서가 들어 있습니다. 원본 기보 데이터셋, 로컬 벤치마크 실행 파일, 빌드 결과물은 포함되어 있지 않습니다.

### 저장소 구성

- **버전별 코덱과 호환성 테스트** — 최신 및 이전 형식에 대한 회귀 테스트를 포함합니다.
- **벤치마크 도구와 보고서** — 코호트별 압축률·속도 측정과 통제된 최적화 실험을 담고 있습니다.
- **연구 기록** — 채택하지 않은 후보와 측정된 장단점을 포함한 모델·구현 실험입니다.

전체 평가부터 보려면 [코호트 벤치마크 보고서](README-Cohorts-20261023-KO.md)를 확인하고, 구현별 자세한 내용은 날짜가 붙은 실험 보고서를 참고하세요.

### 범위와 한계

- 입력은 표준 시작 포지션의 체스 수순입니다. 현재 전체 PGN 문서를 인코딩하지 않습니다.
- 모든 체스 전용 또는 범용 압축기보다 우수하다고 주장하지 않습니다. 비교하려면 데이터셋, 프레이밍 범위, 측정 절차를 맞춰야 합니다.
- 압축 크기와 속도는 기보 분포, 하드웨어, 컴파일러와 빌드 설정에 따라 달라집니다.
- 실험 형식과 모델은 버전별로 관리됩니다. 아카이브를 교환할 때는 호환성 테스트와 해당 형식의 도구를 사용하세요.

### 라이선스

포함된 코드의 라이선스와 고지 사항은 소스 묶음 안의 문서를 확인하세요.
