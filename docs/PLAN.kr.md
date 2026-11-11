# LatteArt 개발 계획

Team 22 · 영상처리 Team Project 02 (Global / Startup 팀)
작성: 2026-11-11 (역할·일정 개편) · 원래 기획서는 [Team22_LatteArt_Plan.docx](Team22_LatteArt_Plan.docx) 참고 · English: [PLAN.en.md](PLAN.en.md)

---

## 1. 과제 요구사항 요약

| 항목 | 내용 |
|---|---|
| 언어 / 환경 | C++, **Visual Studio에서 컴파일 가능**해야 함 |
| 라이브러리 | **OpenCV 입출력 함수만 사용 가능** (`imread`, `imwrite`, `imshow`, `waitKey`). 나머지 알고리즘은 전부 직접 구현 |
| 예외 (FAQ Q1) | 프로젝트 핵심 아이디어가 아닌 보조 단계는 라이브러리 사용 가능. 단, **보고서에 사용한 함수와 우리 방식의 기여점을 명시**해야 함 |
| UI | GUI 필수 아님. 콘솔 프로그램으로 충분 |
| 가점 | Global 팀 +5점, Startup 계획 추가 시 가점. **발표 첫 장에 "Global / Startup team" 명시** |
| 발표 | 발표자는 무작위로 선정되므로 **전원이 발표를 준비** |

### 제출물과 마감

| 마감 | 제출물 | 제출 위치 |
|---|---|---|
| **11/28(토) 22:00** | `Team22_LatteArt_Src.zip`: VS 솔루션 전체 + **컴파일러가 만든 파일 전부**(`bin/`, `build/`, OpenCV DLL) | e-Class / Team project 02 - SRC |
| **12/1(화) 22:00** | `Team22_LatteArt_PPT.zip`: 보고서(Word), 발표 자료, 데모 영상 (**영문 이름 포함**) | e-Class / Team project 02 - PPT |
| **12/1(화) 22:00** | `Team22_LatteArt_Final.zip`: 위와 같은 구성 | e-Class / Team Project / Final |
| **12/3(목)** | 최종 발표. 12/10까지 이어질 수 있으니 e-Class에서 순서 확인 | 수업 시간 |

### 최종 보고서(Word) 필수 항목
1. 프로그램 개요
2. 프로그램 설계 (모듈 구조, 각 알고리즘)
3. 프로젝트 타임라인 (git 커밋 이력 활용)
4. **그룹 토론 과정** (카카오톡 캡처, 회의 사진 등) → `docs/meetings/`에 계속 모으기
5. 팀의 소감
6. 기타 (라이브러리 사용 내역, 한계, 사업 계획)

---

## 2. 개발 환경 (전원 동일하게)

| 항목 | 설정 |
|---|---|
| IDE | **Visual Studio 2022, 툴셋 v143** (수업 자료 기준). VS 2026을 쓰면 v143 빌드 도구를 추가 설치하고, 툴셋 변경 창이 뜨면 **취소** |
| 플랫폼 | **x64** (공식 OpenCV Windows 빌드는 x64 전용) |
| OpenCV | **4.12.0** Windows 패키지, `C:\opencv`에 압축 해제 |
| 환경변수 | `OPENCV_DIR=C:\opencv\build`, `OPENCV_VER=4120`, `OPENCV_VC=vc16` |

설치 방법은 [README.md](../README.md)의 "One-time setup" 참고.

### 코딩 규칙
- `cv::Mat`은 **컨테이너로만** 사용: 생성, `ptr<T>`, `at<T>`, `rows`, `cols`, `clone`
- `cv::` 처리 함수(`cvtColor`, `GaussianBlur`, `resize`, `threshold`, `Canny`, `line`, `putText` 등)와 Mat 산술 연산자는 **금지**
- OpenCV 입출력 호출은 `src/io/`에서만
- 머지 전에 반드시 점검 스크립트 실행:
  ```
  powershell -ExecutionPolicy Bypass -File scripts\check_library_usage.ps1
  ```

---

## 3. 프로그램 구조

