# Audio Format Support & Operational Safety 🎵🛡️

## 1. Supported WAV Encodings (MVP)

| Property | Supported |
|---|---|
| 🗂️ Container | Standard `RIFF` / `WAVE`, little-endian |
| 🏷️ Format tags | `WAVE_FORMAT_PCM (0x0001)` — Signed 16/24-bit · `WAVE_FORMAT_IEEE_FLOAT (0x0003)` — 32-bit float · `WAVE_FORMAT_EXTENSIBLE (0xFFFE)` when the subformat GUID is PCM/Float and `validBits == bitsPerSample` |
| 🔊 Channels | Mono (1), Stereo (2) |
| ⏱️ Sample rates | 1 Hz – 768,000 Hz (validated; typical 8k – 192k) |
| 🧩 Chunks | Arbitrary order; `JUNK`, `LIST`, `bext`, `fact`, unknown chunks skipped; `data`-before-`fmt ` accepted |
| 📏 Alignment | Odd-size chunk payloads must carry a pad byte (per RIFF spec) |

Internal representation is always interleaved `float` normalized to `[-1.0, +1.0]`; output files retain the source encoding unless `override_format` is set.

## 2. Validation & Rejection Policy

| Condition | Result |
|---|---|
| Missing `RIFF` / non-`WAVE` | ❌ `InvalidWavHeader` |
| `RIFF` size exceeds buffer | ❌ `TruncatedFile` |
| Truncated subchunk header/payload, missing pad byte | ❌ `TruncatedFile` |
| `fmt ` < 16 bytes, extensible < 40 bytes | ❌ `CorruptedChunk` |
| Duplicate `fmt ` / `data` | ❌ `CorruptedChunk` |
| Inconsistent `blockAlign` / `byteRate` | ❌ `CorruptedChunk` |
| `data` size % `blockAlign` != 0 | ❌ `CorruptedChunk` |
| Channels ∉ {1, 2}, PCM bits ∉ {16, 24}, float bits ≠ 32 | ❌ `UnsupportedWavFormat` |
| Unknown format tag / GUID | ❌ `UnsupportedWavFormat` |
| `NaN` / `±Infinity` float sample | ❌ `InvalidSampleValue` (reader *and* pre-encode check) |
| Empty / missing `data` | ❌ `EmptyAudio` |
| File > 1 GiB | ❌ `InvalidParameter` (allocation guard) |
| Encoded RIFF > 4 GiB | ❌ `InvalidParameter` (spec limit) |

## 3. Writer Safety 💾

1. **Pre-flight checks:** empty/non-frame-aligned buffers and non-finite samples are rejected before touching disk.
2. **Collision guards:** `source_path_guard` refuses to overwrite the input; existing destinations require `overwrite_existing = true`.
3. **Crash safety:** encode fully in memory → write sibling `*.clipforge.tmp` → `flush` → atomic `rename`. Partial files are removed on failure.
4. **Rounding:** PCM16 uses `lrintf(x * 32768)` clamped to `[-32768, 32767]`; PCM24 uses `lrintf(x * 8388608)` clamped to `[-8388608, 8388607]`; Float32 is bit-copied.
5. **Padding:** odd-byte `data` payloads get a trailing zero pad byte.

## 4. Out of Scope (for now)

FLAC / MP3 / OGG, >2 channels, >32-bit float, RF64/BW64 (>4 GiB), and embedded cue/marker preservation — tracked on the [roadmap](../README.md#-roadmap). 🗺️
