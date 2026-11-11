# LatteArt Development Plan

Team 22 · Image Processing Team Project 02 (Global / Startup team)
Written 2026-11-11 (roles and schedule revised) · Original proposal: [Team22_LatteArt_Plan.docx](Team22_LatteArt_Plan.docx) · 한국어: [PLAN.kr.md](PLAN.kr.md)

---

## 1. Assignment requirements

| Item | Details |
|---|---|
| Language / environment | C++, **must compile in Visual Studio** |
| Libraries | **Only OpenCV input/output functions** (`imread`, `imwrite`, `imshow`, `waitKey`). Every other algorithm is implemented by us |
| Exception (FAQ Q1) | Library functions are allowed for supporting steps that are not the main idea of the project, but the **report must list them and state our contribution** |
| UI | A GUI is not required; a console program is fine |
| Bonus | Global team +5 points, extra points for a startup plan. **State "Global / Startup team" on the first slide** |
| Presentation | The presenter is chosen at random, so **everyone must be ready to present** |

### Deliverables and deadlines

| Deadline | Deliverable | Where |
|---|---|---|
| **Sat Nov 28, 22:00** | `Team22_LatteArt_Src.zip`: the whole VS solution + **every file the compiler produced** (`bin/`, `build/`, OpenCV DLL) | e-Class / Team project 02 - SRC |
| **Tue Dec 1, 22:00** | `Team22_LatteArt_PPT.zip`: report (Word), slides, demo video (**English names included**) | e-Class / Team project 02 - PPT |
| **Tue Dec 1, 22:00** | `Team22_LatteArt_Final.zip`: same contents | e-Class / Team Project / Final |
| **Thu Dec 3** | Final presentation. It may continue to Dec 10, so check our slot on e-Class | In class |

### Required sections of the final report (Word)
1. Outline of the program
2. Design of the program (module structure, each algorithm)
3. Project timeline (use the git commit history)
4. **Group discussion process** (KakaoTalk captures, meeting photos, etc.) → keep collecting them in `docs/meetings/`
5. Team's thoughts on the project
6. Etc. (library usage, limitations, business plan)

---

## 2. Development environment (everyone the same)

| Item | Setting |
|---|---|
| IDE | **Visual Studio 2022, toolset v143** (per course material). If you use VS 2026, install the v143 build tools and **cancel** the retarget prompt |
| Platform | **x64** (official OpenCV Windows builds are x64 only) |
| OpenCV | **4.12.0** Windows package, extracted to `C:\opencv` |
| Environment variables | `OPENCV_DIR=C:\opencv\build`, `OPENCV_VER=4120`, `OPENCV_VC=vc16` |

Setup steps: see "One-time setup" in [README.md](../README.md).

### Coding rules
- Use `cv::Mat` **only as a container**: construction, `ptr<T>`, `at<T>`, `rows`, `cols`, `clone`
- `cv::` processing functions (`cvtColor`, `GaussianBlur`, `resize`, `threshold`, `Canny`, `line`, `putText`, ...) and Mat arithmetic operators are **forbidden**
- OpenCV I/O calls only inside `src/io/`
- Always run the check script before merging:
  ```
  powershell -ExecutionPolicy Bypass -File scripts\check_library_usage.ps1
  ```

---

## 3. Program structure

```
Two photos (reference, user) + target pattern (e.g. tulip:3)
   │
   ├─ normalize   find cup rim → ellipse fit → tilt correction → scale → rotate pattern axis to vertical
   ├─ structure   segment pattern → brightness profile along the axis → count layers → FFT cross-check → uniformity U
   ├─ quality     contrast · shape · texture · position · size → quality score Q
   ├─ scoring     pick score band from layer-count error → S = L + (H − L)(0.3U + 0.7Q)
   └─ viz         console report + save/show result image
```

### Run modes

