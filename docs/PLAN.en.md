# LatteArt Development Plan

Team 22 · Image Processing Team Project 02 (Global / Startup team)
Written 2026-11-11 · Original proposal: [Team22_LatteArt_Plan.docx](Team22_LatteArt_Plan.docx) · 한국어: [PLAN.md](PLAN.md)

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

### Normalization contract (shared by all modules)
- A normalized cup image is **512 × 512**
- The cup rim has **radius 240 px**, centered in the image
- The pattern's main axis points **up (↑)**
- Until the normalize module is ready, develop with hand-cropped images that follow these rules

### Data passed between modules (`src/core/types.h`)
`NormalizedCup`, `PatternMask`, `LayerResult`, `QualityResult`, `ScoreResult`
→ **Do not change them without team agreement**

### Score bands (from the proposal, already implemented)

| Layer-count error Δn | Score band |
|---|---|
| 0 | 60 – 100 |
| 1 | 30 – 59 |
| 2 or more | 0 – 29 |

Quality weights (draft, to be tuned with data): contrast 30% · shape 25% · texture 20% · position 15% · size 10%

---

## 4. Roles

| Owner | Scope | Folder |
|---|---|---|
| **Jaeku Kang** | Normalization, structure analysis (layer count), scoring | `src/normalize`, `src/structure`, `src/scoring` |
| **Ranju Kim** | The five quality metrics | `src/quality` |
| **Junkyu Kim** | Photo collection, ground-truth labels, blind rating, correlation analysis | `data/` |
| **Cecelia Hein** | Result display, demo video, market research, presentation | `src/viz`, presentation |
| **Everyone** | Split the basic operations in `src/basic` | `src/basic` |

### Splitting `basic/` operations (proposal)
Jaeku has the heaviest load, so the basic operations are shared.

| Operation | Owner (proposed) | Mainly used by |
|---|---|---|
| Sobel / Canny, geometric warp | Jaeku Kang | Normalization |
| Otsu, morphology, Gaussian blur, FFT | Ranju Kim | Segmentation, quality, layer cross-check |
| Lab color conversion, CLAHE | Junkyu Kim | Preprocessing |
| (Pixel drawing helpers for the result image) | Cecelia Hein | Result image |

---

## 5. Schedule

| Dates | Common / Jaeku Kang | Ranju Kim | Junkyu Kim | Cecelia Hein |
|---|---|---|---|---|
| **Nov 11 – 13** | ✅ Repo skeleton, VS solution, shared headers<br>Agree on `types.h` | Basic ops (Otsu, morphology, Gaussian) | Write shooting guide, start collecting photos | Result screen layout draft |
| **Nov 14 – 17** | Cup detection → ellipse fit → homography → PCA rotation | **Size, position, contrast** on hand-normalized images | Ground-truth labels (`data/labels.csv`) | Result image overlay |
| **Nov 18 – 22** | Pattern segmentation, multi-column profile, layer counting, FFT cross-check | **Shape (mirror IoU, Hu), texture (GLCM, LoG)** | Batch run, first layer-count accuracy measurement | Console report, feedback messages |
| **Nov 23 – 26** | Full integration, bug fixes | Tune weights and thresholds | Blind rating, correlation analysis | Save result image, prepare demo |
| **Nov 27 – 28** | **Code freeze**, library check, clean build on another PC, submit Src zip | | | |
| **Nov 29 – Dec 1** | Error analysis, report, slides, demo video → submit PPT / Final zips | | | |
| **Dec 2** | **Full-team rehearsal** | | | |
| **Dec 3** | Final presentation | | | |

### Milestones
- **Nov 17**: normalized image set ready
- **Nov 22**: two photos + pattern in → final score out (quality can still be rough)
- **Nov 26**: tuning done, failure cases collected
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
| **Layer-count accuracy** (core of the project) | Control input quality with a shooting guide (shoot straight down, no shadows, whole cup visible). This helps more than algorithm tweaks |
| Too much work on Jaeku | Basic operations split across all four; FFT owned by Ranju |
| Ellipse fitting / homography are hard to implement | If behind schedule, reduce scope to "ask for a retake when the photo is heavily tilted" (already stated in the proposal) |
| Integration conflicts | Agree on shared headers first, keep `main` always building, feature branches + PRs |
| Build fails on the grader's PC | On Nov 27, unzip on another PC, clean build and run |
| Breaking the library rule | Check script before every merge; record any exception and our contribution in the report |

---

## 8. Git workflow

- Keep `main` **always building**
- Work on `feature/<name>-<topic>` branches (e.g. `feature/ranju-contrast`) and merge through PRs
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
