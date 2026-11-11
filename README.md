# LatteArt — Automated Latte Art Scoring (Team 22)

Image Processing Team Project 02 (Global / Startup team).
Scores a latte art photo against an expert reference photo of the same pattern,
structure first (layer count), then quality (contrast, shape, texture, position, size).

## Build environment (everyone must match)

| Item | Version |
|---|---|
| IDE | **Visual Studio 2022** (toolset **v143**). VS 2026 also works if the v143 build tools are installed. |
| Platform | **x64** (official OpenCV Windows builds are x64 only) |
| Language | C++17 |
| OpenCV | 4.12.0 prebuilt Windows package (I/O functions only) |

> If VS 2026 asks to "retarget" the project to v145, choose **No / cancel**.
> The project must stay on v143 so it builds on VS 2022.

### One-time setup
1. **Visual Studio Installer** → *Desktop development with C++* workload.
   If you use VS 2026: *Individual components* → **MSVC v143 – VS 2022 C++ x64/x86 build tools**.
2. Download the OpenCV Windows package from https://opencv.org/releases/ and extract it, e.g. to `C:\opencv`.
3. Set user environment variables (then restart Visual Studio):
   ```
   OPENCV_DIR = C:\opencv\build
   OPENCV_VER = 4120        # 4.12.0 -> 4120, 4.10.0 -> 4100
   OPENCV_VC  = vc16        # folder name under C:\opencv\build\x64\
   ```
   PowerShell: `[Environment]::SetEnvironmentVariable('OPENCV_DIR','C:\opencv\build','User')`
4. Open `LatteArt.sln`, select **x64**, build. `opencv_world*.dll` is copied next to `bin\<Config>\LatteArt.exe` automatically.

## Run
```
bin\Release\LatteArt.exe data\reference\tulip3.jpg data\user\tulip3_junkyu_01.jpg tulip:3 result.png
```
Unimplemented steps throw `TODO: ...` and the program prints which stage stopped.

## Layout and owners

| Folder | Contents | Owner |
|---|---|---|
| `src/core` | Shared structs (`types.h`) — change only after team agreement | all |
| `src/io` | The **only** place that calls OpenCV (`imread/imwrite/imshow/waitKey`) | — |
| `src/basic` | Own implementations: color, Gaussian, CLAHE, Sobel/Canny, Otsu, morphology, FFT, warp | split among all |
| `src/normalize` | Cup rim, ellipse fit, perspective/scale/rotation normalization, PCA axis | Jaeku |
| `src/structure` | Pattern segmentation, axis profile, layer counting, FFT cross-check | Jaeku |
| `src/scoring` | Banded score `S = L + (H − L)(0.3U + 0.7Q)` | Jaeku |
| `src/quality` | Contrast, shape, texture, position, size | Ranju |
| `src/viz` | Console report, result image | Cecelia |
| `data/` | Reference photos, labels (`labels.csv`); practice photos stay on the team drive | Junkyu |
| `docs/meetings` | KakaoTalk captures / meeting photos for the final report | all |

### Normalized frame (contract)
Every `NormalizedCup` is 512×512, cup rim radius 240 px centered, pattern main axis pointing **up**.
Until normalization is ready, quality/structure work can use hand-cropped images in this frame.

## Library rule
Only OpenCV I/O is allowed (assignment spec). Use `cv::Mat` as a container only
(`ptr<T>`, `at<T>`, `rows`, `cols`, `clone`). No `cv::` processing functions, no Mat arithmetic operators.
Check before every merge:
```
powershell -ExecutionPolicy Bypass -File scripts\check_library_usage.ps1
```

## Git workflow
- `main` must always build. Work on `feature/<name>-<topic>` branches and merge with a PR.
- Commit message: `[module] what changed`, e.g. `[quality] add Michelson contrast`.

## Deadlines
| Date | Deliverable |
|---|---|
| Sat Nov 28, 22:00 | `Team22_LatteArt_Src.zip` — full VS solution **including compiler output** (`bin/`, `build/`) and the OpenCV DLL |
| Tue Dec 1, 22:00 | `Team22_LatteArt_PPT.zip` and `Team22_LatteArt_Final.zip` — report (Word), slides, demo video |
| Thu Dec 3 | Final presentation — presenter chosen randomly, everyone prepares |
