# Pull Request Template

## 📝 Description

<!-- What does this PR change and why? Link issues: Closes #... -->

## 🔧 Type

- [ ] ✨ feat (new feature)
- [ ] 🐛 fix (bug fix)
- [ ] 📚 docs (documentation only)
- [ ] 🧪 test (tests only)
- [ ] ⚡ perf (performance)
- [ ] 🧹 chore/refactor

## ✅ Checklist

- [ ] Title follows Conventional Commits (`feat(scope): ...`)
- [ ] `clipforge-tests.exe all` is green (paste output below)
- [ ] Docs updated (`README.md` / `docs/` / `CHANGELOG.md`)
- [ ] No build artifacts committed (`build/`, `*.exe`, `*.csv`)
- [ ] CLI smoke log attached (for CLI/facade changes)
- [ ] Bench CSV before/after attached (for DSP changes)

## 🧪 Test Output

```text
<!-- paste clipforge-tests output + ctest output -->
```

## 📊 Bench / Smoke (if applicable)

```text
<!-- paste CLI session or bench rows -->
```
