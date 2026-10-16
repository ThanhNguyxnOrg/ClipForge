# Changelog 📝

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-10-16 ✨

First officially stable release: redesigned Qt app, fuller tests, bot-owned pipeline.

### Added
- 🎨 Full Qt visual restyle (dark DSP theme, styled controls, app window icon).
- 📂 Folder picker dialogs for input/output directories.
- 🖥️ GUI-subsystem link for `clipforge-ui.exe` (no console window) with a CI gate.
- 🧪 Batch service suite, WAV adversarial cases, processing edges, Analyzer vectors.
- 🔧 `tools/make_icon.py` (stdlib-only SVG→ICO generator) + `assets/clipforge.ico`.

## [0.1.1] - 2026-10-16 🖥️

Qt app joins the release assets.

### Added
- 🖥️ Release pipeline now builds the Qt 6 Quick app (`CLIPFORGE_BUILD_QT_UI=ON`),
  packages it with `windeployqt` plus the official `vc_redist`, and attaches
  `ClipForge-<ver>-windows-x64-qt.zip` to every GitHub Release.

## [0.1.0] - 2026-10-15 🎉

First public MVP release.

### Added
- 🎵 Strict RIFF/WAVE reader supporting PCM16, PCM24, IEEE Float32 (mono/stereo),
  arbitrary chunk order, odd-size padding, and `WAVE_FORMAT_EXTENSIBLE`.
- 🔍 `Analyzer`: duration, peak/RMS (linear + dBFS), clipped-sample count,
  leading/trailing silence detection.
- 🎚️ `Normalizer`: peak normalization to a dBFS target with silent-input guard.
- ✂️ `SilenceTrimmer`: frame-accurate leading/trailing trim with head/tail padding
  and minimum-speech guard.
- 🏭 `EngineFacade`: single-file + batch orchestration, collision detection,
  temp-file + atomic-rename writes, `--dry-run`, CSV reports, progress/cancel hooks.
- 💻 `clipforge-cli`: `analyze` / `normalize` / `trim` / `batch` with stable exit codes (0/2/3/4).
- ⚡ Dual DSP backends: portable C++23 reference + Windows x64 NASM SSE2
  (`cf_peak_abs_f32`, `cf_count_clipped_f32`, `cf_apply_gain_clamp_f32`),
  runtime-selectable via `--backend auto|cpp|asm`.
- 🧪 Test suite: WAV I/O, processing safety, C++ vs ASM differential
  (20 sizes × 4 misalignments × 6 thresholds, bit-exact).
- 📈 `clipforge-bench`: warmup + median timing + CSV output.
- 🏗️ CMake 3.25+ build with presets (`windows-x64-release` / `windows-x64-debug`),
  optional NASM auto-detection with clean C++ fallback.
- 📚 Docs: architecture, audio-format support, assembly design, building guide,
  release process, Qt slice-1 notes.
- 🖥️ Qt 6 Quick slice-1 desktop app: batch queue, per-file and overall progress,
  cancel, headless offscreen tests, and a `windeployqt` CI artifact.
- 🧪 Extended coverage: batch service suite (dry-run, collisions, CSV), WAV
  adversarial cases, processing edges, Analyzer vectors, `NaN` rejection.
- 🎨 Light-mode logo variant with drop shadow; README switches via `<picture>`.
- 🤖 Release-gated Qt CI job (release commits, tags, manual dispatch).

[Unreleased]: https://github.com/ThanhNguyxnOrg/ClipForge/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/ThanhNguyxnOrg/ClipForge/releases/tag/v0.2.0
[0.1.1]: https://github.com/ThanhNguyxnOrg/ClipForge/releases/tag/v0.1.1
[0.1.0]: https://github.com/ThanhNguyxnOrg/ClipForge/releases/tag/v0.1.0
