# ase — agent notes

*Last verified against `916c25b` (2026-07-21).*

The GPU + platform layer. Concepts are in `readme.md`; project-wide conventions
are in the top-level `docs/`.

## Layout

- `platform.h` — the platform-neutral abstraction (window, render context).
- `src/gl/` — the shared OpenGL implementation.
- `src/glx/`, `src/windows/` — the Linux (GLX) and Windows (WGL) bindings;
  `wglrendercontext`/`wglwindow` are the Windows rendering path.
- `src/dummy/` — the headless backend for tests.

## Notes

- The Windows GPU/rendering path lives under `src/windows/` and `src/gl/` — this
  is where to look for Windows rendering performance or context-selection issues
  (integrated-vs-discrete GPU, vsync).
- A backend owns its window's lifetime in the `WindowImpl` destructor —
  `GlxWindow` destroys its X window there and `WglWindow` its `HWND`. Leaving
  that out does not fail a test: nothing closes a window while the app runs
  unless the window list says to, so the symptom is a dead window left on
  screen. The WGL backend was missing it until a window list became reactive.
- **Unfixed:** destroying a WGL window mid-run leaves two loose ends that were
  unreachable while windows lived for the whole process, and became reachable
  when the window list did. `WglDispatchedContext` caches the last DC it made
  current and nothing invalidates that entry, so a recycled `HDC` can make it
  skip a `wglMakeCurrent` it needed; and with no window left the run loop keeps
  submitting fences against a context current on a destroyed DC. Releasing the
  context from the window's destructor would close both, and has to be
  dispatched to the render thread.
- `src/ase/meson.build` sets `ase_is_headless`, which is how a dependent decides
  whether a test may open a window (`src/bqui/meson.build` uses it).
- The root `meson.build` adds MSVC-style flags (`/wd4251`, `/bigobj`, `/UNICODE`)
  for any Windows build; those assume an MSVC-compatible driver, which is why
  clang must be `clang-cl`, not `clang++` (the build notes in the repo-root
  `AGENTS.md` cover this).