```
사진 2장(기준, 사용자) + 목표 패턴(예: tulip:3)
   │
   ├─ normalize   컵 테두리 검출 → 타원 피팅 → 기울기 보정 → 크기 통일 → 패턴 축 세로 정렬
   ├─ structure   패턴 분할 → 축 방향 밝기 프로파일 → 층 수 계산 → FFT 교차검증 → 균일도 U
   ├─ quality     대비 · 모양 · 질감 · 위치 · 크기 → 품질 점수 Q
   ├─ scoring     층 수 오차로 점수 구간 결정 → S = L + (H − L)(0.3U + 0.7Q)
   └─ viz         콘솔 리포트 + 결과 이미지 저장/표시
```

### 실행 모드

| 명령 | 용도 |
|---|---|
| `LatteArt.exe <기준.jpg> <내것.jpg> tulip:3 [result.png]` | 채점 (최종 프로그램) |
| `LatteArt.exe --canon <사진> <cx> <cy> <r> <out.png>` | **수동 정규화** (구현 완료). 컵 중심과 반지름을 그림판에서 읽어 넣으면 512 규격 이미지 생성 |
| `LatteArt.exe --debug <단계> ...` | **단계별 디버그 모드.** 한 단계만 실행하고 중간 결과를 `debug_<단계>_*.png`로 저장. 단계: `preprocess`(Junkyu), `normalize`·`layers`(Jaeku), `segment`·`quality`(Ranju), `viz`(Cecelia). 각 단계 파일 `src/debug/<단계>.cpp`는 그 단계 담당자만 수정. 보고서·데모에서 중간 결과를 보여줄 때도 사용 |
| `LatteArt.exe --batch ...` | 데이터셋 일괄 채점 → CSV (Junkyu 구현) |

### 정규화 규칙 (모든 모듈 공통 약속)
- 정규화된 컵 이미지는 **512 × 512**
- 컵 테두리는 **반지름 240px**, 이미지 정중앙
- 패턴 주축은 **위쪽(↑)** 방향
- 자동 정규화가 완성되기 전에는 **`--canon`으로 만든 이미지**로 각자 개발

### 점수 구간 (기획서 기준, 구현 완료)

| 층 수 오차 Δn | 점수 구간 |
|---|---|
| 0 | 60 ~ 100 |
| 1 | 30 ~ 59 |
| 2 이상 | 0 ~ 29 |

품질 가중치 (초안, 데이터로 조정): 대비 30% · 모양 25% · 질감 20% · 위치 15% · 크기 10%

---

## 4. 역할 분담 (충돌 최소화)

### 충돌 방지 규칙
1. **파일 단위 소유.** 모든 소스 파일 첫 줄에 `// Owner: 이름`이 적혀 있습니다. **자기 파일만 수정**합니다.
2. **공용 파일(SHARED)은 따로 PR + 전원 승인.** `core/types.h`, `core/todo.h`, `io/`, `app/main.cpp`, `debug/debug.h`, `LatteArt.vcxproj(.filters)`, `opencv.props`
3. **테스트는 자기 단계의 debug 파일에서.** `--debug <단계>`로 실행하고 `main.cpp`는 건드리지 않습니다.
4. **남의 함수가 필요하면 직접 고치지 말고 담당자에게 요청**합니다(단톡 또는 GitHub Issue). 기다리는 동안에는 자기 debug 파일 안에 임시 구현을 만들어 씁니다.
5. **필요한 파일은 이미 전부 만들어 프로젝트에 등록**해 두었습니다. 새 파일 추가는 가급적 피하고, 꼭 필요하면 vcxproj만 고치는 작은 PR을 따로 올립니다.
6. 내 모듈에만 필요한 구조체는 `types.h`에 넣지 말고 **내 헤더에** 정의합니다.

### 담당 파일

| 담당 | 역할 | 소유 파일 |
|---|---|---|
| **Jaeku Kang** | 정규화 · 층 수 · 점수 · 전체 연결 | `basic/edge`, `basic/geometry`, `normalize/*`, `structure/layers`, `scoring/*`, `app/pipeline`, `debug/normalize.cpp`, `debug/layers.cpp` |
| **Ranju Kim** | 패턴 분할 · 품질 지표 5개 · FFT | `basic/threshold`, `basic/morphology`, `basic/fft`, `structure/segment`, `quality/*`, `debug/segment.cpp`, `debug/quality.cpp` |
| **Junkyu Kim** | 기초 필터 · 색 변환 · 결과 출력 · 배치 검증 | `basic/filter`, `basic/color`, `basic/clahe`, `viz/report`, `app/batch`, `debug/preprocess.cpp` |
| **Cecelia Hein** | 결과 이미지 그리기 함수 · 자료 수집 · 평가 진행 · 문서 · 발표 | `viz/draw`, `debug/viz.cpp`, `data/`, `docs/` |

