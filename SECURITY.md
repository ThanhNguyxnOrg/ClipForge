# Security Policy 🔒

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | ✅ Supported        |
| < 0.1   | ❌ Not supported    |

Only the latest `main` branch and the most recent tagged release receive security fixes.

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security reports.** 🙏

Instead, use one of these private channels:

1. 🔐 **GitHub Private Vulnerability Reporting** (preferred):
   `Security` tab → `Report a vulnerability` on
   [ThanhNguyxnOrg/ClipForge](https://github.com/ThanhNguyxnOrg/ClipForge)
2. 📧 Email the maintainers (see `CODEOWNERS` / repo contacts).

Include:
- 📝 Description and impact
- 🔁 Reproduction steps (sample WAV hex dump, CLI command, commit SHA)
- 🖥️ Environment (OS, compiler, NASM version, backend flag)
- 🩹 Suggested fix, if any

## Response Process

- ⏱️ **Acknowledgement:** within 72 hours
- 🔍 **Triage & reproduction:** within 7 days
- 🛠️ **Fix & release:** patch release + `CHANGELOG.md` entry + credit (unless anonymity is requested)
- 📢 **Disclosure:** coordinated; public advisory after a fix is available

## Scope Notes

ClipForge is an **offline** tool by design — it never transmits audio over the network. Security-sensitive areas include:

- 🧩 RIFF/WAVE parsing (malformed-chunk DoS, oversized allocations — guarded by the 1 GiB cap)
- 💾 Writer path handling (symlink/rename races, overwrite guards)
- ⚡ Native ASM kernels (out-of-bounds reads/writes — covered by differential + canary tests)

Thank you for helping keep ClipForge safe! 💖
