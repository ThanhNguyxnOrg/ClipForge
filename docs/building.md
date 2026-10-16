# Building ClipForge 🔧

## 1. Toolchains

| Path | Compiler | CMake | NASM | Output |
|---|---|---|---|---|
| 🅰️ VSCode quick (g++) | `C:\msys64\ucrt64\bin\g++.exe` 13+ | ❌ not needed | ❌ ASM off | `build/vscode/*.exe` |
| 🅱️ Full (MSVC) | VS 2022 19.38+ | 3.25+ | 2.16+ optional | `build/release/*`, `build/debug/*` |

> The C++ DSP backend is always available. NASM only unlocks the ASM fast path; absence falls back cleanly to C++ (`CLIPFORGE_HAS_NASM_X64=0`).

## 2. Option A — VSCode + g++ (no CMake) ✅

Prerequisites: [MSYS2 UCRT64](https://www.msys2.org/) with `mingw-w64-ucrt-x86_64-gcc` (tested: GCC 16.1).

```powershell
# Build + test from VSCode:
#   Ctrl+Shift+B            -> build: tests (g++)
#   Terminal > Run Task     -> test: run all
```

Manual equivalent:

```powershell
New-Item -ItemType Directory -Force build/vscode | Out-Null
C:\msys64\ucrt64\bin\g++.exe -std=c++23 -DCLIPFORGE_HAS_NASM_X64=0 -Iinclude `
  src/dsp/cpp/kernels_cpp.cpp src/dsp/dispatch.cpp `
  src/audio/wav_reader.cpp src/audio/wav_writer.cpp `
  src/processing/analyzer.cpp src/processing/normalizer.cpp src/processing/silence_trimmer.cpp `
  src/services/engine_facade.cpp `
  tests/test_main.cpp tests/audio/test_wav_io.cpp tests/processing/test_processing.cpp tests/dsp/test_dsp_differential.cpp `
  -o build/vscode/clipforge-tests.exe
.\build\vscode\clipforge-tests.exe all
```

IntelliSense is preconfigured in `.vscode/c_cpp_properties.json` (C++23, UCRT64).

## 3. Option B — CMake (MSVC + optional NASM) 🏢

```powershell
# One-time MSYS2 packages (UCRT64 shell):
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-nasm

# Open "x64 Native Tools Command Prompt for VS 2022":
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
ctest --preset windows-x64-release --output-on-failure
.\build\release\clipforge-bench.exe --csv benchmark_results.csv
```

### Qt 6 Quick (CI-only)

The Qt slice needs Qt 6.8, installed only on CI (`install-qt-action@v4`, `win64_msvc2022_64` + `qtshadertools`). Add `-DCLIPFORGE_BUILD_QT_UI=ON` to the configure step; the `build-test-qt` job (release commits/tags, or dispatch with `run_qt=true`) builds `clipforge-ui`, runs `qmllint` and offscreen `CTest` (`clipforge.qt_bridge`, `clipforge.qt_smoke`), packages with `windeployqt` plus the official `vc_redist`, and uploads the ZIP artifact.

### CMake options

| Option | Default | Meaning |
|---|---|---|
| `CLIPFORGE_ENABLE_ASM` | `ON` | Enable NASM SSE2 when `nasm` + Win64 is detected |
| `CLIPFORGE_BUILD_CLI` | `ON` | Build `clipforge-cli` |
| `CLIPFORGE_BUILD_TESTS` | `ON` | Build `clipforge-tests` + CTest entries |
| `CLIPFORGE_BUILD_BENCH` | `ON` | Build `clipforge-bench` |
| `CLIPFORGE_BUILD_QT_UI` | `OFF` | Placeholder for Phase 2 Qt 6 UI |

## 4. Troubleshooting 🩹

| Symptom | Fix |
|---|---|
| `cmake: command not found` | Install CMake (MSYS2 package above or `winget install Kitware.CMake`) and reopen the terminal |
| `NASM not found; falling back to C++` | Expected without NASM — ASM backend reports `NO`, everything else works |
| `MSVC ... does not support /std:c++23` | Update to VS 2022 17.8+ and use the x64 Native Tools prompt |
| `PathCollision` on first run | You pointed `--output` at the input file or into the input dir — pick a distinct `--output-dir` |
| PowerShell shows mojibake for emoji | Cosmetic console-font issue — output files (WAV/CSV) are unaffected; use Windows Terminal + UTF-8 (`chcp 65001`) |