### Cecelia Hein 담당 업무
- **결과 이미지 그리기 함수** (`src/viz/draw.cpp`): 사각형·십자 → 이미지 나란히 붙이기 → 글자(5×7 폰트) → 직선·원 순서로 구현. 함수마다 방법이 주석으로 적혀 있음. `--debug viz`로 결과 확인
- **기준 사진 확보**: 하트·튤립 전문가 사진
- **연습 사진 수집** 약 10장 (하트·튤립 섞어서, 층 수를 맞춘 컵과 틀린 컵을 모두 포함), 각 사진의 **층 수 정답**을 `data/labels.csv`에 기록
- **`--canon` 이미지 만들기**: 그림판에서 컵 중심(cx, cy)과 반지름(r)을 읽어 실행
- **회의 기록**: 단톡 캡처와 회의 사진을 `docs/meetings/`에 정리 (보고서 ④)
- **시장·경쟁 앱 조사** 업데이트, 사업 계획 정리
- **보고서 취합**(①개요 ③타임라인 ④토론 ⑤소감 ⑥사업), **발표 자료**, **데모 영상** 촬영·편집, 영어 교정, 리허설 진행

---

## 5. 일정

### 주고받는 날짜 (이 날짜가 밀리면 다른 사람이 막힘)

| 날짜 | 전달물 | 보내는 사람 → 받는 사람 |
|---|---|---|
| **11/13** | `gaussianBlur`, `convolve2D` | Junkyu → Jaeku (Canny), Ranju (분할, LoG) |
| **11/14** | 기준 사진 2장 + 연습 사진 약 10장 (원본, `--canon` 이미지, 층 수 정답) | Cecelia → 개발 3명 |
| **11/20** | 그리기 함수 (`drawRect`, `drawCross`, `sideBySide`, `drawText`, `drawLine`, `drawCircle`) | Cecelia → Junkyu (결과 이미지) |
| **11/15** | `segmentPattern` | Ranju → Jaeku (패턴 축 PCA) |
| **11/17** | `fft`, `magnitudeSpectrum` | Ranju → Jaeku (층 수 교차검증) |
| **11/17** | `normalizeCup` | Jaeku → 전원 |
| **11/22** | `analyzeLayers` | Jaeku → Junkyu (결과 이미지) |
| **11/24** | 배치 결과 CSV | Junkyu → Jaeku, Ranju (임계값 점검) |

### Jaeku Kang

| 기간 | 할 일 | 완료 기준 |
|---|---|---|
| 11/12 ~ 11/14 | `sobel`, `canny`, `warpPerspective` (Gaussian이 오기 전에는 `debug/normalize.cpp` 안에 임시 블러) | 실제 사진의 엣지 맵 저장 |
| 11/15 ~ 11/17 | `findRimPoints`, `fitEllipse`, `normalizeCup`(타원→원, 크기 맞춤), `patternAxisAngleDeg` | 사진 10장이 512 규격으로 자동 정규화 |
| 11/18 ~ 11/21 | `axisProfile`, `findPeaks`, 간격·두께 CV, FFT 교차검증, U | 기준 사진에서 목표 층 수 검출 |
| 11/22 | `pipeline` 끝까지 연결 | 사진 2장 → 최종 점수 출력 |
| 11/23 ~ 11/26 | 배치 결과로 prominence 등 임계값 점검, 버그 수정 | 10장의 층 수 정확도 확인 |

### Ranju Kim

