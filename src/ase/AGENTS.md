# ase — agent notes

*Last verified against `6b21821` (2026-08-18); the texture note against
`f3ed6e5` (2026-10-22); the shader interface section against `3c3dfb34`
(2026-11-12); the WGL surface and the format notes against `ede84a6b`
(2026-11-11); the command buffer section against `1da30d6a` (2026-11-11).*

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
- The headless (dummy) backend is compiled on every platform (`dummysrcs` in
  `src/ase/meson.build`), so any test or app can select it via
  `ase::makeDummyPlatform(loop)` without opening an OS window. `makeDefaultPlatform(loop)`
  returns the native backend where one exists (GLX/WGL) and the dummy otherwise;
  a dependent that must avoid opening a real window (e.g. `apptest` in
  `src/bqui/meson.build`) keys off the target OS.
- **WGL surface and context.** `WglPlatform` picks one pixel format at
  construction and `setPixelFormat(dc)` applies it to the dummy DC and every
  window: the shared contexts are created against the dummy DC, so the two
  must agree. It bootstraps a legacy context on a throwaway window to load
  `wglChoosePixelFormatARB` and `wglCreateContextAttribsARB`, then asks for a
  double-buffered RGBA8 format with 24/8 depth/stencil and
  `WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB` (GLX asks for
  `GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT`); when the entry point is missing or no
  such format exists it keeps the `ChoosePixelFormat` result and says so on
  `DBG`. Contexts request 3.3 core and fall back, with a `DBG` line, to a
  legacy `wglCreateContext` context.
  `GlRenderState` enables `GL_FRAMEBUFFER_SRGB` on every GL backend that
  reports `GL_ARB_framebuffer_sRGB`; the enable only converts on an
  sRGB-capable surface, which is why the pixel format matters. Nothing checks the chosen format at run time: verifying it
  takes a manual run on a machine with a GL driver.
- **The platform does not own its run loop.** The `btl::RunLoop` is created by
  the caller (bqui's `App`, a test) and injected at construction; `PlatformBase`
  holds it by reference and `runLoop()` returns it. The loop must outlive the
  platform, so callers keep it as a named local declared before the platform
  (`RunLoop` is non-movable, so a plain local is the natural home). `App`
  registers its as the process default (`RunLoop::makeDefault()`) so socket IO
  can reach it via `RunLoop::getDefault()`.
- **The frame loop is context-free and lives on the platform.** `Platform::run`
  (`PlatformBase::run`, one shared body in `src/platformbase.cpp`) drives frames
  on the injected loop: per dirty window it gates — without blocking — on that
  window's own `canAcquire` backpressure, skipping a saturated window (which a
  freed slot later wakes back through `requestFrame`), renders and presents
  through the context the window carries, and fences on the window's own queue —
  so `run` names no `RenderContext` of its own.
  A backend supplies only what differs through protected virtuals: the static
  cadence via `runConfig()` (`PlatformBase::RunConfig`: just `frameStep`; the
  frame *rate* is per-window — the loop schedules to the earliest of the windows'
  `nextFrameTime`, not a platform cap — and the dummy queue completes fences
  inline), the OS `wakeSource()` (read
  once at loop start), and the live `getRenderWindows()` list (re-read every tick,
  since windows open and close during a run); there is no `Session`. Manual
  driving is a `Platform::pause()` RAII token (`PauseToken`) whose `step(dt)`
  produces one frame off the same callback/render path an auto tick takes;
  "paused" suspends auto-cadence frame *production*, not the loop (events/IO keep
  pumping), and dropping the token resumes the cadence. `step` lives only on the
  token, so a free-running loop cannot be stepped by construction.
- **`PlatformImpl` is a pure interface; `PlatformBase` holds the shared guts.**
  Mirroring the `WindowImpl`/`WindowBase` split below: `PlatformImpl`
  (`include/ase/platformimpl.h`) is only the public platform virtuals the
  `Platform` handle and its `PauseToken` call (`makeWindow`, `makeRenderContext`,
  `run`, `pauseFrames`/`resumeFrames`/`stepFrame`, `requestFrame`, `runLoop`) plus
  the `getImplOfType` type-erasure plumbing. Every backend derives instead from
  `PlatformBase` (`include/ase/platformbase.h`, `src/platformbase.cpp`), which
  owns the injected `loop_`, the shared frame-loop body, the pause/step state, and
  the loop-contract *protected* virtuals a backend fills in (`handleEvents`,
  `runConfig`, `wakeSource`, `getRenderWindows`) — none of which are on the public
  interface. `RunConfig` moved here too (`PlatformBase::RunConfig`).
