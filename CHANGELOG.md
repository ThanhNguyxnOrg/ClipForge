# Changelog 📝

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- N/A

### Changed
- N/A

### Fixed
- N/A

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
- 📚 Docs: architecture, audio-format support, assembly design, building guide.

[Unreleased]: https://github.com/ThanhNguyxnOrg/ClipForge/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/ThanhNguyxnOrg/ClipForge/releases/tag/v0.1.0