| Command | Purpose |
|---|---|
| `LatteArt.exe <ref.jpg> <mine.jpg> tulip:3 [result.png]` | Scoring (the final program) |
| `LatteArt.exe --canon <photo> <cx> <cy> <r> <out.png>` | **Manual normalization** (done). Read the cup center and radius in Paint and get a 512 canonical image |
| `LatteArt.exe --debug <stage> ...` | **Per-stage debug mode.** Runs one stage and saves intermediate results as `debug_<stage>_*.png`. Stages: `preprocess` (Junkyu), `normalize` · `layers` (Jaeku), `segment` · `quality` (Ranju), `viz` (Junkyu). Each `src/debug/<stage>.cpp` is edited only by that stage's owner. Also used to show intermediate results in the report and demo |
| `LatteArt.exe --batch ...` | Score the whole dataset → CSV (Junkyu) |

### Normalization contract (shared by all modules)
- A normalized cup image is **512 × 512**
- The cup rim has **radius 240 px**, centered in the image
- The pattern's main axis points **up (↑)**
- Until automatic normalization is ready, develop with images made by **`--canon`**

### Score bands (from the proposal, already implemented)

| Layer-count error Δn | Score band |
|---|---|
| 0 | 60 – 100 |
| 1 | 30 – 59 |
| 2 or more | 0 – 29 |

Quality weights (draft, to be tuned with data): contrast 30% · shape 25% · texture 20% · position 15% · size 10%

---

## 4. Roles (designed to avoid merge conflicts)

### Conflict rules
1. **File-level ownership.** The first line of every source file says `// Owner: <name>`. **Edit only your own files.**
2. **SHARED files need a separate PR approved by everyone:** `core/types.h`, `core/todo.h`, `io/`, `app/main.cpp`, `debug/debug.h`, `LatteArt.vcxproj(.filters)`, `opencv.props`
3. **Test in your stage's debug file.** Run it with `--debug <stage>`; never edit `main.cpp` for testing.
4. **Need someone else's function? Ask the owner** (KakaoTalk or a GitHub Issue) instead of editing it. While you wait, write a temporary version inside your own debug file.
5. **Every file we need already exists and is registered in the project.** Avoid adding files; if you must, open a small PR that only touches the vcxproj.
6. Structs needed only by your module go in **your own header**, not `types.h`.

### File ownership

| Owner | Role | Files |
|---|---|---|
| **Jaeku Kang** | Normalization · layer count · scoring · wiring the pipeline | `basic/edge`, `basic/geometry`, `normalize/*`, `structure/layers`, `scoring/*`, `app/pipeline`, `debug/normalize.cpp`, `debug/layers.cpp` |
| **Ranju Kim** | Pattern segmentation · five quality metrics · FFT | `basic/threshold`, `basic/morphology`, `basic/fft`, `structure/segment`, `quality/*`, `debug/segment.cpp`, `debug/quality.cpp` |
| **Junkyu Kim** | Basic filters · color conversion · result output · batch validation | `basic/filter`, `basic/color`, `basic/clahe`, `viz/*`, `app/batch`, `debug/preprocess.cpp`, `debug/viz.cpp` |
| **Cecelia Hein** | Data collection · rating sessions · documents · presentation | `data/`, `docs/` |

### Cecelia Hein's tasks
- **Get the reference photos**: expert heart and tulip
- **Collect about 10 practice photos** (mix of heart and tulip, including cups that hit and miss the target layer count) and record the **true layer count** of each in `data/labels.csv`
- **Make `--canon` images**: read the cup center (cx, cy) and radius (r) in Paint and run the exe
- **Run the blind rating**: recruit 3 raters, collect scores
- **Meeting records**: KakaoTalk captures and meeting photos in `docs/meetings/` (report section 4)
- Update **market and competitor research**, business plan
- **Assemble the report** (sections 1, 3, 4, 5, 6), **slides**, shoot and edit the **demo video**, English proofreading, run the rehearsal

---

## 5. Schedule

### Hand-off dates (if one slips, someone else is blocked)

