# Windows x64 NASM SSE2 Design ⚙️

Target assembler: **NASM** (`-f win64`) · ABI: **Microsoft Windows x64** · File: `src/dsp/asm/win64/kernels_sse2.asm`

## 1. Calling Convention (Microsoft x64 ABI)

| Function | Arg 1 (`rcx`) | Arg 2 (`rdx`) | Arg 3 (`xmm2`) | Arg 4 (`xmm3`) | Return |
|---|---|---|---|---|---|
| `cf_peak_abs_f32` | `const float* samples` | `uint64_t count` | — | — | `xmm0` (`float`) |
| `cf_count_clipped_f32` | `const float* samples` | `uint64_t count` | `float threshold` | — | `rax` (`uint64_t`) |
| `cf_apply_gain_clamp_f32` | `float* samples` | `uint64_t count` | `float gain` | `float ceiling` | `void` |

> Floating-point parameters follow **positional slot mapping**: the Nth argument (of any type) uses the Nth slot — hence the 3rd parameter arrives in `xmm2` and the 4th in `xmm3`.

## 2. Register Discipline 🛡️

All three routines are **leaf functions** touching only volatile state:

- ✅ Allowed GPR: `rax`, `rcx`, `rdx`, `r8`–`r11`
- ✅ Allowed XMM: `xmm0`–`xmm5`
- 🚫 Never touched: `rbx`, `rbp`, `rdi`, `rsi`, `rsp`, `r12`–`r15`, `xmm6`–`xmm15`

No stack frame is required (`ret`-only epilogues), keeping call overhead minimal for small buffers.

## 3. SIMD Strategy

- 📏 **Unaligned access:** `movups` for loads/stores — trimmed sub-buffers and foreign allocators need no 16-byte preconditions.
- 🧱 **Main loop:** 4 floats per iteration (`count / 4` vectors in `r8`).
  - Peak: `andps` (abs) + `maxps`, then a 2-step horizontal reduction (`shufps 0x4E`, `shufps 0xB1` + `maxss`).
  - Clipped-count: `andps` + `cmpnltps` (yields `0xFFFFFFFF` for `|s| >= thr`) + `psubd` into `uint32` lane counters; horizontal `paddd`/`psrldq` sum into `rax`.
  - Gain+clamp: broadcast `gain`/`±ceiling` with `shufps`, then `mulps` → `maxps(-ceil)` → `minps(+ceil)`.
- 🦐 **Tails:** scalar SSE (`movss`/`maxss`/`minss`/`comiss` + `setae`) for `count % 4`.
- 🔢 **Overflow guard:** `cf_count_clipped_f32` retires lane counters to 64-bit `rax` every `0x3FFFFFFF` vectors, so 32-bit lanes can never wrap.

## 4. Equivalence & Benchmarking 🥊📈

- The C++ reference (`kernels_cpp.cpp`) uses bit-mask `fabs` for identical NaN/sign handling.
- `clipforge-bench` asserts `p_cpp == p_asm` and `c_cpp == c_asm` (bitwise for floats) before timing.
- The differential suite (`tests/dsp/test_dsp_differential.cpp`) covers 20 sizes × 4 misalignments × 6 thresholds with `memcpy`-based bit comparison plus canary guards against over-reads/writes.

## 5. Future Work

AVX2 (`ymm`, `-f win64` with `vzeroupper` discipline) plugs in behind the same `Backend` enum once SSE2 baselines are recorded — no caller changes required. ⚡
