# 데모 샘플

동결 코드(`main` 3331b9c, 2026-11-22)로 직접 돌려서 확인한 결과입니다.
레포 루트에서 실행하세요. OpenCV가 한글 경로를 못 읽으므로 경로는 아래처럼 상대 경로로 씁니다.

```
bin\Release\LatteArt.exe <기준 사진> <사용자 사진> <패턴> result.png
```

전부 한 번에 돌리기: `powershell -ExecutionPolicy Bypass -File demo_samples\run_demo.ps1`

`*_result.png`는 각 사례의 결과 이미지입니다(실행하면 다시 만들어집니다).

## 1. 높은 점수: 같은 패턴 (`1_high_score/`)

전문가 사진을 돌리거나 줄이거나 어둡게 바꾼 사진입니다. **원본 사진을 편집한 합성 이미지**이므로 발표에서 그렇게 소개해 주세요.
보여 줄 것: 같은 패턴이면 각도, 크기, 밝기가 달라도 90점대가 나온다.

| 사용자 사진 | 기준 사진 | 패턴 | 최종 점수 | Q |
|---|---|---|---|---|
| Pro_Tulip_3__scale90 (90% 축소) | reference/Pro_Tulip_3 | tulip | 97.8 | 0.98 |
| Pro_Heart_3__rot10 (10° 회전) | reference/Pro_Heart_3 | heart | 96.1 | 0.96 |
| Pro_Tulip_4__scale90 (90% 축소) | reference/Pro_Tulip_4 | tulip | 94.9 | 0.91 |
| Pro_Heart_3__dark (밝기 0.8배) | reference/Pro_Heart_3 | heart | 94.3 | 1.00 |
| Pro_Tulip_3__rot10 (10° 회전) | reference/Pro_Tulip_3 | tulip | 93.3 | 0.83 |

참고: 기준 사진을 자기 자신과 비교하면 100점입니다.

## 2. 층이 뭉개지면 점수가 떨어짐 (`2_layers_merged/`)

Pro_Tulip_4의 거품을 두껍게 만들어 층 사이 크레마 선이 사라진 합성 이미지입니다.
보여 줄 것: 같은 컵이어도 층 구조가 무너지면 구조 점수가 크게 떨어진다.

| 사용자 사진 | 기준 사진 | 패턴 | 최종 점수 | Q |
|---|---|---|---|---|
| Pro_Tulip_4__thick | reference/Pro_Tulip_4 | tulip | 34.7 | 0.74 |

## 3. 연습 사진: 잘 만든 컵 (`3_practice_good/`)

| 사용자 사진 | 기준 사진 | 패턴 | 최종 점수 | Q |
|---|---|---|---|---|
| Tulip_Practice_11 | reference/Pro_Tulip_3 | tulip | 53.3 | 0.71 |
| Tulip_Practice_18 | reference/Pro_Tulip_3 | tulip | 43.7 | 0.51 |
| Heart_Practice_18 | reference/Pro_Heart_3 | heart | 43.5 | 0.54 |
| Tulip_Practice_9 | reference/Pro_Tulip_3 | tulip | 42.6 | 0.65 |
| Heart_Practice_14 | reference/Pro_Heart_3 | heart | 40.2 | 0.62 |

## 4. 연습 사진: 망친 컵 (`4_practice_bad/`)

| 사용자 사진 | 기준 사진 | 패턴 | 최종 점수 | Q |
|---|---|---|---|---|
| Tulip_Practice_6 | reference/Pro_Tulip_1 | tulip | 19.5 | 0.62 |
| Tulip_Practice_8 | reference/Pro_Tulip_1 | tulip | 20.8 | 0.58 |
| Heart_Practice_6 | reference/Pro_Heart_3 | heart | 33.7 | 0.56 |

## 5. 실패해야 하는 사진 (`5_should_fail/`)

점수 대신 오류 메시지가 나오는 것이 정상입니다.

| 사용자 사진 | 기준 사진 | 패턴 | 나오는 메시지 | 이유 |
|---|---|---|---|---|
| no_cup_gray (회색 빈 이미지) | reference/Pro_Heart_3 | heart | `stage 'normalize user': 컵 후보를 찾지 못했습니다.` | 컵이 없음 |
| Tulip_Practice_1 | reference/Pro_Tulip_3 | tulip | `stage 'normalize user': 컵 후보를 찾지 못했습니다.` | 비스듬한 촬영, 복잡한 배경 |
| Heart_Practice_21 | reference/Pro_Heart_3 | heart | `stage 'pattern size validation': 크기가 맞지 않습니다.` | 거품이 컵 대부분을 덮음 |
| Heart_Practice_5 | reference/Pro_Heart_3 | heart | `stage 'pattern size validation': 크기가 맞지 않습니다.` | 패턴이 무너지고 크레마 얼룩이 섞임 |

## 발표 시 주의

- 연습 사진 점수는 대부분 20~55점이고, 잘 만든 컵과 망친 컵의 점수 범위가 겹치는 경우도 있습니다. 이 폴더는 순서가 잘 드러나는 사례만 고른 것입니다. 전체 결과는 보고서의 한계 부분에 함께 쓰는 것이 좋습니다.
- 4번의 튤립 두 장은 Pro_Tulip_1(축이 옆으로 누운 사진)을 기준으로 했습니다. 기준 사진을 바꾸면 점수가 달라집니다.
- 1번과 2번은 원본 사진을 편집한 합성 이미지입니다.
