# ClipForge Architecture 📐

## 1. Overview

ClipForge is a layered, offline-first audio toolkit. Dependencies flow **inward**:

```text
apps/cli ──▶ services ──▶ processing ──▶ audio ──▶ dsp
                                  │          │
                                  └──── core (Result<T>/Error)
```

- **Outward layers** own I/O, orchestration, and UX concerns.
- **Inward layers** are pure, testable, and UI-agnostic.
- No layer skips a level: `services` never calls raw NASM routines directly; it goes through `processing` → `dsp` dispatch.

## 2. Layers

### 2.1 `clipforge::dsp` ⚡ (`include/clipforge/dsp/`, `src/dsp/`)
Low-level kernels over contiguous `float` buffers:
`cf_peak_abs_f32`, `cf_count_clipped_f32`, `cf_apply_gain_clamp_f32`.

- `src/dsp/cpp/kernels_cpp.cpp` — portable C++23 reference, always compiled and tested.
- `src/dsp/asm/win64/kernels_sse2.asm` — Windows x64 NASM SSE2 fast path (optional at configure time).
- `src/dsp/dispatch.cpp` — `resolve_backend()` + `peak_abs_f32()` / `count_clipped_f32()` / `apply_gain_clamp_f32()` entry points.

Rules: `noexcept` everywhere, nullptr/zero-count safe, alignment-free loads (`movups`), scalar tails.

### 2.2 `clipforge::audio` 🎵 (`include/clipforge/audio/`, `src/audio/`)
RIFF/WAVE parsing, validation, and serialization.

- `WavReader::read_file()` / `read_bytes()` → normalized interleaved `float` in `[-1, +1]` (`AudioBuffer`), preserving source format metadata.
- `WavWriter::encode_bytes()` / `write_file()` → re-encodes at the source bit depth by default; writes via sibling temp file + atomic rename.
- Safety rails: 1 GiB file cap, `blockAlign`/`byteRate` cross-checks, `NaN`/`Inf` rejection, duplicate-chunk rejection, odd-size pad-byte enforcement.

### 2.3 `clipforge::processing` 🔧 (`include/clipforge/processing/`, `src/processing/`)
Pure DSP use-cases over `AudioBuffer` — no filesystem, no UI:

| Class | Responsibility |
|---|---|
| `Analyzer` | Peak/RMS (linear + dBFS), clipped count, per-frame silence map |
| `Normalizer` | Peak-to-target gain with ceiling clamp + silent-input guard |
| `SilenceTrimmer` | Leading/trailing trim with dBFS threshold, head/tail padding, min-speech guard |

### 2.4 `clipforge::services` 🏭 (`include/clipforge/services/`, `src/services/`)
Qt-ready orchestration for single files and directory batches:

- `inspect_file()`, `normalize_file()`, `trim_file()` — read → process → guarded write.
- `run_batch()` — discovery (flat/recursive), relative-path mapping, collision/overwrite guards, progress callback, `std::atomic<bool>` cancel token, optional CSV via `write_batch_csv()`.
- Stateless and thread-safe per call; wrapped by the Qt `EngineBridge` `QObject` since slice 1.

### 2.5 Qt app 🖥️ (`apps/qt/`)
Thin presentation layer over `services`, built only with `CLIPFORGE_BUILD_QT_UI=ON`:

- `EngineBridge` — runs `run_batch` on a worker thread (`QtConcurrent` + `QFutureWatcher`), forwards progress/cancel, and maps `BatchSummary` records back into rows. Every run terminates with `batchFinished`; `batchError` is informational.
- `BatchQueueModel` — `QAbstractListModel` holding per-row file/status/progress/error; row state lives in the model, never in delegates.
- `qml/` — single-window `Main.qml` (queue, inspector, global bar) + `Theme.qml` tokens; verified headless (`-platform offscreen`) with `qmllint` clean.

## 3. Cross-Cutting Concerns

### 3.1 Error Model 📦
`clipforge::Result<T> = std::expected<T, Error>` and `Status = std::expected<void, Error>` (`include/clipforge/core/result.hpp`).
`Error{ErrorCode, message}` carries actionable diagnostics — no exceptions for control flow, no hidden `errno`.

### 3.2 Threading & Cancellation 🧵
`EngineFacade::run_batch()` is sequential today (deterministic CSV ordering) but re-entrant: pass distinct `BatchRequest`s from multiple threads. Long batches poll the atomic cancel token between files.

### 3.3 Test Strategy 🧪
- WAV I/O: golden round-trips + adversarial chunks (duplicates, bad metadata, odd padding, extensible GUIDs, non-finite floats).
- Processing: exact frame math at 1000 Hz (1 frame = 1 ms), all-silent/min-duration/head-pad edges, Analyzer vectors, invalid-parameter and `NaN` rejection.
- Batch services: dry-run counting, real writes, collisions, existing targets, CSV content, trim operation.
- DSP: C++ vs ASM differential with bit-exact float comparison across sizes, misalignments, and thresholds.
- Qt: headless C++ model/bridge tests plus QML smoke (500-row load, warning-free engine start, click-driven batch).

## 4. Future Hooks (Phase 2+)
- 🌊 Waveform preview (`QQuickPaintedItem` fed with precomputed peaks) and folder picker for the Qt slice 2.
- ⚡ AVX2 kernels slot behind the same `Backend` enum + `resolve_backend()`.
- 🎧 New decoders (FLAC/MP3/OGG) only need to produce `AudioBuffer`; processing stays untouched.
