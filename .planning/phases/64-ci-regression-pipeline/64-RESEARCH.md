# Phase 64: CI Regression Pipeline - Research

**Researched:** 2026-03-08
**Domain:** GitHub Actions CI, benchmark-action/github-action-benchmark, nanobench JSON, gh-pages branch strategy
**Confidence:** HIGH

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CI-01 | GitHub Actions workflow (.github/workflows/benchmarks.yml) triggers on push to main and PRs touching src/** or include/** | Workflow trigger syntax: `on.push.paths` and `on.pull_request.paths` with `src/**` and `include/**` globs |
| CI-02 | JSON conversion script combines benchmark results into github-action-benchmark customSmallerIsBetter format | Conversion: multiply `median(elapsed)` (seconds) × 1e9 → ns/op; map `medianAbsolutePercentError(elapsed)` → `range`; merge all three bench-results/*.json into one array |
| CI-03 | Benchmark history stored on gh-pages branch (isolated from Docusaurus deployment) | Use `gh-pages-branch: bench-data` input — action pushes data commits only to `bench-data`, never touching the `gh-pages` branch that `docs.yml` deploys via `actions/deploy-pages` |
| CI-04 | Performance dashboard auto-generated on gh-pages from benchmark history | github-action-benchmark generates `index.html` + `data.js` in `benchmark-data-dir-path` on the `bench-data` branch; GitHub Pages can serve from `bench-data` branch at a sub-path |
| CI-05 | Regression threshold with fail-on-alert for PRs | `fail-on-alert: true` + `alert-threshold: '150%'` on PR job; push-to-main job uses `auto-push: true` + `auto-push: false` guard via `github.event_name == 'push'` conditional |
</phase_requirements>

---

## Summary

Phase 64 wires together three existing outputs — the nanobench JSON files in `bench-results/`, the `scripts/build-bench.sh` runner, and the `github-action-benchmark@v1` GitHub Action — into a single workflow file (`.github/workflows/benchmarks.yml`). The workflow has two logical paths: (1) push-to-main records results to a persistent `bench-data` branch and updates the dashboard; (2) PRs build and compare against the most recent baseline, failing the workflow when the `150%` regression threshold is crossed.

The critical architectural decision already made (PROJECT.md) is that benchmark history lives on a separate `bench-data` branch, NOT on `gh-pages`. This sidesteps a concrete conflict: the existing `docs.yml` uses `actions/deploy-pages` which wipes the github-pages environment's source on every deploy. The `github-action-benchmark` action's `gh-pages-branch: bench-data` input routes all data commits to `bench-data`, which is a plain git branch that `docs.yml` never touches.

The JSON conversion script is a straightforward Python or JavaScript pass that reads every file in `bench-results/`, extracts `name` and `median(elapsed)` (converting seconds → nanoseconds), maps `medianAbsolutePercentError(elapsed)` to the optional `range` field, and writes a single `bench-combined.json` array in `customSmallerIsBetter` format. The `bench-data` branch must be created as an orphan before the first workflow run — this is a one-time prerequisite.

**Primary recommendation:** Use `benchmark-action/github-action-benchmark@v1` with `tool: customSmallerIsBetter`, `gh-pages-branch: bench-data`, `alert-threshold: '150%'`, split into two jobs gated by `github.event_name`.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| benchmark-action/github-action-benchmark | v1 | Store history, render dashboard, alert on regression | Official GitHub Action Marketplace; supports customSmallerIsBetter; handles git commits to data branch |
| actions/checkout | v4 | Checkout repository in CI | Current standard; v3 deprecated |
| actions/setup-node | v4 | Node.js for conversion script (optional) | Current standard; v3 deprecated |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Python 3 (ubuntu-latest built-in) | 3.12+ | JSON conversion script | ubuntu-latest ships Python 3; no install step needed |
| actions/cache | v4 | Cache cmake build artifacts to speed CI | Useful if build time exceeds 3 min; optional but recommended |
| concurrency group | N/A | Prevent race conditions on bench-data branch | Required when multiple pushes queue up |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| github-action-benchmark@v1 | Bencher (bencher.dev) | Bencher is more feature-rich but requires an account/token; github-action-benchmark is self-contained |
| Python conversion script | Node.js script | Either works; Python is available without a setup step on ubuntu-latest |
| bench-data branch (git push) | actions/cache external-data-json-path | Cache approach is simpler but history is wiped when cache expires (7 days max); git branch is permanent |

**Installation:**
```bash
# No npm install needed — github-action-benchmark is called via uses: in workflow YAML
# Python and cmake already available on ubuntu-latest
# Lua dependency needed for bench_lua:
sudo apt-get install -y liblua5.4-dev
```

---

## Architecture Patterns

### Recommended File Structure
```
.github/
└── workflows/
    ├── docs.yml             # EXISTING: Docusaurus deploy to gh-pages via deploy-pages
    └── benchmarks.yml       # NEW: build+run benchmarks, convert JSON, store history

scripts/
├── build-bench.sh           # EXISTING: cmake configure + build + run all binaries
└── convert-bench.py         # NEW: nanobench JSON → customSmallerIsBetter JSON

bench-results/               # EXISTING: written by benchmark binaries
    bench_canvas.json
    bench_ecs.json
    bench_lua.json

(remote branch: bench-data)  # NEW orphan branch: github-action-benchmark writes here
    dev/bench/
        index.html           # auto-generated dashboard
        data.js              # benchmark history
```

### Pattern 1: Two-Job Workflow (push vs PR)
**What:** A single workflow file with two jobs. The `store` job runs only on `push` to main and writes to `bench-data`. The `compare` job runs on all triggers and uses `save-data-file: false` to compare without writing, then `fail-on-alert: true` to block regression.
**When to use:** Prevents PRs from polluting the history branch while still giving regression feedback.

```yaml
# Source: github-action-benchmark README + verified against action.yml inputs
name: Benchmarks

on:
  push:
    branches: [main]
    paths:
      - 'src/**'
      - 'include/**'
  pull_request:
    paths:
      - 'src/**'
      - 'include/**'

concurrency:
  group: benchmarks-${{ github.ref }}
  cancel-in-progress: false   # do NOT cancel — bench-data push must complete

jobs:
  benchmark:
    runs-on: ubuntu-latest
    permissions:
      contents: write          # needed for auto-push to bench-data branch

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update -qq
          sudo apt-get install -y liblua5.4-dev

      - name: Build and run benchmarks
        run: bash scripts/build-bench.sh

      - name: Convert to customSmallerIsBetter format
        run: python3 scripts/convert-bench.py

      # Store history + render dashboard (push to main only)
      - name: Store benchmark result
        if: github.event_name == 'push'
        uses: benchmark-action/github-action-benchmark@v1
        with:
          name: enjin2 Benchmarks
          tool: customSmallerIsBetter
          output-file-path: bench-results/bench-combined.json
          gh-pages-branch: bench-data
          benchmark-data-dir-path: dev/bench
          github-token: ${{ secrets.GITHUB_TOKEN }}
          auto-push: true
          comment-always: false

      # Compare against baseline for PRs (no write to history branch)
      - name: Check for regression (PR only)
        if: github.event_name == 'pull_request'
        uses: benchmark-action/github-action-benchmark@v1
        with:
          name: enjin2 Benchmarks
          tool: customSmallerIsBetter
          output-file-path: bench-results/bench-combined.json
          gh-pages-branch: bench-data
          benchmark-data-dir-path: dev/bench
          github-token: ${{ secrets.GITHUB_TOKEN }}
          auto-push: false
          save-data-file: false
          alert-threshold: '150%'
          fail-on-alert: true
          comment-on-alert: true
```

### Pattern 2: nanobench → customSmallerIsBetter Conversion
**What:** A Python script that merges all three `bench-results/*.json` files into one `bench-combined.json` array.
**When to use:** Always — this is the required glue between nanobench's output format and the benchmark action's expected format.

**Nanobench output format (actual observed):**
```json
{
    "results": [
        {
            "name": "canvas4: setPixel",
            "unit": "op",
            "median(elapsed)": 2.1e-08,
            "medianAbsolutePercentError(elapsed)": 0.045,
            ...
        }
    ]
}
```

**customSmallerIsBetter required format:**
```json
[
    {
        "name": "canvas4: setPixel",
        "unit": "ns/op",
        "value": 21.0,
        "range": "± 4.5%"
    }
]
```

**Conversion script (`scripts/convert-bench.py`):**
```python
#!/usr/bin/env python3
"""Convert nanobench JSON output to github-action-benchmark customSmallerIsBetter format.

Reads: bench-results/bench_canvas.json, bench_ecs.json, bench_lua.json
Writes: bench-results/bench-combined.json
"""
import json
import os

RESULTS_DIR = os.path.join(os.path.dirname(__file__), '..', 'bench-results')
INPUT_FILES = ['bench_canvas.json', 'bench_ecs.json', 'bench_lua.json']
OUTPUT_FILE = os.path.join(RESULTS_DIR, 'bench-combined.json')

combined = []
for fname in INPUT_FILES:
    path = os.path.join(RESULTS_DIR, fname)
    with open(path) as f:
        data = json.load(f)
    for r in data['results']:
        elapsed_s = r.get('median(elapsed)', 0.0)
        mape = r.get('medianAbsolutePercentError(elapsed)', 0.0)
        combined.append({
            'name': r['name'],
            'unit': 'ns/op',
            'value': round(elapsed_s * 1e9, 4),
            'range': f'± {round(mape * 100, 2)}%',
        })

with open(OUTPUT_FILE, 'w') as f:
    json.dump(combined, f, indent=2)

print(f'Wrote {len(combined)} benchmarks to {OUTPUT_FILE}')
```

### Pattern 3: bench-data Orphan Branch Creation (one-time setup)
**What:** Create an orphan branch that github-action-benchmark can push to. Must be done before the first workflow run.
**When to use:** Once, as part of Wave 0 setup.

```bash
# One-time: create bench-data as orphan branch
git checkout --orphan bench-data
git rm -rf .
git commit --allow-empty -m "chore: initialize bench-data branch"
git push origin bench-data
git checkout main
```

### Anti-Patterns to Avoid
- **Using `gh-pages-branch: gh-pages`:** The existing `docs.yml` uses `actions/deploy-pages` which manages the `github-pages` environment. The benchmark action's direct git push to `gh-pages` would conflict with `deploy-pages` artifact-based deployments — the two write mechanisms are incompatible. Use `bench-data` instead.
- **Running `auto-push: true` on PRs:** Anyone who opens a PR can trigger history corruption. Gate `auto-push` behind `if: github.event_name == 'push'`.
- **Using `cancel-in-progress: true` on the concurrency group:** If a push-to-main is canceled mid-way through the `auto-push` step, the `bench-data` branch can be left in a partial state. Use `cancel-in-progress: false`.
- **Setting `alert-threshold: '110%'`:** GitHub Actions shared runners have high variance (~10-30% jitter on ubuntu-latest). The project decision (STATE.md) is to start at `150%` and tighten after 30-50 baseline runs.
- **Not prefixing benchmark names by suite:** The `name` field in `customSmallerIsBetter` must be globally unique across all benchmarks in the combined JSON. The existing benchmark names (`canvas4: setPixel`, `scene::addObject x1`, `lua engine: init+shutdown`) are already distinct across files — no prefix needed.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Benchmark history storage + dashboard | Custom HTML/JS history tracker | github-action-benchmark@v1 | Handles time-series chart rendering, git commits, branch management, alert logic |
| Regression detection logic | Custom threshold comparison script | `fail-on-alert: true` + `alert-threshold` | Action already compares against last result on `bench-data` branch |
| JSON format conversion | Regex-based text manipulation | Python json module with field extraction | nanobench JSON is well-structured; direct field access is reliable |
| Branch creation in CI | Workflow step to `git checkout --orphan` | One-time manual setup before first run | Branch must pre-exist; creating in CI adds complexity without benefit |

**Key insight:** The `github-action-benchmark` action handles everything after the conversion step — git commits, branch pushes, chart generation, alert detection, PR comments. The only custom code needed is the 20-line Python conversion script.

---

## Common Pitfalls

### Pitfall 1: docs.yml deploy-pages wipes bench-data history
**What goes wrong:** If `gh-pages-branch: gh-pages` is used, the `actions/deploy-pages` step in `docs.yml` deploys an artifact that replaces the entire gh-pages branch content, deleting all benchmark data files.
**Why it happens:** `actions/deploy-pages` manages an artifact-based deployment that does a full replacement of the pages source, not a merge. It is incompatible with direct git pushes to the same branch.
**How to avoid:** Use `gh-pages-branch: bench-data` exclusively. The `bench-data` branch is a plain git branch that `docs.yml` never references.
**Warning signs:** Benchmark history disappears after a documentation deploy.

### Pitfall 2: Branch does not exist on first workflow run
**What goes wrong:** `auto-push: true` fails with a git error because `bench-data` does not exist as a remote branch.
**Why it happens:** The action pushes to an existing branch; it does not create branches.
**How to avoid:** Create the orphan branch manually before first CI run (see Pattern 3 above).
**Warning signs:** Workflow fails on the `Store benchmark result` step with `fatal: 'bench-data' does not appear to be a git repository`.

### Pitfall 3: PR regression check has no baseline to compare against
**What goes wrong:** On a PR, `save-data-file: false` + the action finds no prior data in `bench-data/dev/bench/data.js`. The action may skip comparison silently or produce an error.
**Why it happens:** At least one successful push-to-main run must have run and recorded data before the PR comparison job can find a baseline.
**How to avoid:** Merge a "seed" commit to main that records the first baseline before any regression-checking PRs are opened. The Phase plan should include an explicit baseline-seed step.
**Warning signs:** PR check completes without any comparison output or alert message.

### Pitfall 4: ubuntu-latest Lua not found
**What goes wrong:** `cmake` fails to find Lua during benchmark build because `liblua5.4-dev` is not installed by default on `ubuntu-latest`.
**Why it happens:** `build-bench.sh` uses `-DENJIN2_BUILD_LUA=ON`, which triggers `find_package(Lua)`. The CMakeLists.txt documents: `apt-get install liblua5.4-dev`.
**How to avoid:** Add `sudo apt-get install -y liblua5.4-dev` to the workflow's dependency step.
**Warning signs:** CMake FATAL_ERROR: `Lua was requested but could not be found`.

### Pitfall 5: cmake FetchContent downloads Lua source on every CI run
**What goes wrong:** If `ENJIN2_BUILD_WASM` or `ENJIN2_BUILD_ESP32` is accidentally ON, cmake tries to download Lua source from lua.org, wasting time or failing on network issues.
**Why it happens:** build-bench.sh sets `-DENJIN2_BUILD_SDL=OFF` but not explicit WASM/ESP32 guards.
**How to avoid:** The CMakeLists.txt already has `if(EMSCRIPTEN OR ESP32)` guards; ubuntu-latest sets neither. No action needed but worth verifying in CI logs.
**Warning signs:** cmake output shows `FetchContent: Downloading lua-5.4.8.tar.gz`.

### Pitfall 6: alert-threshold comparison fails for GC benchmark (high variance)
**What goes wrong:** `lua GC: full collect` and `lua engine: init+shutdown` have naturally high variance (GC timing is non-deterministic). These can trigger false alerts at 150%.
**Why it happens:** GC pauses are platform-dependent and fluctuate between CI runner instances.
**How to avoid:** Accept false alerts initially; the 150% threshold was chosen (STATE.md) precisely because of runner variance. After 30-50 runs, review which benchmarks are noisy and consider excluding them or raising their individual threshold. The action does not support per-benchmark thresholds — if needed, exclude GC benchmarks from combined JSON initially.
**Warning signs:** Repeated alerts on GC benchmarks across consecutive pushes with no code changes.

---

## Code Examples

Verified patterns from official sources:

### customSmallerIsBetter JSON format (verified from benchmark-action README)
```json
[
  {
    "name": "canvas4: setPixel",
    "unit": "ns/op",
    "value": 21.0,
    "range": "± 4.5%"
  },
  {
    "name": "canvas4: clear",
    "unit": "ns/op",
    "value": 530.0,
    "range": "± 2.0%",
    "extra": "epochs: 100"
  }
]
```
Source: https://github.com/marketplace/actions/continuous-benchmark (action inputs documentation)

### Minimal workflow step (verified against action.yml input names)
```yaml
- uses: benchmark-action/github-action-benchmark@v1
  with:
    name: enjin2 Benchmarks           # must be consistent across all runs
    tool: customSmallerIsBetter        # enables custom lower-is-better format
    output-file-path: bench-results/bench-combined.json
    gh-pages-branch: bench-data        # custom branch — NOT gh-pages
    benchmark-data-dir-path: dev/bench # path within bench-data branch
    github-token: ${{ secrets.GITHUB_TOKEN }}
    auto-push: true                    # push commit to bench-data
    alert-threshold: '150%'
    fail-on-alert: true
    comment-on-alert: true
```

### Actual nanobench fields in bench-results/*.json (verified locally)
```
Fields present in all three benchmark files:
  title, name, unit, batch, complexityN, epochs, clockResolution,
  clockResolutionMultiple, maxEpochTime, minEpochTime, minEpochIterations,
  epochIterations, warmup, relative, median(elapsed),
  medianAbsolutePercentError(elapsed), median(instructions),
  medianAbsolutePercentError(instructions), median(cpucycles),
  median(contextswitches), median(pagefaults), median(branchinstructions),
  median(branchmisses), totalTime, measurements

Conversion uses:
  median(elapsed)                        -> value (× 1e9 = nanoseconds)
  medianAbsolutePercentError(elapsed)    -> range (× 100 = percent string)
  name                                   -> name (use as-is; already unique)
```

### Complete list of benchmarks to be tracked (27 total, verified locally)
```
bench_canvas.json (8):
  canvas4: setPixel
  canvas4: clear
  canvas4: fillRect 32x32
  canvas4: drawCircle r16
  canvas4: blit 128x128 sprite
  canvas8: setPixel
  canvas8: fillRect 32x32
  compositor: composite 5 layers

bench_ecs.json (12):
  scene::addObject x1 / x8 / x16 / x32 / x48
  object::addComponent<C_Position>
  object::removeComponent<C_Position>
  scene::update x1 / x8 / x16 / x32 / x48 objects

bench_lua.json (7):
  lua engine: init+shutdown
  lua engine: executeString (noop script)
  lua binding: engine.time.delta call
  lua binding: math.clamp call
  lua proxy: find+field round-trip
  lua event: emit dispatch
  lua GC: full collect
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Deploy gh-pages with git push directly | `actions/deploy-pages` with artifact upload | 2022-2023 | Direct git push to gh-pages now conflicts with artifact-based deploy |
| `actions/checkout@v2` / `@v3` | `actions/checkout@v4` | 2023 | v2/v3 deprecated; v4 uses Node 20 |
| `actions/setup-node@v3` | `actions/setup-node@v4` | 2023 | v3 deprecated |
| `alert-threshold: '200%'` default | Start at 150% for tighter signal | Project decision | 200% allows 2x regressions to pass silently |

**Deprecated/outdated:**
- `actions/checkout@v2`, `@v3`: Deprecated; use `@v4`
- Direct git push to `gh-pages` when `actions/deploy-pages` is active: Incompatible deployment mechanisms

---

## Open Questions

1. **GitHub Pages serving from bench-data branch**
   - What we know: `github-action-benchmark` writes `index.html` + `data.js` to `bench-data/dev/bench/`. GitHub Pages can serve from a branch.
   - What's unclear: Whether GitHub Pages can be configured to serve from both `gh-pages` (Docusaurus) AND `bench-data` simultaneously. GitHub Pages only supports one source branch per repository.
   - Recommendation: The `bench-data` branch dashboard URL will NOT be publicly served by GitHub Pages unless Pages source is switched. For Phase 64, the dashboard is accessible by checking out the `bench-data` branch locally or navigating to the raw GitHub branch view. If a public dashboard URL is required (success criterion 3), the Docusaurus site could be extended to embed or link to the benchmark data — OR the benchmark data dir could be committed to the main docs source. This needs a decision before Wave 1.
   - Alternative: Use `benchmark-data-dir-path` pointing to a path inside the Docusaurus `docs/static/` directory and keep `gh-pages-branch: gh-pages`. This means benchmark data IS on gh-pages but in a subdirectory that Docusaurus's deploy-pages does NOT overwrite (because deploy-pages replaces with the built output, which would include the benchmark subdir if built into the Docusaurus static output). This is complex — the simpler answer is: the success criterion says "gh-pages performance dashboard URL is accessible" — this may mean the benchmark action's own generated dashboard served from the `bench-data` branch as GitHub Pages source.

2. **PR regression check without write access (fork PRs)**
   - What we know: Fork PRs don't have access to `secrets.GITHUB_TOKEN` for write operations.
   - What's unclear: Whether the `comment-on-alert` step works for fork PRs.
   - Recommendation: For Phase 64, restrict workflow to non-fork PRs (`if: github.event.pull_request.head.repo.full_name == github.repository`) or accept that fork PRs only get the `fail-on-alert` log output (not a comment). The success criterion only requires "alert visible in the GitHub Actions log" — `fail-on-alert: true` satisfies this without needing PR comments.

3. **cmake build time on ubuntu-latest**
   - What we know: build-bench.sh builds three binaries from scratch. nanobench is a single header (fast). Lua is system-installed (fast). ECS/Canvas are small C++ files.
   - What's unclear: Actual build time without caching. May be 2-5 minutes.
   - Recommendation: Add `actions/cache@v4` for the `build-bench/` CMake build directory keyed on `${{ runner.os }}-cmake-bench-${{ hashFiles('CMakeLists.txt', 'benchmarks/**', 'src/**', 'include/**') }}`. Add in Wave 1 if needed; start without it.

---

## Validation Architecture

> `workflow.nyquist_validation` is not set in `.planning/config.json` — treating as enabled.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | No automated test framework for workflow YAML — validation is functional (run the workflow) |
| Config file | `.github/workflows/benchmarks.yml` |
| Quick run command | `python3 scripts/convert-bench.py && python3 -c "import json; d=json.load(open('bench-results/bench-combined.json')); assert len(d)==27; print('OK')"` |
| Full suite command | Push to main branch; observe GitHub Actions run |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CI-01 | Workflow triggers on push/PR to src/** or include/** | smoke (manual) | Push commit touching `src/`; verify Actions tab shows run | ❌ Wave 0 |
| CI-02 | Conversion script produces valid customSmallerIsBetter JSON | unit | `python3 scripts/convert-bench.py && python3 -c "import json; d=json.load(open('bench-results/bench-combined.json')); assert len(d)==27; [assert set(['name','unit','value']).issubset(e) for e in d]; print('PASS')"` | ❌ Wave 0 |
| CI-03 | bench-data branch accumulates commits without touching gh-pages | manual | After first push: `git fetch origin bench-data && git log origin/bench-data` | ❌ Wave 0 (requires orphan branch creation) |
| CI-04 | Dashboard index.html present in bench-data/dev/bench/ | manual | `git fetch origin bench-data && git show origin/bench-data:dev/bench/index.html` | ❌ Wave 0 |
| CI-05 | PR regression alert visible in Actions log | smoke (manual) | Modify a benchmark to artificially inflate its value; open PR; verify workflow fails | ❌ Wave 0 |

### Sampling Rate
- **Per task commit:** `python3 scripts/convert-bench.py && python3 -c "import json; d=json.load(open('bench-results/bench-combined.json')); assert len(d)==27; print('OK')"`
- **Per wave merge:** Full workflow run on push to main
- **Phase gate:** All 5 success criteria manually verified before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `scripts/convert-bench.py` — conversion script (CI-02)
- [ ] `.github/workflows/benchmarks.yml` — workflow file (CI-01, CI-03, CI-04, CI-05)
- [ ] Orphan branch `bench-data` must exist on remote before first CI run (CI-03, CI-04)
- [ ] Decision on dashboard public URL (Open Question 1 above) — needed before CI-04 can be verified

---

## Sources

### Primary (HIGH confidence)
- https://github.com/marketplace/actions/continuous-benchmark — all action inputs verified (tool, gh-pages-branch, auto-push, fail-on-alert, alert-threshold, comment-on-alert, save-data-file, benchmark-data-dir-path)
- `bench-results/bench_canvas.json`, `bench_ecs.json`, `bench_lua.json` — nanobench JSON format verified locally; all 27 benchmark names enumerated
- `/home/unwn/git/enjin/.github/workflows/docs.yml` — existing docs workflow verified: uses `actions/deploy-pages@v4`; manages `github-pages` environment; no reference to `bench-data`
- `/home/unwn/git/enjin/.planning/STATE.md` — project decisions verified: `bench-data` branch decision, 150% threshold decision
- `/home/unwn/git/enjin/scripts/build-bench.sh` — build flags verified: `ENJIN2_BUILD_LUA=ON`, uses `build-bench/` directory
- `/home/unwn/git/enjin/CMakeLists.txt` — Lua dependency documented: `apt-get install liblua5.4-dev`

### Secondary (MEDIUM confidence)
- https://github.com/benchmark-action/github-action-benchmark — README confirms branch must pre-exist for auto-push; gh-pages-branch input customizable; orphan branch creation pattern documented
- WebSearch (multiple sources) — confirmed `cancel-in-progress: false` needed for data branch commits; `contents: write` permission required for auto-push

### Tertiary (LOW confidence)
- Open Question 1 (GitHub Pages single-source limitation) — inferred from GitHub Pages documentation behavior; not directly tested for this repo's Pages configuration

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — action inputs verified from official Marketplace page; existing workflow and CMake files read directly
- Architecture: HIGH — nanobench output format verified from actual local files; conversion logic validated via Python prototype
- Pitfalls: HIGH (pitfalls 1-4) / MEDIUM (pitfalls 5-6) — conflicts verified by reading docs.yml; Lua/cmake pitfalls inferred from CMakeLists.txt documentation

**Research date:** 2026-03-08
**Valid until:** 2026-06-08 (github-action-benchmark@v1 is stable; action inputs unlikely to change)
