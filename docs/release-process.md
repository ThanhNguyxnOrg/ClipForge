# Release Process 📦

How ClipForge ships `vX.Y.Z` Windows x64 releases — and how to verify them like an engineer (not just a green badge).

## 1. Version Sources (single truth + cross-checks)

| Source | File / Place | Example |
|---|---|---|
| 🔧 Build truth | `CMakeLists.txt` → `project(ClipForge VERSION 0.1.1 ...)` | `0.1.1` |
| 📝 Human notes | `CHANGELOG.md` → `## [0.1.1] - YYYY-MM-DD` | must exist before release |
| 🏷️ Git tag | `v0.1.1` (created by the workflow if missing) | `v` prefix, SemVer |

The `check-trigger` job **fails the run** if trigger version ≠ `CMakeLists.txt`, or if `CHANGELOG.md` lacks the entry. Fix the files, not the workflow.

## 2. Triggers

Release builds run on **any one** of:

1. **Release commit to `main`** (preferred):
   ```text
   Release: v0.2.0
   # or: chore(release): v0.2.0 / [release] v0.2.0 / bump: v0.2.0
   ```
   ⚠️ Do **not** append `[skip ci]` — GitHub would skip the release workflow itself.
2. **Tag push**: `git tag -a v0.2.0 -m "Release v0.2.0" && git push origin v0.2.0`
3. **Manual**: Actions → `Release 📦` → `Run workflow` → optional `tag` input (defaults to `CMakeLists.txt`).

Dependabot pushes never trigger releases (`github.actor != 'dependabot[bot]'` guard).

## 3. What the Workflow Actually Proves

`build-release` (windows-latest) does, **in order, in the same job**:

1. 🐝 Install NASM **before** CMake configure (order is load-bearing).
2. ⚙️ `cmake --preset windows-x64-release` → 🔨 build → 🧪 `ctest --preset windows-x64-release`.
3. 🥊 **ASM gate**: `clipforge-bench` must print `ASM Backend Compiled: YES`. A silent C++ fallback fails the job — this is the fake-pass guard from the org audit.
4. 📝 Extract the `CHANGELOG.md` section for the version as release notes.
5. 📦 Package `ClipForge-<ver>-windows-x64.zip` (+ standalone `.exe`s), the Qt
   `ClipForge-<ver>-windows-x64-qt.zip` (`windeployqt` + official `vc_redist`),
   and ensure the `vX.Y.Z` tag exists (bot identity `ThanhNguyxnAgent[bot]`).
6. 🚀 Publish via `softprops/action-gh-release` (idempotent: existing tag → assets updated).

## 4. Verify a Release (API, not vibes)

```powershell
# 1. Release exists with assets:
gh release view v0.1.1 --repo ThanhNguyxnOrg/ClipForge

# 2. Tag points at the right commit:
git ls-remote --tags origin | Select-String 'v0\.1\.0'

# 3. Workflow run that produced it (logs: NASM line, CTest 4/4 core + 6/6 Qt, ASM YES):
#    Actions → Release 📦 → run → build-release → steps above
```

Lessons from the org audit, encoded here:
- **PurgeKit v1.2.1**: green run still had Tauri-action input warnings — always open the artifact + release page, not just the checkmark.
- **PurgeKit #141 / lumeo #79**: `npm ci` / unit-test reds must block merge and release — hence no `continue-on-error` on any gating step here.
- **apidex-hub link-checker**: `continue-on-error: true` is for *reporting* jobs only; CI/release gates must fail loudly.

## 5. Hotfix Checklist

- [ ] Bump `CMakeLists.txt` `VERSION` + add `CHANGELOG.md` entry
- [ ] `ctest --preset windows-x64-release` green locally (or via CI on the PR)
- [ ] Push a `Release: vX.Y.Z` commit (no `[skip ci]`)
- [ ] Watch the `Release 📦` run: configure → build → CTest → ASM YES → tag → published
- [ ] `gh release view vX.Y.Z` shows the ZIP + EXE assets
