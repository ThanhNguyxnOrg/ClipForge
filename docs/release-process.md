# ClipForge Release Process

ClipForge uses the organization-level release controller in [`ThanhNguyxnOrg/.github`](https://github.com/ThanhNguyxnOrg/.github). This repository keeps its native Windows/MSVC/NASM/Qt build adapter; it does not own a second version-bump, tag-creation, or GitHub Release publisher.

## Source of release policy

The central `release-policy.json` profile for ClipForge defines:

- Release branch: `main`
- Release mode: central `release: vX.Y.Z` commit
- Version source: `CMakeLists.txt` `project(... VERSION ...)`
- Changelog: `CHANGELOG.md`
- Build adapter: `Release 📦`
- Required assets:
  - `ClipForge-{version}-windows-x64.zip`
  - `ClipForge-cli-{version}-windows-x64.exe`
  - `ClipForge-{version}-windows-x64-qt.zip`

The initial version and baseline commit are also recorded centrally. Do not independently increment the version, create the tag, or publish a second release from this repository.

## Release lifecycle

1. Conventional commits on `main` are evaluated by the central Release Controller. A release-worthy change is classified using the central SemVer policy.
2. The controller updates `CMakeLists.txt` and `CHANGELOG.md` in a bot-authored `release: vX.Y.Z` commit.
3. The central CI profile requires the ClipForge checks `🔖 Version consistency`, `🐧 g++ C++ backend (ubuntu)`, and `🪟 MSVC + NASM (windows)` to succeed on the release commit. Missing, failed, skipped, neutral or pending required checks hold the release.
4. Once the release commit is green, the central controller creates the real `vX.Y.Z` Git tag and a draft GitHub Release. The tag-create event starts this repository's `Release 📦` adapter.
5. The adapter validates the tag against `CMakeLists.txt` and `CHANGELOG.md`, then builds and verifies the Windows x64 artifacts in one job:
   - MSVC environment and NASM are installed before CMake configuration.
   - CMake configures the release preset with Qt UI enabled.
   - The native build and CTest suite must pass.
   - The benchmark must report `ASM Backend Compiled: YES`; a C++-only fallback is rejected.
   - QML lint must pass.
   - The CLI/benchmark ZIP, standalone CLI executable, and Qt deployment ZIP are packaged and uploaded to the central draft.
6. The central controller waits for the exact configured workflow run on the exact tag commit. It publishes only after that run succeeds and all three centrally declared `requiredAssets` are present and uploaded.

The local adapter is therefore responsible for native compilation, tests, packaging and upload. The central controller owns versioning, changelog commits, tags, release metadata, required-check policy, asset verification and final publication.

## Verify a release

Run these commands from a machine with GitHub CLI authenticated:

```powershell
$repo = "ThanhNguyxnOrg/ClipForge"
$tag = "vX.Y.Z"

# Confirm the central release is public and list its assets.
gh release view $tag --repo $repo
gh api "repos/$repo/releases/tags/$tag" --jq '{draft: .draft, assets: [.assets[].name]}'

# Confirm the tag exists and resolve it to its commit.
git ls-remote --tags "https://github.com/$repo.git" "refs/tags/$tag"

# Inspect the project-specific build adapter runs.
gh run list --repo $repo --workflow "Release 📦" --limit 10
```

A green CI run alone does not prove a release is ready. Check the public release page and verify all three manifest assets.

## Failure handling

- If the adapter fails, the central controller leaves the draft unpublished. Fix the failing build/test/package step and rerun the failed adapter run.
- If the adapter succeeds but one required asset is absent, the central controller keeps the draft unpublished. Do not create a replacement release or delete the draft manually.
- If the public release is published, verify its manifest before announcing it.
- Keep NASM ahead of CMake configure and retain the runtime ASM assertion; both prevent a false green build.

## Maintainer checklist

- [ ] Use Conventional Commits on `main`; let the central controller calculate the next version.
- [ ] Keep the CMake version and changelog section valid for the generated release commit.
- [ ] Keep the native CI job names aligned with the central `ci-policy.json` profile.
- [ ] Keep all required output filenames aligned with the central `release-policy.json` manifest.
- [ ] Verify the exact tag's adapter run and all public assets after publication.