| 기간 | 할 일 | 완료 기준 |
|---|---|---|
| 11/12 ~ 11/14 | `otsuThreshold`, `binarize`, `erode`/`dilate`, `largestComponent` | `--debug segment`로 이진화 결과 저장 |
| 11/15 | `segmentPattern` → Jaeku에게 전달 | `--canon` 이미지에서 패턴 마스크 저장 |
| 11/15 ~ 11/17 | `fft`, `magnitudeSpectrum`(→ Jaeku), `evalSize`, `evalPosition` | 크기·위치 점수 출력 |
| 11/18 ~ 11/20 | `fft2Magnitude`, `evalContrast` (Michelson, 경계 폭, 고주파 비율) | 대비 점수 출력 |
| 11/20 ~ 11/22 | `evalShape` (mirror IoU, Hu), `evalTexture` (GLCM, LoG, 하이라이트) | 5개 항목 점수 + 피드백 문장 |
| 11/23 ~ 11/26 | σ · 가중치 점검 (데이터가 적어 큰 조정은 하지 않음), 피드백 문장 다듬기 | 층 수를 맞춘 깔끔한 컵이 높은 품질 점수를 받음 |

### Junkyu Kim

| 기간 | 할 일 | 완료 기준 |
|---|---|---|
| **11/12 ~ 11/13** | ★ **`gaussianBlur`, `convolve2D` 최우선** → 바로 main에 머지 | 다른 사람이 사용 가능 |
| 11/14 ~ 11/17 | `bgrToLab`, `clahe` | `--debug preprocess`로 결과 저장 |
| 11/18 ~ 11/20 | `printReport` 다듬기, `runBatch` 준비 (labels.csv 읽기) | 콘솔 리포트 완성 |
| 11/21 ~ 11/22 | `renderResult` (축, 층 위치, 문제 영역 표시. Cecelia의 그리기 함수 사용) | 결과 이미지 저장 |
| 11/22 ~ 11/24 | `runBatch` (labels.csv → 결과 CSV) | 전체 사진 일괄 채점 |
| 11/25 ~ 11/26 | 층 수 정확도 표 (검출 층 수 vs 정답), 실패 사례 정리 | 보고서용 수치 |

### Cecelia Hein

| 기간 | 할 일 | 완료 기준 |
|---|---|---|
| 11/12 ~ 11/13 | 개발 환경 설치 (README 참고), `drawRect`, `drawCross` | `--debug viz`에 사각형·십자 표시 |
| 11/12 ~ 11/14 | 기준 사진(하트·튤립) 확보, **연습 사진 약 10장 수집**, 층 수 정답 기록, `--canon` 이미지 | **11/14 전달** |
| 11/15 ~ 11/17 | `sideBySide`, `drawText` (5×7 폰트 표 입력) | 두 컵이 나란히, 점수 글자 표시 |
| 11/18 ~ 11/20 | `drawLine`, `drawCircle` | **11/20 전달** |
| 11/23 ~ 11/28 | 보고서 초안(①③④⑤⑥), 발표 자료 초안, 시장 조사 업데이트 | 초안 공유 |
| 11/29 ~ 12/1 | 데모 영상 촬영·편집, 보고서·PPT 취합 및 제출 | 두 zip 제출 |
| 상시 | 회의 기록·단톡 캡처 정리 | `docs/meetings/` |

### 전원 공통

| 날짜 | 할 일 |
|---|---|
| **11/13** | `types.h` 구조체와 정규화 규칙 최종 합의 |
| **11/27 ~ 11/28** | **코드 동결**, 라이브러리 점검, 다른 PC에서 클린 빌드, Src zip 제출 (Jaeku 주관) |
| 11/29 ~ 12/1 | 개발 3명: 보고서 ②설계와 오류 분석 작성 → Cecelia에게 전달 |
| **12/2** | 전원 발표 리허설 |
| **12/3** | 최종 발표 |

### 마일스톤
- **11/17**: 자동 정규화 완성
- **11/22**: 사진 2장과 패턴을 넣으면 최종 점수와 결과 이미지가 나오는 상태
- **11/26**: 임계값 점검 완료, 실패 사례 정리
- **11/27**: Src zip 1차본 미리 만들어 두기

---

## 6. 품질 지표 상세 (Ranju Kim)

| 항목 | 가중치 | 측정 방법 | 구현 파일 |
|---|---|---|---|
| 크기 | 10% | 패턴 면적 ÷ 컵 면적을 기준 사진과 비교 | `position_size.cpp` |
| 위치 | 15% | 패턴 중심과 컵 중심의 거리 ÷ 반지름, 축 기울기 | `position_size.cpp` |
| 대비 | 30% | Michelson 대비, 경계 폭(edge width), 고주파 비율 | `contrast.cpp` |
| 모양 | 25% | 좌우 반전 IoU(대칭), Hu 모멘트 거리 | `shape.cpp` |
| 질감 | 20% | GLCM homogeneity, LoG 기포 검출, 하이라이트 비율 | `texture.cpp` |