| Date | Deliverable | From → To |
|---|---|---|
| **Nov 13** | `gaussianBlur`, `convolve2D` | Junkyu → Jaeku (Canny), Ranju (segmentation, LoG) |
| **Nov 14** | 2 reference + about 10 practice photos (originals, `--canon` images, true layer counts) | Cecelia → the three developers |
| **Nov 15** | `segmentPattern` | Ranju → Jaeku (pattern-axis PCA) |
| **Nov 17** | `fft`, `magnitudeSpectrum` | Ranju → Jaeku (layer cross-check) |
| **Nov 17** | `normalizeCup` | Jaeku → everyone |
| **Nov 22** | `analyzeLayers` | Jaeku → Junkyu (result image) |
| **Nov 24** | Batch result CSV | Junkyu → Jaeku, Ranju (threshold check) |
| **Nov 21** | Human rating scores (10 photos) | Cecelia → Junkyu (comparison) |

### Jaeku Kang

| Dates | Tasks | Done when |
|---|---|---|
| Nov 12 – 14 | `sobel`, `canny`, `warpPerspective` (temporary blur in `debug/normalize.cpp` until Gaussian arrives) | Edge map of a real photo saved |
| Nov 15 – 17 | `findRimPoints`, `fitEllipse`, `normalizeCup` (ellipse → circle, scale), `patternAxisAngleDeg` | 10 photos normalized to 512 automatically |
| Nov 18 – 21 | `axisProfile`, `findPeaks`, spacing/thickness CV, FFT cross-check, U | Target layer count detected on reference photos |
| Nov 22 | Wire `pipeline` end to end | Two photos → final score printed |
| Nov 23 – 26 | Check prominence and other thresholds against batch results, fix bugs | Layer-count accuracy on the 10 photos checked |

### Ranju Kim

| Dates | Tasks | Done when |
|---|---|---|
| Nov 12 – 14 | `otsuThreshold`, `binarize`, `erode`/`dilate`, `largestComponent` | Binarized result saved via `--debug segment` |
| Nov 15 | `segmentPattern` → hand to Jaeku | Pattern mask saved from a `--canon` image |
| Nov 15 – 17 | `fft`, `magnitudeSpectrum` (→ Jaeku), `evalSize`, `evalPosition` | Size and position scores printed |
| Nov 18 – 20 | `fft2Magnitude`, `evalContrast` (Michelson, edge width, high-frequency ratio) | Contrast score printed |
| Nov 20 – 22 | `evalShape` (mirror IoU, Hu), `evalTexture` (GLCM, LoG, highlights) | All 5 item scores + feedback sentences |
| Nov 23 – 26 | Sanity-check σ and weights (little data, so no big changes), polish feedback sentences | Ranking agrees in direction with human ratings |

### Junkyu Kim

| Dates | Tasks | Done when |
|---|---|---|
| **Nov 12 – 13** | ★ **`gaussianBlur`, `convolve2D` first** → merge to main right away | Others can use them |
| Nov 14 – 17 | `bgrToLab`, `clahe`, `drawLine`/`drawCircle`/`drawCross`/`drawRect` | Shapes drawn on the `--debug viz` image |
| Nov 18 – 20 | `drawText` (5×7 bitmap font), `sideBySide`, polish `printReport` | Score text shown on the image |
| Nov 21 – 22 | `renderResult` (axis, layer positions, problem areas) | Result image saved |
| Nov 22 – 24 | `runBatch` (labels.csv → result CSV) | Whole dataset scored in one run |
| Nov 25 – 26 | Compare with human ratings (ranking of the 10 photos, Spearman as a reference value), layer-count accuracy table | Numbers for the report |

### Cecelia Hein

| Dates | Tasks | Done when |
|---|---|---|
| Nov 12 – 14 | Reference photos (heart, tulip), **collect about 10 practice photos**, record true layer counts, `--canon` images | **Handed over Nov 14** |
| Nov 15 – 19 | Retake photos that are blurry or too tilted, get 2–3 extra photos for the live demo | By Nov 19 |
| Nov 15 – 21 | Blind rating (3 raters, 10 photos) | **Handed over Nov 21** |
| Nov 23 – 28 | Report draft (sections 1, 3, 4, 5, 6), slide draft, market research update | Drafts shared |
| Nov 29 – Dec 1 | Shoot and edit demo video, assemble and submit report and slides | Both zips submitted |
| Ongoing | Meeting records and chat captures | `docs/meetings/` |

### Everyone