- **`WindowImpl` is a pure interface; `WindowBase` holds the shared guts.**
  `WindowImpl` (`include/ase/windowimpl.h`) is only the public window virtuals plus
  the `getImplOfType` type-erasure plumbing. Every backend window instead derives
  from `WindowBase` (`include/ase/windowbase.h`, `src/windowbase.cpp`), which owns
  the co-owned `RenderContext`, the per-window present backpressure (the
  non-blocking `canAcquire`/`submitFrameFence` + the `WindowPresentSync` fence
  bookkeeping), the loop-contract members (the `frame` private virtual and the
  concrete `nextFrameTime`, both reached through `friend class PlatformBase`), and
  the `GenericWindow genericWindow_` (protected, so backends
  reach it during OS-event translation). The callback setters and event injectors
  that just forward to `genericWindow_` — plus `getSize`/`getScalingFactor` — are
  concrete forwarders on `WindowBase`; a backend overrides only what genuinely
  differs (`present`, framebuffer, visibility, scaling-aware title/requestFrame,
  `frame`). `nextFrameTime` reads `genericWindow_` on `WindowBase` itself, so
  backends do not reimplement it. The `WindowBase` ctor takes `(context, size,
  scalingFactor)` so it can build `genericWindow_`. The platform render lists are
  `weak_ptr<WindowBase>` since the loop hooks live there. `getRenderContext` is
  a pure virtual on `WindowImpl` (implemented by `WindowBase`), so `Window`
  reaches the render context without a downcast; the window's main render queue
  is just `getRenderContext().getMainRenderQueue()`, so `Window::getMainRenderQueue`
  is a convenience computed on that and stays off the impl interface.
- **Offscreen ≠ dummy.** `Platform::makeWindow(context, size, headless)` with
  `headless` gives an `OffscreenWindow` (backend-agnostic,
  `src/offscreenwindow.cpp`): the *real* backend rendering into an FBO built from
  the `RenderContext`, shown nowhere. This is how the real GLX/WGL backend runs
  headless, distinct from the no-GL dummy backend; the one entry point makes an
  on-screen window when `headless` is false. The real GLX/WGL backends keep **one
  render list** of all their windows (real and offscreen) — the render-polling
  surface the platform's frame loop drives (`nextFrameTime()` for when a window is
  due, and the `frame()` private virtual), reached through `friend class
  PlatformBase` and not part of the public window API — which the loop draws
  uniformly. The **dummy
  backend registers its windows on the same render list** and is driven by the one
  loop like the real backends; its `frame()` runs (evaluating the signal graph and
  building the render tree), only the draw and present are no-ops on the dummy
  queue. **Event routing is separate and backend-specific**
  (WGL's HWND→window map for `wndProc`, GLX's X windows) and holds real windows
  only: a real window is in both, an offscreen window is in the render list
  alone. `present` is a **surface operation, not a render command**: the
  backend-agnostic, status-returning `WindowImpl::present()` virtual each window
  type implements. A GL window sequences its own swap by enqueuing it on the
  same GL dispatcher its draws went through (reached via its own
  `getRenderContext().getMainRenderQueue().getImpl<GlRenderQueue>().dispatch(...)`), so the swap
  runs FIFO after the submitted draws on the render thread; `present()` returns
  immediately without blocking. The swap body — the only place the GL-only
  `Dispatched` tag legitimately appears — is what GlxWindow (swaps its GLX
  drawable with the X lock held inside) and WglWindow (`SwapBuffers`es its
  `hdc_`) run there; OffscreenWindow and DummyWindow are the no-op a future
  pixel readback hooks into and name no GL types. Because the dispatched swap
  runs after `present()` returns, the GL windows capture a `shared_from_this()`
  keep-alive into the task so the window outlives it. `PresentStatus` is
  `Ok`-only on GL; it exists so a backend whose present can fail (a lost
  swapchain) is not a `void`-return retrofit.