- 각 지표는 절대값이 아니라 **기준 사진 대비 차이**로 0~1 점수화: `score = exp(-|user − ref| / σ)` (`closeness()` 구현됨). σ는 데이터로 조정
- 점수와 함께 **감점 이유 문장**을 반환 → "설명 가능한 채점"이 핵심 차별점
- 구현 순서: 크기 → 위치 → 대비 → 모양 → 질감 (쉬운 것부터)
- 합성 이미지(완벽한 원, 좌우 대칭 하트, 노이즈 표면)로 지표가 기대한 방향으로 움직이는지 확인

---

## 7. 리스크와 대응

| 리스크 | 대응 |
|---|---|
| **층 수 판정 정확도** (프로젝트 핵심) | 사진을 고를 때 위에서 수직으로 찍고 그림자가 없으며 컵 전체가 보이는 사진 위주로 선택. 알고리즘 개선보다 효과가 큼 |
| Gaussian(11/13)이 늦어 Canny·분할이 막힘 | Junkyu 최우선 처리. 늦어지면 각자 debug 파일 안에 3×3 평균 필터로 임시 대체 |
| 자동 정규화(11/17)가 늦음 | 품질·결과 화면은 `--canon` 이미지로 계속 진행 가능 |
| 타원 피팅 · 호모그래피 구현 난이도 | 일정이 밀리면 "심하게 기운 사진은 재촬영 요청"으로 범위 축소 (기획서에 명시됨) |
| 사진 데이터가 늦음 | Cecelia 전달 날짜(11/14)를 단톡에서 확인 |
| 데이터가 10장으로 적음 | 가중치는 기획서 초안을 유지하고 큰 조정은 하지 않음. 10장 안에 층 수가 틀린 컵을 꼭 포함. 보고서 "한계"에 표본 수를 명시 |
| 통합 시 충돌 | 파일 단위 소유, 공용 파일은 전원 승인, `main`은 항상 빌드되는 상태 유지 |
| 채점 PC에서 빌드 실패 | 11/27에 다른 PC에서 압축 풀고 클린 빌드 · 실행 확인 |
| 라이브러리 규칙 위반 | 머지 전 점검 스크립트, 예외 사용 시 보고서에 기여점 기록 |

---

## 8. Git 작업 방식

- `main`은 **항상 빌드되는 상태**로 유지
- 작업은 `feature/<이름>-<주제>` 브랜치에서 (예: `feature/ranju-otsu`), PR로 머지
- PR 전에 **Release x64 빌드 + 점검 스크립트** 통과 확인
- 커밋 메시지: `[모듈] 변경 내용` (예: `[quality] add Michelson contrast`)
- 사진 원본은 git에 올리지 않고 팀 드라이브로 공유. 기준 사진(`data/reference/`)만 레포에 포함

---

## 9. 제출 전 체크리스트

### 소스 (11/28)
- [ ] `check_library_usage.ps1` 통과
- [ ] Release x64 클린 빌드 성공
- [ ] 다른 PC에서 압축 해제 → 빌드 → 실행 확인
- [ ] `bin/`, `build/`, OpenCV DLL 포함해서 `Team22_LatteArt_Src.zip` 생성
- [ ] 샘플 기준 사진과 실행 방법(README) 포함

### 보고서 · 발표 (12/1)
- [ ] 보고서 ①~⑥ 항목 모두 포함, 토론 기록 캡처 첨부
- [ ] 라이브러리 사용 내역과 기여점 정리
- [ ] 발표 자료 첫 장에 "Global / Startup team" 명시, 영문 이름 포함
- [ ] 데모 영상 (실행 화면 녹화)
- [ ] `Team22_LatteArt_PPT.zip`, `Team22_LatteArt_Final.zip` 두 곳에 제출

### 발표 (12/3)
- [ ] 12/2 전원 리허설, 누가 뽑혀도 발표 가능하게
