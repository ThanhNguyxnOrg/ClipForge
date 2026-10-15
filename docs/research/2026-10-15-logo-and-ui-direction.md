# ClipForge Logo + UI Direction — Primary-Source Research (2026-10-15)

Context: `clipforge.svg` (120x120 viewBox, `width/height=256`) heads the README; hero is dark navy + orange `#FF5500–#FF8800` + cyan `#00E5FF` + monospace DSP readouts. Qt 6 Quick UI is Phase 2; CLI-only today.

## 1. GitHub README image conventions

- Prefer **relative links** for in-repo images: GitHub rewrites them per branch; "absolute links may not work in clones". Table gives `/assets/images/...` for `.md` on same branch. Source: https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax#relative-links
- Official dark/light pattern is `<picture>` + `prefers-color-scheme` + fallback `<img>` (used for profile/repo READMEs). Source: https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/quickstart-for-writing-on-github#adding-an-image-to-suit-your-visitors
- `<picture>` element is explicitly supported in GitHub Markdown. Source: https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax#the-picture-element
- GitHub renders PNG, JPG, GIF, PSD, **SVG**; SVGs get **no inline scripting or animation**; Firefox may not render SVGs. Source: https://docs.github.com/en/repositories/working-with-files/using-files/working-with-non-code-files#rendering-and-diffing-images
- External images are proxied through **Camo** (open-source SSL image proxy) to anonymized `*.githubusercontent.com` URLs; Camo enforces a **content-type whitelist** (`mime-types.json`) and cache rules — wrong `Content-Type` = image missing on github.com. Private-network/auth-gated hosts never render. Source: https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/about-anonymized-urls / https://github.com/atmos/camo / https://github.com/atmos/camo/blob/master/mime-types.json
- Recommendation: commit logo at `docs/assets/clipforge-logo.svg` (or repo-root `clipforge.svg` as today), reference relatively, pin explicit size, always set `alt`:
  `![ClipForge logo](docs/assets/clipforge-logo.svg)` or `<img src="docs/assets/clipforge-logo.svg" width="120" alt="ClipForge logo">`. `width` on `<img>` is the reliable sizing knob on github.com; the current file's `width="256"` is fine but `120–160` suits a README header next to the H1. Relative-path + `width` + `alt` behavior per the two docs URLs above.
- Dark/light option (only if a second variant is drawn): two files + the official `<picture>` snippet (dark `srcset`, light `srcset`, default `<img>` fallback). No `#gh-dark-mode-only` fragments — deprecated. Source: same quickstart URL above.

## 2. Qt 6 Quick/QML patterns for the Phase-2 batch UI

- Waveform: `Canvas { onPaint: var ctx = getContext("2d"); ... }`, repaint via `requestPaint()` / `markDirty(area)`; painting code must live in `onPaint` (port timers/`setInterval` to `Timer`/`requestAnimationFrame`). Source: https://doc.qt.io/qt-6/qml-qtquick-canvas.html
- Qt 6 supports one Canvas render target, `Canvas.Image` (QImage, background-thread capable); default strategy `Canvas.Immediate`. Doc warns: avoid large canvases/frequent animation on `Canvas.Image` (texture upload per update) and prefer `QQuickPaintedItem` + `QPainter` in C++ for heavy/large drawing. Source: https://doc.qt.io/qt-6/qml-qtquick-canvas.html
- ClipForge implication: feed Canvas (or preferably a C++ `QQuickPaintedItem`) with **precomputed min/max peaks per pixel** from C++ (SSE2/DSP side), not raw samples; overlays (clipping markers, silence regions, playhead) are cheap rects/lines on top.
- Batch queue: `ListView { model: ...; delegate: ... }`; models from `ListModel` or C++ `QAbstractItemModel`/`QAbstractListModel`; flickable by inheritance; only visible delegates instantiate. Source: https://doc.qt.io/qt-6/qml-qtquick-listview.html
- **Never store per-row state in the delegate** (instantiated/destroyed/reused any time); keep status/progress in the model, bind delegate to model roles; `reuseItems: true` recycles rows (handle `onPooled`/`onReused`). Source: https://doc.qt.io/qt-6/qml-qtquick-listview.html#avoid-storing-state-in-delegates
- Progress bar: `ProgressBar { from: 0; to: 100; value: ... }`, `indeterminate: true` when total unknown; `position`/`visualPosition` expose 0–1 fraction. Source: https://doc.qt.io/qt-6/qml-qtquick-controls-progressbar.html
- Worker reporting: `QPromise::setProgressRange(min,max)` + `setProgressValue(v)` / `setProgressValueAndText(v,text)`; worker loop polls `QPromise::isCanceled()` and calls `suspendIfRequested()`; `finish()` ends it. Source: https://doc.qt.io/qt-6/qpromise.html
- Control: `QFuture::cancel()` is async (pair with `waitForFinished()` for sync); `progressValue/minimum/maximum/text`, `isCanceled/isFinished/isRunning`, `then/onCanceled/onFailed` chaining; `QtConcurrent::run()` futures **cannot** be canceled/suspended, mapped/filter variants can. Source: https://doc.qt.io/qt-6/qfuture.html / https://doc.qt.io/qt-6/qtconcurrent-index.html
- GUI wiring: `QFutureWatcher::setFuture()` **after** connecting; signals `progressValueChanged/progressRangeChanged/progressTextChanged/resultReadyAt/finished/canceled`; slots `cancel/suspend/resume`; progress signals are throttled (last update always delivered). Bind `ProgressBar.value` to watcher progress; Cancel button to `watcher.cancel()`. Source: https://doc.qt.io/qt-6/qfuturewatcher.html
- Atomic cancel token: `QAtomicInteger<T>` (`loadAcquire/storeRelease`, `fetchAndAdd*`, `testAndSet*`) gives lock-free flag the DSP loop checks per block; complements `QPromise::isCanceled`. Source: https://doc.qt.io/qt-6/qatomicinteger.html
- `WorkerScript` (JS-in-thread, `sendMessage`/`onMessage`) is isolated: no QML-item or context-property access, message values restricted (bool/number/string/objects/arrays/ListModel). Source: https://doc.qt.io/qt-6/qml-qtqml-workerscript-workerscript.html — so DSP stays in C++/`QtConcurrent`, not `WorkerScript`.

## 3. Hero / social-preview conventions

- Social preview: PNG/JPG/GIF, **< 1 MB**, min `640x320`, best `1280x640`; set via repo Settings → Social preview → Upload. Source: https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/customizing-your-repositorys-social-media-preview
- Transparency (PNG) is supported but renders over varying platform backgrounds; docs recommend solid background unless sure. Source: same URL, "About transparency".
- Measured: `clipforge-social-preview.png` is **1280x640 RGBA, ~440 KB** — meets size/dimension limits; flatten to solid dark-navy if light-mode link unfurls look washed out.

## Applied recommendations

1. README header: relative-path SVG + `width="120"` + `alt`; keep `viewBox="0 0 120 120"`; optional `<picture>` pair only with a real light-variant asset.
2. Phase-2 QML: `ListView` (C++ model) + per-row `ProgressBar` + global bar; `QPromise` progress + `isCanceled` polling in DSP loop; `QFutureWatcher` → QML bridge; peaks-downsampled waveform via `QQuickPaintedItem`.
3. Keep hero palette tokens (`#FF5500/#FF8800/#00E5FF`, navy, mono readouts) as QML theme constants so README and UI share one direction.
