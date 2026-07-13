# ase

`ase` ("accelerated simple engine") is the low-level graphics and platform layer:
it opens windows, manages a GPU render context, and rasterises the drawings
produced by `avg`.

Rendering is OpenGL, with per-platform windowing — **GLX** on Linux and **WGL**
on Windows — plus a headless **dummy** backend used by tests. The rest of the
project talks to it through a platform-neutral abstraction, so the UI toolkit
does not depend on the windowing system directly.

See `AGENTS.md` for the backend layout and the platform traps.