| Date | Task |
|---|---|
| **Nov 13** | Final agreement on the `types.h` structs and normalization rules |
| **Nov 27 – 28** | **Code freeze**, library check, clean build on another PC, submit Src zip (led by Jaeku) |
| Nov 29 – Dec 1 | The three developers write report section 2 (design) and the error analysis → hand to Cecelia |
| **Dec 2** | Full-team rehearsal |
| **Dec 3** | Final presentation |

### Milestones
- **Nov 17**: automatic normalization done
- **Nov 22**: two photos + pattern in → final score and result image out
- **Nov 26**: threshold check done, failure cases collected
- **Nov 27**: first version of the Src zip prepared in advance

---

## 6. Quality metrics in detail (Ranju Kim)

| Item | Weight | Measurement | File |
|---|---|---|---|
| Size | 10% | Pattern area ÷ cup area, compared with the reference | `position_size.cpp` |
| Position | 15% | Distance between pattern centroid and cup center ÷ radius, axis tilt | `position_size.cpp` |
| Contrast | 30% | Michelson contrast, edge width, high-frequency ratio | `contrast.cpp` |
| Shape | 25% | Mirror IoU (symmetry), Hu moment distance | `shape.cpp` |
| Texture | 20% | GLCM homogeneity, LoG bubble detection, highlight ratio | `texture.cpp` |

- Each metric is scored 0–1 by its **difference from the reference photo**, not its absolute value: `score = exp(-|user − ref| / σ)` (`closeness()` is implemented). σ is tuned on data
- Each metric also returns **a sentence explaining the deduction** → "explainable scoring" is our key differentiator
- Implementation order: size → position → contrast → shape → texture (easiest first)
- Use synthetic images (perfect circle, symmetric heart, noisy surface) to check that each metric moves in the expected direction

---

## 7. Risks and mitigation

| Risk | Mitigation |
|---|---|
| **Layer-count accuracy** (core of the project) | When choosing photos, prefer ones shot straight down, without shadows, with the whole cup visible. This helps more than algorithm tweaks |
| Gaussian (Nov 13) is late, blocking Canny and segmentation | Junkyu's top priority. If late, use a temporary 3×3 box filter inside your debug file |
| Automatic normalization (Nov 17) is late | Quality and result-image work continue on `--canon` images |
| Ellipse fitting / homography are hard to implement | If behind schedule, reduce scope to "ask for a retake when the photo is heavily tilted" (already stated in the proposal) |
| Photos or ratings arrive late | Check Cecelia's hand-off dates (Nov 14, 21) daily in the group chat |
| Only about 10 photos | Keep the draft weights from the proposal, no big changes. Make sure some of the 10 cups miss the target layer count. State the sample size under "limitations" in the report |
| Integration conflicts | File-level ownership, SHARED files need everyone's approval, keep `main` always building |
| Build fails on the grader's PC | On Nov 27, unzip on another PC, clean build and run |
| Breaking the library rule | Check script before every merge; record any exception and our contribution in the report |

---

## 8. Git workflow

- Keep `main` **always building**
- Work on `feature/<name>-<topic>` branches (e.g. `feature/ranju-otsu`) and merge through PRs
- Before a PR, make sure the **Release x64 build and the check script** pass
- Commit message: `[module] what changed` (e.g. `[quality] add Michelson contrast`)
- Original photos are shared on the team drive, not in git. Only reference photos (`data/reference/`) live in the repo

---

## 9. Pre-submission checklist

### Source (Nov 28)
- [ ] `check_library_usage.ps1` passes
- [ ] Release x64 clean build succeeds
- [ ] Unzip on another PC → build → run works
- [ ] Create `Team22_LatteArt_Src.zip` including `bin/`, `build/` and the OpenCV DLL
- [ ] Include sample reference photos and run instructions (README)

### Report and presentation (Dec 1)
- [ ] Report covers sections 1–6, with discussion-record captures attached
- [ ] Library usage and our contribution documented
- [ ] First slide says "Global / Startup team", English names included
- [ ] Demo video (screen recording of a run)
- [ ] Submit `Team22_LatteArt_PPT.zip` and `Team22_LatteArt_Final.zip` to both locations

### Presentation (Dec 3)
- [ ] Full-team rehearsal on Dec 2 so anyone can present
