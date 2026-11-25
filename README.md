# LatteArt — Automated Latte Art Scoring (Team 22)

Image Processing Team Project 02 (Global / Startup team).
Scores a latte art photo against an expert reference photo of the same pattern,
structure first (per-line layer correspondence, spacing and thickness), then quality
(contrast, shape, texture, position, size). No single layer count is assigned to a photo.

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
bin\Release\LatteArt.exe                                                # keyboard viewer (also: double-click, Ctrl+F5, --ui)
bin\Release\LatteArt.exe --ui --snapshot shots                         # save every viewer screen as PNG, no window
bin\Release\LatteArt.exe data\reference\tulip3.jpg data\user\tulip3_junkyu_01.jpg tulip result.png
bin\Release\LatteArt.exe reference.png user.png tulip result.png --no-show  # save without opening a window
bin\Release\LatteArt.exe --canon photo.jpg <cx> <cy> <r> canon.png     # manual normalization to 512x512
bin\Release\LatteArt.exe --debug <stage> [args...]                     # run one stage, save debug_<stage>_*.png
bin\Release\LatteArt.exe --batch data\labels.csv data\user results.csv   # score the whole dataset
```
The console reports each compared line and both directional scores. The result image shows the reference and
aligned user cups above separate vertical/horizontal structure diagrams: green means matched,
red means a missing reference peak, and yellow means an extra user peak.
The optional old `tulip:3` syntax is still accepted; `3` is catalog metadata only.
Cup detection and size failures stop scoring and report the stage and error.
When no original rim candidate passes validation, normalization tries compatible
edge-arc merging and bounded, reproducible RANSAC consensus fitting. The size,
axis ratio, image containment and angular coverage checks remain in force.
Existing successful rim candidates retain priority. This recovers candidate absence;
it does not guarantee a correct liquid boundary for every cup material or reflection.

```powershell
bin\Release\LatteArt.exe --debug normalize photo.png --prefix=data/raw/cup-test
powershell -ExecutionPolicy Bypass -File scripts/test_cup_recovery.ps1
```
The debug command saves the actual resized detection edges, candidate CSV with
rejection reasons, candidate/inlier overlays, and (on success) final boundaries
and normalized cup. Red boundary = rim, green = liquid. Inlier diagnostics use
green for accepted support and yellow for excluded points. Candidate diagnostics
are still saved on detection failure. `--ransac-iterations=0` disables consensus
fitting for comparison; arc merging remains enabled. Recovery tests use the tracked
`Tulip_Practice_10.png`; generated outputs remain under ignored `data/raw/`.

Batch input uses UTF-8 CSV with `file,pattern,reference,notes` columns (`notes` is optional).
User file paths are relative to `<photo_dir>`; reference paths are relative to the
labels CSV directory. Absolute paths and quoted commas/quotes/newlines are supported.
Old `target_layers,true_layers` label files still work: `target_layers` selects
`data/reference/<pattern><target_layers>.jpg` when no reference is given, while
`true_layers` is ignored. Neither value is used as a scoring target.

Batch output consists of `results.csv` (image summaries), `results_lines.csv`
(per-line counts and scores), and `results_matches.csv` (matched/missing/extra peaks).
Join these using `row_id` and `line_index`, not filenames, which may repeat.
Summary missing/extra occurrences count events across lines, not distinct physical layers.
Rejected/error rows have blank scores and no detail rows; remaining rows continue.
The old global `detected_layers` and `layer_correct` output columns are removed.
Each detail row has `direction` and `coordinate`: vertical fixes x and scans
top to bottom; horizontal fixes y and scans left to right. Coordinates, peak
positions and widths are normalized to the relevant pattern dimension.
Image summaries include `vertical_lines`, `horizontal_lines`, `vertical_score`,
`horizontal_score`, `vertical_structure` and `horizontal_structure`.

Final score = (mean vertical line score + mean horizontal line score) / 2.
The directions always have equal weight, even with different numbers of valid lines.
Each line retains the count-error band and 30% spacing/thickness U, 70% quality Q
formula. Rotation search also uses the equal directional mean of structure-only
scores; quality is evaluated once at the selected rotation. Both directions are required.

After building, run the end-to-end checks with two actual photos:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/test_multiline_integration.ps1 -Reference reference.png -UserPhoto user.png
```
This test expects the photos to pass the size check. It also exercises synthetic
size rejection and no-cup cases; all artifacts are written under ignored `data/raw/`.

## Owners

Full plan, file ownership rules and per-person schedule: **[docs/PLAN.en.md](docs/PLAN.en.md)** (한국어: [docs/PLAN.kr.md](docs/PLAN.kr.md)).
The first line of every source file says `// Owner: <name>` — edit only your own files.

| Owner | Files |
|---|---|
| Jaeku Kang | `basic/edge`, `basic/geometry`, `normalize/*`, `structure/layers`, `scoring/*`, `app/pipeline`, `debug/normalize.cpp`, `debug/layers.cpp` |
| Ranju Kim | `basic/threshold`, `basic/morphology`, `basic/fft`, `structure/segment`, `quality/*`, `debug/segment.cpp`, `debug/quality.cpp` |
| Junkyu Kim | `basic/filter`, `basic/color`, `basic/clahe`, `viz/report`, `debug/preprocess.cpp` |
| Cecelia Hein | `viz/draw`, `debug/viz.cpp`, `app/batch`, `data/` (photos, labels, ratings), `docs/` (meetings, report, slides, demo video) |
| SHARED (PR approved by all) | `core/*`, `io/*`, `app/main.cpp`, `debug/debug.h`, `LatteArt.vcxproj(.filters)`, `opencv.props` |

### Normalized frame (contract)
Every `NormalizedCup` is 512×512, with the detected liquid surface mapped to a
centered radius of 240 px. The reference should be upright; the pipeline searches
the user's rotation against it. Structural comparison independently normalizes
pattern width/height; quality evaluation retains the original cup-relative size/position.
`core/types.h` exposes `StructureComparison.lines`; counts are the sizes of each
line's reference/user peak lists. Correspondence indices preserve skipped layers.

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
