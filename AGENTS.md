# Reactive — agent guide

Reactive is a C++17 GUI toolkit built on functional reactive programming (FRP).
Code lives under `bqui` (the UI toolkit) on top of `bq` (the reactive/signal
core). The repository is named `reactive` for historical reasons; `bq` is a nod
to the becquerel.

Read these when you need depth — they are the source of truth for *how the
system works*, not this file:

- [`docs/architecture.md`](docs/architecture.md) — the mental model.
- [`docs/conventions.md`](docs/conventions.md) — idioms and gotchas to know before editing.
- [`docs/style-guide.md`](docs/style-guide.md) — how to write code and docs here (the review rubric).
- [`docs/decisions.md`](docs/decisions.md) — why non-obvious choices were made.
- [`docs/reviewing.md`](docs/reviewing.md) — how the pre-merge review works (what to weight, known false-positives).

API reference is **Doxygen in the public headers**, not Markdown. Do not
transcribe signatures into any Markdown doc, and do not add comments in code
that point at these docs (they rot). Docs point at code; code never points at
docs.

## Libraries (dependency order: `btl → bq → ase → avg → bqui`)

- **`btl`** — foundational C++ utilities (option, variant, tuple helpers, cloning).
- **`bq`** — the reactive core: signals (`bq::signal`) and streams (`bq::stream`). No UI dependency.
- **`ase`** — low-level accelerated graphics and platform abstraction (OpenGL via GLX/WGL, plus a headless "dummy" backend).
- **`avg`** — vector graphics: paths, shapes, brushes/pens, text, the render tree, and animation primitives.
- **`bqui`** — the UI toolkit: widgets, modifiers, layout, shapes, theming, and the app/window loop.

Public headers live in `src/<lib>/include/<lib>/`. Only that directory is
exported to dependents, so `#include <lib/header.h>` compiles only when `<lib>`
is a declared dependency — a deliberate compile-time firewall (see
`docs/conventions.md`).

**Each library documents itself** in `src/<lib>/readme.md` (concepts and usage)
and `src/<lib>/AGENTS.md` (internals, entry points, traps). Read the relevant
library's `AGENTS.md` before working in it; those docs are co-located so they
travel with the library if it becomes its own repository. `docs/architecture.md`
indexes them.

## Build / test / run

Meson + Ninja. Dependencies (Eigen, FreeType, mapbox, gtest, tracy) are fetched
as Meson subprojects on first configure, so initial setup needs a network.
(If `meson` isn't on PATH, invoke it as `python -m mesonbuild.mesonmain`.)

```sh
meson setup build --buildtype=debug     # or release
meson compile -C build
meson test -C build                     # unit tests
```

CI drives the same builds through `lw` (loomworks), using the configuration sets
in `loomworks.json`: `lw profile create <set> <tool> --local`, then `lw build`
and `lw test`. `lw` establishes the compiler environment itself, so it sidesteps
the MSVC note below. Profiles are per-machine and deliberately uncommitted; see
`lw help` and `lw help ci`.

Platform notes that will bite you:

- **Windows configure needs the MSVC environment** — run from an "x64 Native
  Tools Command Prompt for VS", or import `vcvars64` into the shell first. To
  build with clang instead of MSVC, set `CC=clang-cl CXX=clang-cl` (the GNU
  `clang++` driver fails on the MSVC-style flags this project passes).
- **Each library builds into its own directory**, so the test/app executables
  can't find their sibling DLLs on PATH by themselves. Use `meson devenv -C
  build` (which puts them on PATH) to run, or add the per-target dirs manually
  (`src/ase`, `src/avg`, `src/bq`, `src/bqui`, and the freetype/tracy subproject
  dirs). Running an exe directly usually fails with a missing-DLL error.
- A `vswhere.exe ... not recognized` line during vcvars import is harmless.

## Workflow

`master` is protected: PRs only, the single `ci-success` status check must pass,
and admin bypass is enforced. Merge PRs as **merge commits** (`gh pr merge
--merge`); squash only when explicitly asked; rebase-merge is disabled. Keep
commits focused.

### Before merging a PR

Every PR must be reviewed and explicitly approved by a **human** before it is
merged. Agents must **never self-merge** — GitHub cannot enforce a review
sign-off here without a separate reviewer account, so this is a hard convention:
wait for human approval.

For substantive changes, run these before asking for that approval (scale them
down for trivial or docs-only PRs):

1. **Clean-context style review** — a *fresh* agent (no authoring context)
   checks the diff against `docs/style-guide.md`, `docs/conventions.md`, and
   `docs/reviewing.md` (what to weight and known false-positives).
2. **Clean-context correctness review** — a *fresh* agent reviews the diff for
   bugs and logic errors.
3. **Docs-freshness check** — if the change alters the model, a convention, or a
   recorded decision, update the relevant `docs/` file in the same PR.

The clean context matters: the author rationalises its own work, so an unbiased
reviewer with only the diff and the rubric catches drift the author misses.

After the human review, **capture any reusable lesson** in its home — a rule →
`docs/style-guide.md`/`conventions.md`; a review heuristic or false-positive →
`docs/reviewing.md` — so the next review applies it without the human repeating
themselves.

## Keeping these docs honest

They describe the *stable* model — the API is the headers' job. If you change
the model (the pipeline, the layering, a convention, a recorded decision),
update the relevant `docs/` file in the same change. Each doc carries a
"verified against" stamp so a reader knows when to trust it versus re-check
against code.
