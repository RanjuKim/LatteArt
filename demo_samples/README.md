# 데모 샘플

발표 시연에 쓰는 사례 7개입니다. 점수는 `main` e3eaec9(2026-11-25) 코드로 직접 돌려서 확인한 값입니다.
레포 루트에서 실행하세요. 한글이 들어간 폴더의 사진도 읽을 수 있습니다(파일을 직접 읽어 `cv::imdecode`로 변환).

## 실행 방법

**화면으로 보기:** `bin\Release\LatteArt.exe`를 인자 없이 실행(더블클릭)합니다. 메뉴에서 W/S로 고르고 Enter로 채점합니다.
결과 화면에서는 1/2/3(또는 Tab)으로 요약 / 구조 / 품질을 바꾸고, B로 메뉴, Q로 종료합니다.
O를 누르면 Windows 파일 선택 창으로 직접 사진을 고를 수 있습니다.

**명령줄로 보기:**
```
bin\Release\LatteArt.exe <기준 사진> <사용자 사진> <패턴> result.png
```
전부 한 번에 돌리기: `powershell -ExecutionPolicy Bypass -File demo_samples\run_demo.ps1`

`*_result.png`는 각 사례의 결과 이미지입니다(실행하면 다시 만들어집니다).

## 사례 목록

| 메뉴 번호 | 사용자 사진 | 기준 사진 | 패턴 | 결과 | Q |
|---|---|---|---|---|---|
| 1 | `1_high_score/Pro_Tulip_3__scale90` (90% 축소) | Pro_Tulip_3 | tulip | **98.1** | 0.98 |
| 2 | `1_high_score/Pro_Heart_3__dark` (밝기 0.8배) | Pro_Heart_3 | heart | **94.6** | 1.00 |
| 3 | `3_practice_good/Tulip_Practice_11` (잘 만든 연습 컵) | Pro_Tulip_3 | tulip | **54.8** | 0.71 |
| 4 | `4_practice_bad/Tulip_Practice_6` (망친 연습 컵) | Pro_Tulip_1 | tulip | **21.0** | 0.62 |
| 5 | `4_practice_bad/Heart_Practice_6` (망친 연습 컵) | Pro_Heart_3 | heart | **34.5** | 0.56 |
| 6 | `5_should_fail/no_cup_gray` (회색 빈 이미지) | Pro_Heart_3 | heart | 오류: 컵을 찾지 못함 | - |
| 7 | `5_should_fail/Heart_Practice_21` (거품이 컵 대부분을 덮음) | Pro_Heart_3 | heart | 오류: 패턴 크기가 맞지 않음 | - |

기준 사진은 모두 `reference/` 폴더에 있습니다. 기준 사진을 자기 자신과 비교하면 100점입니다.

### 사례별로 보여 줄 것
- **1, 2 (같은 작품):** 전문가 사진을 줄이거나 어둡게 바꾼 **합성 이미지**입니다. 크기나 밝기가 달라도 같은 패턴이면 90점대가 나온다는 것을 보여 줍니다. 발표에서 합성 이미지라고 소개해 주세요.
- **3 (잘 만든 연습 컵) vs 4, 5 (망친 컵):** 실제 연습 사진입니다. 층 구조가 기준과 많이 다를수록 점수가 크게 떨어집니다.
- **6, 7 (실패 사례):** 점수 대신 이유가 나오는 것이 정상입니다. 명령줄에서는 각각 `stage 'normalize user': 컵 후보를 찾지 못했습니다.`, `stage 'pattern size validation': 크기가 맞지 않습니다.`가 나옵니다.

## 발표 시 주의
- 연습 사진 점수는 대부분 20~55점이고, 잘 만든 컵과 망친 컵의 점수 범위가 겹치는 경우도 있습니다. 이 폴더는 차이가 잘 드러나는 사례만 고른 것이므로, 전체 결과는 보고서의 한계 부분에 함께 쓰는 것이 좋습니다.
- 4번 튤립은 Pro_Tulip_1(축이 옆으로 누운 사진)을 기준으로 했습니다. 기준 사진을 바꾸면 점수가 달라집니다.
- 원본 연습 사진 전체는 `pictures/` 폴더에 그대로 있습니다.