- **Textures and uploads.** `FORMAT_R8` is a linear single-channel 8-bit
  format (avg's glyph coverage); the GL context sets `GL_UNPACK_ALIGNMENT` to 1
  once at setup so its odd-width rows upload tightly packed.
  `CommandBuffer::pushSubUpload` replaces a rectangle of an existing texture
  (`glTexSubImage2D`). Both uploads are non-draw
  commands, so they end a run of draws (see "Command buffer and draw
  order") and keep their place relative to the draws around them.
  Uploads bind on texture unit 0 and record
  the binding there, because the draw path compares each texture against its
  own unit's recorded binding; an upload on another unit leaves that record
  stale and a later draw skips a bind it needed.
- **Formats.** `format.h` has four colour formats and five depth formats.
  Colour formats make textures and are all colour-renderable under a core
  profile (`src/gl/glformat.h` maps them to sized internal formats):
  `FORMAT_SRGBA` -> `GL_SRGB8_ALPHA8`, `FORMAT_SRGB` -> `GL_SRGB8`,
  `FORMAT_R8` -> `GL_R8` (all uploaded as `GL_UNSIGNED_BYTE`), and
  `FORMAT_RGBA16F` -> `GL_RGBA16F` (uploaded as `GL_HALF_FLOAT`, linear,
  8 bytes a pixel). Depth formats make renderbuffers only: `FORMAT_DEPTH16`,
  `_DEPTH24`, `_DEPTH32`, `_DEPTH32F` attach at `GL_DEPTH_ATTACHMENT` and
  `FORMAT_DEPTH24_STENCIL8` at `GL_DEPTH_STENCIL_ATTACHMENT`; `GlFramebuffer`
  has no separate stencil target. `getBytes` is the upload pixel size the
  buffer checks use. Every switch over `Format` lists each enumerator with no
  `default`, so adding a format fails to compile until it is placed in each
  of them; the dummy backend accepts any format, so `test/framebuffertest.cpp`
  covers only the public contract, not the GL mapping.

- The root `meson.build` adds MSVC-style flags (`/wd4251`, `/bigobj`, `/UNICODE`)
  for any Windows build; those assume an MSVC-compatible driver, which is why
  clang must be `clang-cl`, not `clang++` (the build notes in the repo-root
  `AGENTS.md` cover this).

## Command buffer and draw order

A `CommandBuffer` (`commandbuffer.h`) is a vector of `RenderCommand`
(`rendercommand.h`), a variant of `DrawCommand`, `ClearCommand`,
`FenceCommand`, `BarrierCommand`, `BufferUploadCommand`,
`TextureUploadCommand` and `TextureSubUploadCommand`. A queue takes the
buffer whole on `submit`; the dummy queue draws nothing, completes fences
inline and, with `recordSubmissions(true)`, keeps the buffers for a test to
read back (`test/commandbuffertest.cpp`, avg's `test/renderingtest.cpp`).

- **Runs and the fence rule.** The GL queue reorders draws for state
  batching, but only within a *run*: the maximal sequence of consecutive
  depth-tested `DrawCommand`s. Every other command ends the run before it,
  so a clear, fence or upload keeps its place relative to the draws on
  either side. So does a draw whose pipeline has `depthTest = false`: depth
  cannot settle it against its neighbours, so it stays in submission order.
  `pushBarrier()` adds the command whose only effect is that split: the GL
  loop skips it without a GL call, since a framebuffer write is already
  visible to the draws submitted after it. A draw that samples a texture an
  earlier draw in the same frame rendered needs a barrier between them, or
  the sort may swap them. The splitting is `sortDrawRuns` in
  `commandbuffer.h`, a template the GL state instantiates with
  `compareCommand` (`src/gl/glrenderstate.cpp`) and the test with a z
  comparator. It stable-sorts a permutation of iterators and moves the
  commands into place: MSVC's `std::stable_sort` refuses a temporary buffer
  of the 16-byte-aligned `RenderCommand`, and the stability means draws with
  equal keys stay in submission order.
- **The sort key.** `compareCommand` splits a run into draws that may go in
  any order (opaque, depth-tested *and* depth-writing: depth settles them)
  and the rest (depth-tested but blended or not depth-writing), any-order
  first. Any-order draws then sort by framebuffer,
  textures, program, vertex buffer, index buffer, uniform set and
  z. The rest sort by framebuffer, then z descending (the painter assigns a
  lower z to a later-painted element, so this is paint order), then the same
  state keys. The key compares
  handles and integers only; z is the one float, and only as an ordering.
- **Pipeline state.** `makePipeline(program, spec, PipelineDesc)`
  (`pipelinedesc.h`) is the one creation path; `makePipelineWithBlend` is a
  wrapper that sets `blend`. `PipelineDesc` holds an optional `BlendFunc`,
  `depthTest` (default on) and `depthWrite`, which left unset resolves
  (`writesDepth()`) to on for an opaque and off for a blended pipeline, the
  behaviour the GL state used to derive from the blend flag. Every backend's
  pipeline reports `isBlendEnabled`, `isDepthTestEnabled` and
  `isDepthWriteEnabled`; the dummy stores the desc. `GlRenderState` tracks
  depth test, depth mask and blend enable as three separate booleans
  initialised to the GL defaults it sets up (`GL_DEPTH_TEST` on, mask true,
  blend off), switches each when the pipeline differs, and compares the
  blend factors on every blended draw rather than only when blending turns
  on. A clear with the depth bit forces the mask back on first, since
  `glClear` obeys it, and the next draw restores its pipeline's mask.
- **What CI does not run.** All of the above compiles on every leg and the
  dummy tests cover the command shapes, the run splitting and the pipeline
  flags, but no GL context runs in CI: the depth switches in
  `glrenderstate.cpp` have only been compiled. A change there needs a
  manual run of an app or `bquiheadlesstest` on a machine with a driver.

## Shader interface and uniforms

The contract between a program and the C++ that feeds it is a
`ShaderInterface` (`shaderinterface.h`) the caller declares: vertex attributes
with their locations, uniform blocks with their bindings and std140 members,
samplers with their units. `docs/design/shader-interface.md` is the design;
`docs/decisions.md` records why it is declared rather than introspected.

- **Shaders are bytes in a declared language.**
  `RenderContext::getShaderLanguages` lists what a backend compiles (GL:
  GLSL 330; dummy: everything) and `makeShader` takes a stage, a language
  and a `Buffer`, so a caller picks its variant from the list instead of
  assuming GLSL. `makeProgram` takes the two shaders and the interface.
- **GL applies the declaration at link; reflection only validates.** The GL
  backend sets attribute locations before linking and the block bindings and
  sampler units after it, straight from the declaration, so GLSL 330 source
  needs no `layout(binding = N)` and no 420pack extension. In debug builds it
  then reflects the linked program and compares: a declared name the shader
  lacks, or a location, binding, unit or std140 offset that differs, fails
  loudly instead of rendering garbage. Release builds skip the comparison.
  All of it runs on the background context, whose `glFlush` must come last:
  the main context is not guaranteed to see program state set after it.
  The dummy backend stores the declaration and reflects nothing, so headless
  tests see the same resolved vertex specs as GL (`Pipeline::getVertexSpec`).
- **Block contents come from one layout computation.** `UniformBlock<T>`
  (`uniformblock.h`) mirrors a block as a C++ struct and checks the struct's
  size and each `blockMember`'s offset against the computed std140 layout at
  construction; `UniformValues` (`uniformvalues.h`) is a by-name shadow of a
  block for parameters that arrive as names (the effect scheduler). Both
  give the same bytes for the same block.
- **Per-frame storage is a `UniformRing`** (`uniformring.h`): one
  `UniformBuffer` per frame in flight, `beginFrame`, `write` a block to the
  next slot aligned to `getUniformBufferOffsetAlignment` (GL reads it from
  the context, dummy fixes 256), `endFrame` pushes the frame's one upload
  into a command buffer the owner submits *before* the frame's draws. A
  constant change is therefore not a `pushUpload` fence in the draw stream.
- **`UniformSet` knows its interface.** It is made by `makeUniformSet(iface)`,
  binds a range by block name or declared binding, throws
  `std::invalid_argument` for an undeclared block or a range smaller than the
  block, and a rebind overwrites (the GL set used to insert and keep the
  first). `GlRenderState` skips re-binding the ranges of the set it bound
  last, keyed on the set and its version, which every bind bumps; a cache on
  the pointer alone would keep a rebound set's old ranges. The dummy set
  records its bindings for tests.
- Textures stay positional on the draw: index i binds unit i, and the
  declaration says which sampler lives there.
