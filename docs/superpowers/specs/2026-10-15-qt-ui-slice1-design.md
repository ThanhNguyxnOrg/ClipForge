# ClipForge Qt UI Slice 1 — Design Spec (2026-10-15)

Status: approved sections 1–3 (visual, architecture, build/CI). Slice 1 = batch queue + progress/cancel. Waveform deferred to slice 2.

## 1. Context and goals

ClipForge is CLI-only today. The Qt 6 Quick desktop app is roadmap Phase 2. Slice 1 delivers the smallest usable GUI: pick a folder, run normalize/trim/both across it, watch per-file and overall progress, cancel mid-run. Success = a CI-built `.exe` the owner downloads and runs on Windows 10/11 with zero local Qt install.

Constraints: no Qt on the dev machine — all Qt compile/test happens on GitHub Actions `windows-latest`. The Qt layer stays thin (QML + `QObject` wrappers); all audio logic stays in `clipforge_core`, already covered by tests. Repo stays 100% English.

## 2. Visual direction (approved)

DSP-lab-bench aesthetic from the repo hero: dark instrument panel, one memorable element (live DSP readout strip), quiet Swiss-grid surfaces elsewhere, SVG icons only (no emoji).

Tokens: Forge Navy `#0B0E1A` (background), Panel `#141826` (cards), Signal Orange `#FF5500→#FF8800` (primary actions/active), Wave Cyan `#00E5FF` (progress/readouts), Bone `#F3F5F8` (primary text), Slate `#8A93A6` (secondary text). Orange reserved for accents/large-bold to hold contrast on navy. UI text: system sans (no bundled font); numeric dBFS/peak readouts: monospace (Cascadia Mono/Consolas), matching the hero DSP monitor.

Layout: single window, three zones — left batch queue (`ListView`), right inspector (selected file stats + per-row progress), bottom global bar (overall progress + Cancel).

## 3. Architecture (approved)

Components:
- `Main.qml` — three-zone single window.
- `BatchQueueModel` — C++ `QAbstractListModel`. Per-row state (status, progress, error) lives in the model, never in delegates (delegates are pooled/reused).
- `EngineBridge` — `QObject` wrapping `EngineFacade::run_batch`. Progress callback emits signals; cancellation via the existing atomic token.
- Worker runs the batch off the UI thread; `QFutureWatcher` bridges progress/finish/cancel signals into QML roles.

Data flow: QML `bridge.startBatch(dir, opts)` → C++ `run_batch` → `fileProgress`/`totalProgress` signals update model roles → on completion the `BatchSummary` maps back into rows. Per-file errors render inline in the row; no blocking modal. CSV report writes exactly like the CLI.

Error handling: `Result<T>` converts to `status`/`errorMessage` roles. Cancel stops after the current file. All `EngineFacade` guarantees (collision guards, temp-file+rename, dry-run) are inherited, not reimplemented.

## 4. Build, CI, testing (approved)

- Qt 6.8.3 via `install-qt-action@v4` (`win64_msvc2022_64` + `qtshadertools`, cache on). New CI job runs after core jobs pass.
- CMake: `CLIPFORGE_BUILD_QT_UI=ON` enables `qt_add_executable` + `qt_add_qml_module` (Qt 6.2+ API, `qt_standard_project_setup(REQUIRES 6.5)`); default OFF so local/MSVC-less builds are unaffected.
- Tests: `qmllint` over all QML (auto target); C++ model tests headless (`-platform offscreen`, `QML_DISABLE_DISK_CACHE=1`); headless app-launch smoke. No real mouse-click tests in slice 1.
- Packaging: `windeployqt --qmldir <qml-src> --release`, ship official `vc_redist.x64.exe`; artifact ZIP uploaded per run for manual download-and-click testing.

## 5. Non-goals (slice 2+)

Waveform preview, drag-and-drop into queue, settings/persistence, auto-update, `.msi` installer.

## 6. File plan

- `apps/qt/Main.qml`, `apps/qt/BatchQueueModel.{hpp,cpp}`, `apps/qt/EngineBridge.{hpp,cpp}` (new).
- `CMakeLists.txt` — Qt block behind `CLIPFORGE_BUILD_QT_UI`.
- `.github/workflows/ci.yml` — Qt job (install, configure, build, lint, offscreen tests, package, upload).
- `docs/building.md`, `README.md` — Qt slice notes.

## 7. Risks

- CI-only iteration: compile errors surface only on runners (~minutes per loop). Mitigation: thin Qt layer, core logic pre-tested, keep first QML minimal.
- QML debugging is blind without local Qt. Mitigation: `qmllint` + offscreen smoke catch syntax/startup faults; visual polish verified from downloaded artifacts.
- `windeployqt` misses QML imports if `--qmldir` is wrong — CI job fails loudly (no silent fallback), artifact is launch-tested headless before upload.
