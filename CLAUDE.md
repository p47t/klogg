# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Klogg is a multi-platform Qt-based GUI log viewer, originally forked from glogg. C++17, CMake, runs on Linux/Windows/macOS. Optimized for very large log files via memory-mapping, multi-threading, SIMD, and the Hyperscan regex backend.

## Build & test

Out-of-tree CMake build. Toolchains: C++17 compiler, CMake ≥ 3.12, Qt 5.9+ or Qt 6, Boost (headers), ragel. Most other deps come from CPM during configure (see `3rdparty/`).

```bash
mkdir -p build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .            # binaries → build/output/
ctest --build-config RelWithDebInfo --verbose   # all tests
```

Single test executable runs (Catch2 + QtTest, must use offscreen platform):

```bash
./build/output/klogg_tests  -platform offscreen "[regex]"   # unit tests, filtered by Catch2 tag/name
./build/output/klogg_itests -platform offscreen             # UI integration tests
```

Notable CMake options:
- `-DKLOGG_USE_HYPERSCAN=OFF` — fall back to Qt regex (needed on non-SSSE3 hardware or when ragel/boost are unavailable).
- `-DKLOGG_USE_SENTRY=ON` — enable crash reporting and dSYM/objcopy symbol post-processing.
- `-DKLOGG_GENERIC_CPU=ON` — `-march=x86-64 -mtune=generic` instead of `-march=native` (for distributable builds).
- `-DKLOGG_BUILD_TESTS=OFF` — skip tests.
- `-DKLOGG_OSX_DEPLOYMENT_TARGET=<10.14|10.15|11|12>` — must be ≥ Qt's deployment target.

CI reference: `.github/workflows/ci-build.yml`.

## Targets

The top-level CMake produces three executables in `src/app/`:
- **`klogg`** — main GUI app (with `AUTORCC`/`AUTOMOC`).
- **`klogg_portable`** — same sources, defines `KLOGG_PORTABLE` (Windows-focused, settings stored next to the binary).
- **`klogg_grep`** — headless CLI grep variant (`klogg_grep.cpp`); does not link `klogg_ui`.

Test executables: `klogg_tests` (unit, in `tests/unit/`) and `klogg_itests` (UI/integration, in `tests/ui/`). The `mainwindow_test.cpp` is excluded on Apple. There is also a `klogg_smoke` ctest that just runs `klogg -platform offscreen -v`.

## Module layout (`src/`)

Each module is its own static library following the `include/` + `src/` convention. Build order in `src/CMakeLists.txt` reflects the dependency order:

- `klogg_version` — generated version header.
- `logging` — logging facade (`log.h`).
- `utils` — generic helpers, CPU feature detection.
- `settings` — persisted user settings, highlighter sets, predefined filters.
- `crash_handler` — Sentry/Crashpad integration (no-op when `KLOGG_USE_SENTRY=OFF`).
- `regex` — Hyperscan + Qt regex abstraction.
- `logdata` — **core data model**: file loading, indexing, line storage, filtering. The big classes here (`LogData`, `LogFilteredData`, `LogDataWorker`, `LogFilteredDataWorker`, `CompressedLineStorage`, `LinePositionArray`, `EncodingDetector`) are what make klogg fast on multi-GB files; treat as performance-sensitive.
- `filewatch` — cross-platform file change watcher (drives tail-like reload).
- `versioncheck` — online update check.
- `ui` — all Qt widgets: `MainWindow`, `CrawlerWidget` (the per-tab view that owns a `LogMainView` + filtered view + `QuickFind`), `AbstractLogView` and the line/filtered/overview views, options/highlighter dialogs, scratchpad. Depends on everything below it.
- `app` — `main.cpp`, single-instance plumbing (`kdsingleapp`), CLI parsing (`cli.h`), Qt resources, translations (`i18n/`), and the three executable targets.

`logdata` and `ui` are split across two test trees: pure logic in `tests/unit/` (Catch2 only), Qt-driven flows in `tests/ui/` (Catch2 + `Qt::Test`).

## Code style

- `.clang-format` defines the canonical style: Qt-derived LLVM base, 4-space indent, 100-col limit, `Left` pointer alignment, spaces inside `(...)` and `[...]`, custom brace wrapping (`BeforeElse: true`, `AfterFunction: true`), constructor initializers break **before** the comma. Run clang-format on touched files; do not reflow unrelated code.
- `.clang-tidy` only enforces `readability-identifier-naming`: classes/structs `CamelCase`, functions/variables/parameters `camelBack`, namespaces `lower_case`, private members `camelBack_` (trailing underscore), global constants `CamelCase`.
- Commit messages use Conventional-Commits-style prefixes per `CONTRIBUTING.md`: `feat:`, `fix:`, `docs:`, `style:`, `refactor:`, `perf:`, `test:`, `build:`, `ci:`, `chore:`, `revert:`, `tr:`. Subject ≤ 70 chars, imperative mood.

## Cross-platform notes

Every change must build on Linux, Windows, and macOS — this is a hard project rule in `CONTRIBUTING.md`. Watch for:
- Hyperscan is **off by default on 32-bit Windows**; `regex/` must keep the Qt fallback path working.
- The custom allocator differs per platform (`mimalloc` on Linux, TBB on Windows, system on macOS); see `KLOGG_OVERRIDE_MALLOC` / `KLOGG_USE_MIMALLOC`.
- `mainwindow_test` is skipped on Apple (`tests/ui/CMakeLists.txt`).
- ASM is enabled in the top-level `project()` — Hyperscan and a few SIMD paths pull in assembly.
