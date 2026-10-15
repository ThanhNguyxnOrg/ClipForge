# Contributing to ClipForge 🤝

Thank you for considering a contribution! 🎉 This document defines the workflow, coding standards, and quality gates for the [ThanhNguyxnOrg/ClipForge](https://github.com/ThanhNguyxnOrg/ClipForge) repository.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Branching & Commits](#branching--commits)
- [Coding Standards](#coding-standards)
- [Testing Requirements](#testing-requirements)
- [Pull Request Checklist](#pull-request-checklist)
- [Review Process](#review-process)

## Code of Conduct

All contributors must follow our [Code of Conduct](CODE_OF_CONDUCT.md). Be kind, be professional. 💖

## Getting Started

1. 🍴 Fork the repository and clone your fork:
   ```powershell
   git clone https://github.com/<you>/ClipForge.git
   cd ClipForge
   ```
2. 🧰 Install the toolchain (see [docs/building.md](docs/building.md)):
   - Quick path: `C:\msys64\ucrt64\bin\g++.exe` 16+ + VSCode tasks
   - Full path: CMake 3.25+, Ninja, MSVC 2022, NASM 2.16+
3. ✅ Verify a clean baseline before changing anything:
   ```powershell
   .\build\vscode\clipforge-tests.exe all
   ```

## Branching & Commits

- 🌿 Branch from `main`: `feat/<short-name>`, `fix/<short-name>`, `docs/<short-name>`, `perf/<short-name>`
- ✍️ Use [Conventional Commits](https://www.conventionalcommits.org/):
  - `feat(dsp): add AVX2 gain kernel`
  - `fix(wav): reject duplicate fmt chunk`
  - `docs(readme): clarify batch CSV schema`
  - `test(trim): cover min-audio-ms edge`
- 📦 Keep PRs focused — one feature/fix per PR.

## Coding Standards

- 📏 **C++23**, no extensions (`CMAKE_CXX_EXTENSIONS OFF`). Headers must be self-contained (`#pragma once`).
- 🛡️ **No exceptions for control flow** — fallible APIs return `clipforge::Result<T>` / `Status`. See `include/clipforge/core/result.hpp`.
- 🧹 Warnings are errors in spirit: MSVC `/W4 /permissive-`, GCC/Clang `-Wall -Wextra -Wpedantic`. Zero new warnings.
- ⚡ **DSP kernels** (`include/clipforge/dsp/kernels.hpp`):
  - C++ reference must remain portable and testable without NASM.
  - ASM routines: Win64 ABI only, volatile registers (`rax`, `rcx`, `rdx`, `r8`–`r11`, `xmm0`–`xmm5`) — never touch `rbx/rbp/rdi/rsi/r12`–`r15` or `xmm6`–`xmm15`.
  - `movups` for arbitrary alignment; scalar tails for `count % 4`; nullptr/zero-count guards.
- 🎨 Format with `.clang-format` (LLVM base, 120 cols) before pushing.
- 💬 Comments explain *why*, not *what*. No dead code, no placeholders.

## Testing Requirements

- 🧪 Every behavior change ships with a test:
  - WAV parsing → `tests/audio/test_wav_io.cpp`
  - Processing/facade → `tests/processing/test_processing.cpp`
  - Kernels → `tests/dsp/test_dsp_differential.cpp` (C++ vs ASM bit-exactness)
- ✅ Gate: `clipforge-tests.exe all` must print 3/3 `[PASS]` lines.
- 🏭 Batch/CLI changes must include a manual smoke log in the PR (commands + exit codes + CSV excerpt).
- 📈 DSP performance changes must include `clipforge-bench` CSV before/after.

## Pull Request Checklist

- [ ] 📝 Title follows Conventional Commits
- [ ] 📖 Docs updated (`README.md`, `docs/`, `CHANGELOG.md` under `Unreleased`)
- [ ] 🧪 `clipforge-tests.exe all` green (paste output)
- [ ] ⚡ Bench CSV attached for DSP changes
- [ ] 🖥️ CLI smoke log attached for CLI/facade changes
- [ ] 🚫 No build artifacts committed (`build/`, `*.exe`, `*.csv` — see `.gitignore`)

## Review Process

Maintainers review within a few days. Expect feedback on correctness, edge cases (empty buffers, `NaN`, odd chunks, path collisions), and API symmetry with `EngineFacade`. Two approvals may be required for DSP/ASM changes. Once approved, squash-merge to `main`. 🚀
