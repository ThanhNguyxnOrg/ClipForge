<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/clipforge-logo.svg">
  <img src="assets/clipforge-logo-light.svg" width="128" alt="ClipForge logo">
</picture>

# 🎬 ClipForge

### Offline Audio Inspection & Batch-Processing Toolkit
#### C++23 • NASM x86-64 SSE2 • CMake • CLI • Tests • Benchmarks

![C++23](https://img.shields.io/badge/C++-23-blue?style=for-the-badge&logo=cplusplus&logoColor=white)
![Windows x64](https://img.shields.io/badge/Windows-x64-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.25+-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![NASM](https://img.shields.io/badge/NASM-SSE2-orange?style=for-the-badge&logo=nasm&logoColor=white)
![License MIT](https://img.shields.io/badge/License-MIT-green?style=for-the-badge&logo=opensourceinitiative&logoColor=white)
![Tests](https://img.shields.io/badge/Tests-passing-brightgreen?style=for-the-badge&logo=checkmarx&logoColor=white)
![Platform](https://img.shields.io/badge/Offline-100%25-blueviolet?style=for-the-badge&logo=lock&logoColor=white)

> 🎙️ Inspect 🔊, normalize 🎚️, and trim silence ✂️ across podcast, interview, lecture, and voice-note recordings — **100% offline** 🔒. No cloud uploads, ever.

[🚀 Quick Start](#-quick-start) • [✨ Features](#-features) • [📖 CLI Reference](#-cli-reference) • [🏗️ Architecture](#️-architecture) • [⚡ Benchmarks](#-benchmarks) • [🧪 Testing](#-testing) • [🤝 Contributing](CONTRIBUTING.md)

</div>

---

## 📋 Table of Contents

- [🎯 Why ClipForge?](#-why-clipforge)
- [✨ Features](#-features)
- [🚀 Quick Start](#-quick-start)
- [💻 CLI Reference](#-cli-reference)
- [🧪 Testing](#-testing)
- [⚡ Benchmarks](#-benchmarks)
- [🏗️ Architecture](#️-architecture)
- [📚 Documentation](#-documentation)
- [🗺️ Roadmap](#️-roadmap)
- [🤝 Contributing](#-contributing)
- [🔒 Security](#-security)
- [📄 License](#-license)

---

## 🎯 Why ClipForge?

| 😩 Problem | ✨ ClipForge Solution |
|---|---|
| 🎤 Ten recordings, ten different loudness levels | 🎚️ `normalize --target-dbfs -1.0` — uniform peak, every file |
| 😴 Dead air at the start/end of every take | ✂️ `trim --threshold-dbfs -45` — frame-accurate silence trimming |
| 🔥 Clipping goes unnoticed until publishing | 🔍 `analyze` — peak, RMS, clipped-sample count, silence map |
| ☁️ Voice data must not leave the machine | 🔒 Fully offline batch processing with CSV audit reports 📊 |
| 🐌 DSP hot loops need to be fast | ⚡ Dual **C++23 ↔ NASM SSE2** backends, `--backend auto\|cpp\|asm` |

---

## ✨ Features

### 🎵 Strict RIFF/WAVE Parser & Safe Writer 🛡️
- ✅ **PCM Signed 16-bit** 🟦, **PCM Signed 24-bit** 🟨, **IEEE Float 32-bit** 🟥 — mono & stereo
- 🧩 Tolerates real-world chunks (`JUNK`, `LIST`, `bext`, `data`-before-`fmt `), odd-size padding bytes
- 🧬 Supports standard `WAVE_FORMAT_EXTENSIBLE` GUIDs
- 🚫 Rejects truncated files, inconsistent `blockAlign`/`byteRate`, and non-finite (`NaN`/`Infinity`) samples
- 💾 Crash-safe writes: **temp file + atomic rename**, source-overwrite guard (`PathCollision`), no-overwrite-by-default (`TargetExists`)

### 🔍 Audio Analysis 📊
`clipforge-cli analyze input.wav`
- ⏱️ Duration, 🔢 sample rate, 🔊 channels, 💽 format & bit depth
- 📈 Peak (linear + dBFS), 📉 RMS (dBFS), 🚨 clipped-sample count
- 🤫 Leading / trailing silence in seconds (frame-accurate)

### 🎚️ Peak Normalization 🎯
`normalize --target-dbfs -1.0`
- 🧮 Gain = target / input-peak with ceiling clamp (no overshoot)
- 🤐 Silent-input guard — never divides by zero

### ✂️ Frame-Accurate Silence Trimming
`trim --threshold-dbfs -45 --head-ms 0 --tail-ms 100`
- 🖼️ Per-frame activity detection (`max(|L|, |R|)`), robust to single-channel-only audio
- 🛋️ Configurable head/tail padding (`ms`) and minimum-speech guard (`--min-audio-ms`)
- 🍼 All-silent input yields a 1-frame silent buffer — downstream writers never see a 0-byte chunk

### 📦 Safe Batch Processing 🏭
`batch <input_dir> --operation normalize|trim|both --output-dir <out> --report summary.csv`
- 📁 `--recursive` with relative-tree preservation
- 👀 `--dry-run` for safe previews
- 🚧 Collision detection (input == output, duplicate targets, existing files)
- 📊 Machine-readable **CSV** report + progress callback + cooperative cancel token (surfaced in the Qt slice-1 app 🖥️)

### ⚡ Dual DSP Backend: C++23 ↔ NASM SSE2 🥊
| Kernel | C++23 reference | NASM Win64 SSE2 |
|---|---|---|
| `cf_peak_abs_f32` | `src/dsp/cpp/kernels_cpp.cpp` | `movups` / `maxps` + horizontal reduction |
| `cf_count_clipped_f32` | portable loop | `cmpnltps` / `psubd`, 0x3FFFFFFF-vector chunking (no uint32 overflow) |
| `cf_apply_gain_clamp_f32` | `clamp` | `mulps` / `maxps` / `minps` |
- 🔀 `resolve_backend(Auto)` → ASM when compiled in, otherwise C++
- 📏 Alignment-free (`movups`), 1–3 sample scalar tails, nullptr-safe 🛡️

---

## 🚀 Quick Start

### ✅ Prerequisites
| Tool | Version | Notes |
|---|---|---|
| 🪟 Windows | 10 / 11 x64 | Primary target |
| 🧰 MSVC | VS 2022 19.38+ | C++23 (`/std:c++latest`) + Ninja — full build |
| 🔧 CMake | 3.25+ | Presets in `CMakePresets.json` |
| 🐝 NASM | 2.16+ | Optional — auto-fallback to C++ backend |
| 🐧 g++ | 13+ (tested 16.1 MSYS2) | VSCode quick build, no CMake needed |

### 🅰️ Option A — VSCode + g++ (fastest, no CMake)
This repo ships ready-to-use VSCode tasks (`.vscode/tasks.json`):

```powershell
# Open D:\Code\clipforge in VSCode, then:
Ctrl+Shift+B          # 👷 build: tests (g++) — default task
# or: Terminal > Run Task > test: run all 🧪
```

Artifacts land in `build/vscode/`:
- 🎯 `clipforge-tests.exe`
- 💻 `clipforge-cli.exe`
- 📈 `clipforge-bench.exe`

### 🅱️ Option B — Full CMake build (MSVC + NASM)
```powershell
# One-time MSYS2 install (UCRT64):
C:\msys64\usr\bin\pacman.exe -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-nasm

# Open "x64 Native Tools Command Prompt for VS 2022", then:
cmake --preset windows-x64-release        # ⚙️ configure
cmake --build --preset windows-x64-release   # 🔨 build core + CLI + tests + bench
ctest --preset windows-x64-release        # 🧪 run all tests
.\build\release\clipforge-bench.exe --csv benchmark_results.csv  # ⚡ benchmark
```

---

## 💻 CLI Reference

> ⚠️ `clipforge-cli.exe` is a command-line tool: **open a terminal first** (Win+R → `powershell`, then `cd` to the folder with the `.exe`), paste a command below, and press Enter. Double-clicking it just flashes a window that closes immediately — that is normal, not a bug. Want a window you can click? Use the Qt app instead ([Qt Desktop App](#-qt-desktop-app-slice-1)).

```powershell
# 🔍 Inspect a single file
clipforge-cli.exe analyze input.wav [--backend auto|cpp|asm]

# 🎚️ Normalize to -1.0 dBFS peak
clipforge-cli.exe normalize input.wav --target-dbfs -1.0 --output output.wav [--backend auto|cpp|asm]

# ✂️ Trim silence below -45 dBFS, keep 100 ms tail
clipforge-cli.exe trim input.wav --threshold-dbfs -45 --tail-ms 100 --head-ms 0 --min-audio-ms 0 --output trimmed.wav

# 🏭 Batch a directory (preview first 👀)
clipforge-cli.exe batch "D:\Recordings" --operation normalize --target-dbfs -1.0 --output-dir "D:\Processed" --report summary.csv --dry-run

# 🏭 Real batch: normalize + trim, recursive
clipforge-cli.exe batch "D:\Recordings" --operation both --target-dbfs -1.0 --threshold-dbfs -45 --output-dir "D:\Processed" --report summary.csv --recursive
```

### 🚦 Exit Codes
| Code | Meaning |
|---|---|
| `0` | ✅ Success |
| `2` | ⚠️ Usage error (unknown command, missing/invalid flag) |
| `3` | ❌ Processing error (`IoError`, `PathCollision`, `InvalidParameter`, …) |
| `4` | ⚠️ Batch completed with partial failures (see per-row CSV status) |

### 📊 Example Session (verified on Windows x64, 2026-10-15)
```text
$ clipforge-cli.exe analyze a_sine.wav --backend cpp
File:               a_sine.wav
Format:             PCM Signed 16-bit 💽
Channels:           2 🔊  Sample Rate: 48000 Hz
Duration:           1.0000 s ⏱️
Peak Linear:        0.2500  Peak Level: -12.04 dBFS 📈
RMS Level:          -15.05 dBFS 📉  Clipped Samples: 0 ✅
DSP Backend Used:   cpp ⚙️

$ clipforge-cli.exe normalize a_sine.wav --target-dbfs -1.0 --output a_norm.wav
Normalized 'a_sine.wav' -> 'a_norm.wav' 🎚️
  Input Peak:  0.2500 → Output Peak: 0.8913 (-1.00 dBFS) 🎯
  Applied Gain: 11.04 dB 📢

$ clipforge-cli.exe trim c_sil.wav --threshold-dbfs -45 --tail-ms 100 --output c_trim.wav
Trimmed 'c_sil.wav' -> 'c_trim.wav' ✂️
  Original: 8000 → Trimmed: 4799 frames
  Removed Lead: 2001 | Trail: 1200 🧹
```

---

## 🧪 Testing

```powershell
# VSCode task (g++):
.\build\vscode\clipforge-tests.exe all

# CMake + CTest (MSVC):
ctest --preset windows-x64-release --output-on-failure
```

| Suite | File | Coverage |
|---|---|---|
| 🎵 `wav` | `tests/audio/test_wav_io.cpp` | PCM16/24/Float32 round-trip (mono+stereo), odd-size `JUNK` + `data`-before-`fmt `, `NaN`/`Inf` rejection, truncation rejection, duplicate-chunk + inconsistent-metadata + unsupported-format adversarial cases, valid/rejected `WAVE_FORMAT_EXTENSIBLE` |
| 🏭 `processing` | `tests/processing/test_processing.cpp` | Silent-norm safety, −6.02 dBFS → 0.5 linear, frame-exact trim (no-pad / tail-pad / head-pad), all-silent → 1 frame, min-duration filtering, Analyzer peak/RMS/clip/silence + empty report, invalid-parameter and `NaN` rejection, source-overwrite `PathCollision` |
| 🥊 `dsp` | `tests/dsp/test_dsp_differential.cpp` | 20 sizes (0 → 65537) × 4 misalignments × 6 thresholds, bit-exact `float` equality, canary guards, nullptr safety |
| 📦 `batch` | `tests/services/test_batch.cpp` | Dry-run counting without writes, real multi-file writes, missing input dir, input==output collision, existing-target refusal, CSV report content, trim operation |

**Last verified:** 2026-10-16 — Windows x64, g++ 16.1 (MSYS2 UCRT64), C++ backend — **4/4 suites PASS** ✅✅✅✅ · CI: MSVC+NASM core 4/4 + Qt 6/6 (`qt_bridge`, `qt_smoke`) ✅

---

## ⚡ Benchmarks

`clipforge-bench` validates C++ vs ASM equivalence, then times 5 warmup + 25 measured iterations (`steady_clock`, volatile sinks, fresh copies per gain iteration) and writes CSV.

**Reference machine:** Windows x64 · g++ 16.1 `-O2` · C++ backend (`ASM: NO` — install NASM for the ASM comparison):

| Kernel | Samples | Median | Throughput |
|---|---|---|---|
| `cf_peak_abs_f32` | 256 | 0.90 µs | 284 MS/s 🚀 |
| `cf_count_clipped_f32` | 256 | 0.50 µs | 512 MS/s 🚀 |
| `cf_apply_gain_clamp_f32` | 256 | 0.30 µs | 853 MS/s ⚡ |
| `cf_peak_abs_f32` | 4,096 | 13.7 µs | 299 MS/s |
| `cf_apply_gain_clamp_f32` | 4,096 | 1.8 µs | 2,275 MS/s ⚡ |
| `cf_peak_abs_f32` | 65,536 | 125 µs | 523 MS/s |
| `cf_apply_gain_clamp_f32` | 65,536 | 31.6 µs | 2,073 MS/s ⚡ |
| `cf_peak_abs_f32` | 1,048,576 | 940 µs | 1,115 MS/s |
| `cf_apply_gain_clamp_f32` | 1,048,576 | 323 µs | 3,240 MS/s ⚡⚡ |
| `cf_peak_abs_f32` | 16,777,216 | 15.7 ms | 1,063 MS/s |
| `cf_apply_gain_clamp_f32` | 16,777,216 | 7.1 ms | 2,343 MS/s ⚡⚡ |

```powershell
.\build\vscode\clipforge-bench.exe --csv bench.csv   # 📁 full machine-readable report
```

> 🥊 For the ASM vs C++ shootout, build with NASM via the `windows-x64-release` preset — the bench asserts bit-exact equivalence before timing.

---

## 🏗️ Architecture

```text
ClipForge/
├── include/clipforge/
│   ├── core/result.hpp            # 📦 Result<T> = std::expected<T, Error>
│   ├── dsp/kernels.hpp            # ⚡ Backend Auto/Cpp/Asm + dispatch API
│   ├── audio/                     # 🎵 wav_types, audio_buffer, reader/writer
│   ├── processing/                # 🔍 analyzer, 🎚️ normalizer, ✂️ trimmer
│   └── services/engine_facade.hpp # 🏭 Qt-ready facade, batch, CSV, cancel
├── src/                           # C++ impl + dsp/asm/win64/kernels_sse2.asm
├── apps/cli/main.cpp              # analyze | normalize | trim | batch
├── apps/qt/                       # 🖥️ Qt 6 Quick slice 1: bridge, queue model, QML, headless tests
├── tests/                         # wav + processing + dsp-differential + batch
├── benchmarks/main_bench.cpp
├── docs/                          # architecture, formats, assembly, building, release
└── CMakeLists.txt + CMakePresets.json
```

**Data flow:** 📂 WAV → `WavReader` → `AudioBuffer<float>` → `Analyzer` / `Normalizer` / `Trimmer` (via `dsp::*`) → `WavWriter` (tmp + rename) → 📂 clean WAV + 📊 CSV

See [docs/architecture.md](docs/architecture.md) for the full layered-design writeup.

---

## 📚 Documentation

| Doc | Description |
|---|---|
| [📐 docs/architecture.md](docs/architecture.md) | Layered design, module responsibilities, threading model |
| [🎵 docs/audio-format-support.md](docs/audio-format-support.md) | Supported encodings, validation & rejection policy |
| [⚙️ docs/assembly-design.md](docs/assembly-design.md) | Win64 ABI mapping, register discipline, SIMD strategy |
| [🔧 docs/building.md](docs/building.md) | CMake presets, VSCode tasks, NASM setup, troubleshooting |
| [📦 docs/release-process.md](docs/release-process.md) | Release triggers, version gates, ASM gate, API verification |
| [📝 CHANGELOG.md](CHANGELOG.md) | Release history (Keep a Changelog) |
| [🤝 CONTRIBUTING.md](CONTRIBUTING.md) | Workflow, coding standards, PR checklist |
| [🔒 SECURITY.md](SECURITY.md) | Supported versions & disclosure policy |

---

## 🗺️ Roadmap

- [x] ✅ **Phase 1 (MVP):** parser/writer, analyze, normalize, trim, batch, dual DSP, CLI, tests, bench
- [x] 🖥️ **Phase 2 slice 1:** Qt 6 Quick batch app — queue, progress/cancel, theme, headless tests, `windeployqt` CI artifact
- [ ] 🌊 **Phase 2 slice 2:** waveform preview, folder picker, DSP parameter fields, CSV toggle
- [ ] ⚡ AVX2 backend (after SSE2 baseline on target hardware)
- [ ] 🎧 FLAC / MP3 / OGG decode
- [x] 🤖 Automation: Dependabot + auto-merge + CI + Windows release (see [📦 docs/release-process.md](docs/release-process.md))

---

## 🤖 Automation

| Workflow | File | What it proves |
|---|---|---|
| 🧪 CI | `.github/workflows/ci.yml` | Ubuntu g++ + Windows MSVC/NASM build, core CTest 4/4, **ASM gate** (`ASM Backend Compiled: YES`), version check, Qt 6.8 job (release/tag/dispatch-gated) with `qmllint` + offscreen CTest 6/6 + `windeployqt` artifact. No `continue-on-error` on gates. |
| 🤖 Auto-merge | `.github/workflows/auto-merge-dependabot.yml` | Org-standard `ThanhNguyxnAgent[bot]` flow: safe updates auto-approved (`--auto --squash`, waits for CI), major prod updates flagged to `ThanhNguyxn07,RealThanhNguyxn`. |
| 📦 Release | `.github/workflows/release.yml` | `Release: vX.Y.Z` / tag / dispatch → rebuild + retest + ASM gate **in the same job** → `vX.Y.Z` tag + ZIP/EXE assets + CHANGELOG notes. |
| 👥 Auto-assign | `.github/workflows/auto-assign.yml` | Human-only routing via org reusable workflow. |
| 📌 Dependabot | `.github/dependabot.yml` | `github-actions` weekly (Mon 06:00 Asia/Ho_Chi_Minh), cooldown 7d. |

Required secrets: `BOT_APP_ID`, `BOT_PRIVATE_KEY` (GitHub App for `ThanhNguyxnAgent[bot]`).

---

## 🖥️ Qt Desktop App (Slice 1)

A Qt 6 Quick batch GUI ships alongside the CLI: file queue, per-file and overall progress, cancel, and the same `EngineFacade` guarantees (collision guards, CSV reports). No local Qt needed — download `ClipForge-<ver>-windows-x64-qt.zip` from any [GitHub Release](https://github.com/ThanhNguyxnOrg/ClipForge/releases) (double-clickable `clipforge-ui.exe`, `vc_redist` bundled) or from a green `CI 🧪` run artifact. Waveform preview arrives in slice 2.

---

## 🤝 Contributing

Contributions are welcome! 🎉 Please read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md) first.

Quick version: 🍴 fork → 🌿 branch → ✅ `clipforge-tests.exe all` green → 📬 PR with CLI output + bench CSV (for DSP changes).

---

## 🔒 Security

See [SECURITY.md](SECURITY.md) for supported versions and how to report vulnerabilities privately. Please do **not** open public issues for security reports.

---

## 📄 License

MIT © 2026 ClipForge Contributors 💖 — see [LICENSE](LICENSE).

---

<div align="center">

### 💖 Built with C++23 + Assembly for offline audio 💖
### ⭐ Star the repo if it helps your workflow! ⭐🚀🎉

`#cpp23` `#dsp` `#audio` `#nasm` `#sse2` `#offline-first` `#clipforge` 🎬🎧⚡

</div>
