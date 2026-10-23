# Lichess / Chessbook과 DMG-Entropy 비교

측정일: 2026-10-23. 세 집단은 고정된 동일 5,000판 코호트이며, 합계 15,000판/2,547,354 반수다. Lichess의 공개 코덱과 현재 upstream의 rank/select 최적화를 Java로 포팅해 현 CPU 1에서 실행했다. 출력이 이전 공개 구현과 15,000판 모두 바이트 단위로 같고, 복호화도 전부 일치했다. Chessbook의 3.7 bit/수 코덱은 공개 소스나 실행 파일이 없어 직접 실행하지 못했다.

## 결과

| 집단 | 기보/반수 | DMG 비트/수 (payload / framed) | Lichess 비트/수 | DMG 인코딩 수/초 | Lichess 인코딩 수/초 | DMG 디코딩 수/초 | Lichess 디코딩 수/초 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| general | 5,000 / 347,515 | 3.347 / **3.475** | 4.473 | 78,025 | 1,195,599 (**15.3×**) | 77,816 | 1,089,384 (**14.0×**) |
| elite | 5,000 / 455,147 | 3.503 / **3.610** | 4.723 | 77,231 | 1,218,244 (**15.8×**) | 76,925 | 974,418 (**12.7×**) |
| random | 5,000 / 1,744,692 | 4.461 / **4.508** | 5.100 | 101,053 | 1,462,311 (**14.5×**) | 102,856 | 1,367,867 (**13.3×**) |

DMG의 수치와 Lichess의 바이트 수는 포장 경계가 완전히 같지 않다. DMG framed 값은 전체 D19 파일의 헤더·구조·체크섬을 포함한다. Lichess 값은 각 기보를 독립 인코딩한 바이트 배열을 합산해 게임마다 끝 바이트까지 센 값이며, 행 길이와 플레이어·대회 정보 등 데이터베이스 필드는 뺐다. 이 조건에서도 세 집단 모두 DMG 파일의 framed 비트/수가 더 낮게 나왔다. 정확한 비교를 위해 두 payload 수치와 DMG framed 수치를 모두 보존했다.

속도는 같은 EPYC 9V74 논리 CPU 1에 고정했다. Lichess 코드는 OpenJDK 17에서 측정했다. 현재 upstream의 move-list rank/select 알고리즘을 Java로 옮겨 2023년 Java 기준 구현에 적용했으며, 나머지 최신 Scala writer 최적화는 포팅하지 않았다. 코덱에 기보별 SAN 배열을 메모리에서 전달하며, 전체 코호트 워밍업 후 다섯 번 처리한 중앙 처리량이다. 15,000판 모두 인코딩 후 정확히 SAN으로 복원됐다. DMG 속도는 기존 native CLI 확인 자료에서 가져왔다. Lichess 측정은 PGN 파싱 및 UCI→SAN 준비를 제외하고, DMG native CLI의 입력 경로와 완전히 같은 타이밍 경계는 아니므로 속도 배수는 같은 조건의 정식 순위로 해석하면 안 된다.

## Chessbook 수치의 범위

Chessbook 글은 법적 수를 생성하고 순위를 매긴 뒤 수 인덱스를 부호화하는 구조, Lichess식 Huffman 대신 산술 부호화를 쓴 점, 오프닝 라인용 기물-제곱표를 따로 튜닝한 점을 설명한다. 제목의 3.7 bit/수는 Chessbook의 오프닝 라인 압축 결과다. 공개 글에는 그 코덱의 실행 가능한 소스/바이너리와 동일한 15,000판에서의 속도 결과가 없다. 따라서 3.7을 이 표의 실측 칸에 넣거나, 15,000판 전체 기보 결과라고 부르지 않았다.

## 재현 및 한계

- 데이터 입력은 이전에 고정한 5,000판 일반인, 5,000판 Elo 2600 이상 엘리트, 5,000판 균등 합법 수 랜덤 집단이다. 원본 UCI 입력 SHA-256은 `san/manifest.json`에 있다. SAN 변환 파일 해시도 같은 manifest에 기록했다.
- Lichess upstream HEAD는 `3e815a1ccd294d8f25621c7cb1be96bf21b09209`이며, move-list 최적화는 `c3be3f2`의 rank/select 방식을 포팅했다. 기반 Java 구현은 `e675db1936eaf6dc0c5f718366760f00a5b7b47d`이다. 현재 Scala writer의 다른 최신 최적화는 포함하지 않아, Lichess 최신 빌드 자체의 실측 속도라고 해석하지 않는다. 압축 바이트는 15,000판 전부 기반 구현과 일치했다.
- 최종 Lichess 반복 5회의 전체 시간을 `lichess-java-currentport/results.csv`에 남겼다. 이전 Java baseline은 `lichess-java/results.csv`에 보존했다. 두 구현의 바이트 일치 검사는 `runner/CompareLichess.java`로 수행했다. 첫 전체 코호트 워밍업은 별도이며 시간 통계에 넣지 않았다. outlier 삭제는 하지 않았다.
- DMG의 95% block bootstrap 신뢰구간은 원래 cohort 보고서에 있다. Lichess는 전체 데이터 반복 처리 5회 중앙값만 측정했고 독립 원본 데이터 블록별 신뢰구간을 계산하지 않았다.
- Chessbook 비교를 완성하려면 3.7 bit/수 방식의 실행 코드나 동일 데이터셋에서 내보낸 결과물이 필요하다.

## 출처

- Lichess compression repository: https://github.com/lichess-org/compression
- Lichess encoding design: https://lichess.org/@/lichess/blog/developer-update-275-improved-game-compression/Wqa7GiAA
- Chessbook 3.7-bit article: https://chessbook.com/blog/compressing-chess-moves-even-further
- Chessbook earlier SAN codec article: https://chessbook.com/blog/compressing-chess-moves
