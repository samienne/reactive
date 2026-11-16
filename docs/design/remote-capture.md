# Design: screenshots over the remote protocol

> **Status: proposed, nothing implemented.** Written against master `4d00d3fd`
> (2026-11-16). The decisions under *Decided* were settled with the owner and are
> recorded, not reopened; everything else is this note's proposal, and the
> choices it could not settle are listed under *Open questions for the owner*.
> The API contract will be Doxygen in the headers once the PRs land.

## Purpose

A remote client can read a window's widget tree and render tree but cannot see
its pixels. This note adds `window.screenshot` to the inspector protocol
(`src/bqui/src/remote/`), and the five enablers it needs first.

Three consumers shape it:

- **A coding agent** through the MCP bridge (`tools/mcp/bqui_mcp.py`), which
  wants small scaled or cropped images.
- **A Robot Framework library** (future) speaking the same protocol headlessly
  and saving images on the client side.
- **A developer inspector application** (future) showing what is on screen:
  repeated full-size captures, a few per second at most.

## Decided

- **Image bytes travel over the wire.** There is no "app writes a file to a
  path" option.
- **Bulk data goes through a general blob facility**, not a screenshot-specific
  one: a producing method replies with a descriptor (id, size, content type),
  the client pulls with `blob.read(id, offset, length)` and ends with
  `blob.release(id)`. Pulling gives flow control and lets other requests
  interleave. Blobs die on release or disconnect, under a cap on total
  outstanding bytes. A chunk is base64 inside the JSON reply for now; the chunk
  encoding is a detail behind `blob.read`, so a binary frame kind can replace it.
  `window.introspect` and `window.renderTree` may move to blobs later.
- **PNG, encoded in the app off the main thread.** zlib with a hand-written PNG
  container is preferred over libpng (see *Encoding*).
- **Capability is per window**: a `canCapture` field in `window.list`, and a
  defined error when a window that cannot capture is asked to.
- **Enablers land as separate PRs off master, in order**: non-blocking writes,
  deferred replies, blob transfer, pixel readback in ase, per-window capability
  reporting; then the feature.
- **Readback is designed against master as it is**, not on the unmerged backdrop
  stack (see *If the backdrop stack lands first*).

## Where master stands

- Frames are a 4-byte big-endian length plus payload (`streamtransport.cpp`).
  Transports are a named pipe, a Unix socket and TCP; the app connects out.
- **Writes block the loop thread.** `StreamTransport::send` runs two `writeAll`
  loops per frame; reads are a `RunLoop` readable source (`remotedriver.cpp`).
- **A handler is a function from params to result**, held in a private registry
  and answered before the next frame is read (`remotedriver.cpp:62`, `:396`).
- **Nothing reads pixels back.** `OffscreenWindow::present()` is a no-op;
  `avg::Painter::paintToImage` renders to a texture nobody reads.
- `inspectorapp` runs on the dummy platform (`src/inspectorapp/main.cpp:74`),
  which draws nothing.

Three things found while reading, all of which the enablers must fix:

- **TCP sends can fail spuriously on Windows.** The run-loop read path flips the
  socket to non-blocking (`tcptransport.cpp:176-183`), after which the blocking
  `writeAll` (`tcptransport.cpp:104-126`) treats `WSAEWOULDBLOCK` as an error;
  the driver takes any send error as the end of the session
  (`remotedriver.cpp:282-293`). A reply larger than the free socket buffer ends
  the session. The bridge uses TCP, so this is its live path. Read from code,
  not reproduced.
- **`addWritable` has never been used**, and on Win32 it cannot coexist with a
  readable source on the same socket: each registration calls `WSAEventSelect`
  with its own event (`src/btl/src/win32/runloopimpl.cpp:211-221`), and a second
  call replaces the first's association. `FD_WRITE` is also edge-reported, where
  the `RunLoop` contract is level-triggered.
- **A stepped frame can silently not render on a real backend.** A step renders
  only windows that are due and have a free in-flight slot
  (`src/ase/src/platformbase.cpp:177`; budget 2, `windowbase.cpp:21`), and while
  paused the slot-freed wake is dropped (`platformbase.cpp:81`). The dummy queue
  completes fences inline, so no test sees it. A screenshot must not inherit it.

## Enabler 1: non-blocking writes

**The queue lives in `StreamTransport`**, beside the existing receive buffer:
whole frames (prefix included) in FIFO order plus an offset into the head.
`send` becomes "append, then write what the handle takes now" and never blocks.
The transport gains the write-side twins of `nativeHandle()` and
`receiveReady()`: a handle to wait on for writability, and a flush that writes
until the handle would block and reports whether the queue is empty. The driver
owns the `RunLoop` writable source and **holds it only while the queue is
non-empty** (readiness is level-triggered, so an idle writable socket would spin
the loop).

Per transport:

- **Unix socket / TCP:** non-blocking `send`; a short write advances the offset,
  would-block stops the flush. TCP on Windows is already non-blocking; POSIX
  sockets are made `O_NONBLOCK` up front, because macOS `send` ignores
  `MSG_DONTWAIT`, and the blocking paths wait with `poll` on would-block.
- **Windows pipe:** overlapped writes are completion-based, so "writable" means
  "the pending write finished". The flush issues one overlapped `WriteFile` on
  the queue head; if it pends, the transport marks it pending and the writable
  handle is the write's event, exactly as the read side exposes its read event.
  The next flush reaps it without waiting and issues the next. The head buffer
  must not move while a write is pending, the event is reset once the queue
  drains, and the destructor cancels and reaps a pending write as it does a read.
- **btl:** the Win32 loop must keep one event per socket and merge the interest
  of its sources, and must re-arm writable after a callback that did not hit
  would-block, to honour the level-triggered contract. `runlooptest` gets its
  first writable tests.

**Ordering:** one queue, whole frames. A notification is enqueued where it is
emitted, so replies and notifications reach the wire in emission order and never
interleave bytewise.

**Backpressure.** Replies are solicited, so a client that stops reading is
throttled rather than cut off:

- Above a high-water mark of queued bytes the driver **stops dispatching
  requests** (and, past a small backlog of undispatched frames, drops its
  readable source so the kernel pushes back on the client). It resumes below a
  low-water mark. The queue is then bounded by the mark plus one reply.
- `frame` notifications are the only unsolicited traffic. One is **dropped**
  when the queue is above the low-water mark; the index it carries lets a client
  see the gap.
- No hard cap that disconnects: a client stopped in a debugger is legitimate.
- Incoming frames get a maximum length; today the prefix is trusted
  (`streamtransport.cpp:54`).

The blocking `receive()`/`send()` pair stays for the tests' client side.

## Enabler 2: deferred replies

**A handler gets its params and a responder.** It either returns a result, as
every handler does today, or keeps the responder and returns "pending". The
responder resolves once, with a result or an error:

- It is completed **on the loop thread only**; work finished elsewhere posts
  back first, so the driver stays single-threaded.
- It refers to the session weakly. Completing after the session ended is a
  no-op.
- Dropping it unresolved sends an internal error, so a lost handler cannot hang
  a client. A request without an id gets a responder that discards.

**Ordering is guaranteed for effects, not for replies.** Requests are dispatched
in arrival order and a handler's synchronous part finishes before the next is
dispatched. A deferred handler whose *effect* is not finished may hold a
**dispatch gate**: later frames are buffered until it releases. `app.step` holds
it until its last frame is rendered; `window.screenshot` holds it until its
frame is rendered and the readback is submitted, then lets go while pixels are
read and encoded. Replies can therefore overtake each other; ids correlate them,
which the Python client already does (`bqui_mcp.py:132-135`).

**Session end.** On disconnect or `app.shutdown` the loop stops as today.
Pending responders go inert, queued-but-undispatched frames are discarded, and
blobs are freed. The driver's destructor waits for its in-flight worker jobs (a
bounded number of PNG encodes), so nothing posts to a loop that is being torn
down; a late render-thread callback finds the session gone.

**`app.step` becomes the first deferred method.** To fix the skipped-frame
problem, a pause-token holder must learn that a stepped frame left a due window
unrendered and be woken when a slot frees, instead of the wake being dropped.
The driver then renders that window before stepping on, and replies after the
last frame. On the dummy backend fences complete inline, so the path stays
synchronous and existing tests keep passing.

## Enabler 3: blob transfer

**A blob is an immutable byte string owned by the session**, stored once and
read by range. The store is part of the driver; producers hand it bytes and a
content type and get back the descriptor they put in their reply.

- **Ids** are integers from a per-session counter, never reused in a session.
  Clients treat them as opaque.
- **`blob.read`** takes the id, an offset and an optional length, and returns
  the bytes (base64), the encoding name, and whether the end was reached. Reads
  may repeat or overlap; a read at the end returns nothing with the end flag set.
- **`blob.release`** frees one blob. Releasing an unknown id is an error, so a
  double release is visible.
- **Limits:** a cap on total outstanding bytes (proposed 256 MiB) and a maximum
  read length (proposed 1 MiB of raw bytes; default 256 KiB when omitted). A
  longer request is clamped, not refused. A chunk is one reply frame, so the
  maximum read also bounds what the write queue holds per request.
- **Errors**, in the JSON-RPC server range: `-32001` unknown blob (never
  existed, released, or from an earlier session); `-32002` outstanding-bytes cap
  reached, raised by the *producing* method with the bytes in use and the cap in
  the error data. A bad offset or length is the standard invalid-params.

**`system.describe` grows two things**: a per-method marker that a method's
result carries a blob descriptor, and a top-level limits object (read maximum
and default, outstanding cap, chunk encodings). A generic client can then pull
any blob-returning method without knowing it by name, and the bridge hides the
`blob.*` plumbing from its generated tools.

## Enabler 4: pixel readback in ase

**Readback is a render command, like a fence.** `CommandBuffer` gains a push
that reads a framebuffer's colour and completes a callback with the pixels. It
runs FIFO behind the draws already submitted, on whatever thread the backend
executes commands, and the callback may fire on any thread (the fence rule).
This keeps it backend-neutral: the earlier decision against a generic
"dispatch a task to the render thread" (`docs/decisions.md`, *Frame cadence is
per-window*) stands, because a command-buffer backend implements this as a copy
to a buffer.

- **GL:** bind the framebuffer for reading and call `glReadPixels` synchronously
  on the dispatcher thread. That stalls the render thread until the preceding
  draws finish, never the loop thread. **A PBO with a fence is not worth it
  yet**: it only un-stalls the render thread, at the price of a two-phase state
  machine, and the target is a few captures per second.
- **Dummy:** completes the callback at once with "unsupported". The render
  context reports whether it can read back; that is what capability reporting
  asks.

**The pixel contract** is fixed at the ase boundary so no consumer guesses:

- Tightly packed 8-bit RGBA, **top row first**. GL returns the bottom row first;
  the backend flips.
- **Bytes as stored.** The offscreen target is an sRGB texture
  (`src/ase/src/offscreenwindow.cpp:10`) rendered with sRGB framebuffer writes
  on, so the stored bytes are already sRGB-encoded: what a PNG wants, no
  conversion.
- **Alpha is premultiplied and not meaningful.** The painter blends
  premultiplied (`src/avg/src/painter.cpp:68-69`) over an opaque clear. Consumers
  ignore alpha; the screenshot is written as opaque RGB.
- The size is the framebuffer's pixel size: logical size times scaling factor.

**The window owns "capture the next frame I present".** A one-shot request on
the window (shared in `WindowBase`) holds a callback; the window's `present()`
consumes it by submitting the readback command for its own framebuffer. For
`OffscreenWindow` that is all `present()` does, which is the
present-as-readback `render-architecture.md` anticipated. It always reads the
whole framebuffer; crop and scale happen later, on the CPU. Destroying a window
with a request armed completes it with a failure.

**An on-screen window fits the same hook**: its `present()` submits the readback
against its default framebuffer *before* it enqueues the swap, so the order on
the render thread is draws, readback, swap. Caveats: obscured or off-screen
regions of a default framebuffer are undefined, and its bytes are whatever the
surface format made them. It is real-GL-only and CI-dark, so it is a follow-up
PR, not part of enabler 4.

**Reaching bqui.** `RemoteWindow` gains a capture operation and a capability
query; `WindowBridge` implements them over its ase window. The driver never
names ase types beyond what the seam already carries.

## Enabler 5: capability reporting

Each `window.list` entry gains `canCapture`. It is true when the window type
hooks its present *and* its render context can read back, so an
`OffscreenWindow` on the dummy context (which `DummyPlatform::makeWindow` does
create) reports false. Capturing such a window fails with `-32010` capture
unsupported, carrying the reason. The check is made at request time as well; the
flag is a hint, not a lease.

## The feature: `window.screenshot`

### Which frame

A screenshot shows **a frame rendered for it**, never a stale surface:

- **Paused (the client owns the clock):** the handler arms the capture, marks
  the window due, and the driver renders one frame with zero elapsed time
  through the same pause-token step every other frame takes. Animation time does
  not move and the frame index does not advance, but **input injected since the
  last step is applied**, as it would be on the next step. If the window has no
  free slot the capture stays armed and the driver retries on the slot-freed
  wake (enabler 2).
- **Running:** the capture is armed and the next frame the loop renders serves
  it. No extra frame is produced.
- **A mode change while armed:** a step consumes the capture with its first
  rendered frame; entering paused with one armed triggers the zero-time frame.
  A capture is never left waiting for a frame nobody will produce.

Several captures armed on one window before a frame are all served by that frame
from one readback. At most a few captures may be in flight per session; beyond
that the request fails with `-32011` capture busy, since each holds a full raw
frame.

### Parameters and reply

- **`window`**, as elsewhere.
- **`region`** (x, y, width, height) *or* **`widget`** (an introspection id),
  not both. Both are in **window-space logical units**, the coordinates
  introspection obbs and `window.inject` already use. A widget resolves to the
  axis-aligned bounds of its obb in the introspection of the captured frame. The
  rectangle is clamped to the window; empty after clamping, or an unknown
  widget, is invalid-params.
- **`scale`**, in (0, 1], relative to physical pixels; default 1. Downscaling is
  an area average in linear light.

The reply is a blob descriptor (`image/png`) plus what a client needs to map
image pixels back to window coordinates for `window.inject`: the image size in
pixels, the logical rectangle actually captured, the image pixels per logical
unit (scaling factor times scale), and the frame index.

### Encoding

The render-thread callback hands the raw frame to a `btl` thread-pool job, which
crops, scales, drops alpha, encodes, and posts the bytes to the loop thread,
where the blob is stored and the responder resolved.

**zlib plus a hand-written container, not libpng.** Writing 8-bit truecolour PNG
is a small, closed problem: signature, header, an sRGB chunk, deflated filtered
rows, end chunk, with zlib supplying deflate and the CRC. That is a couple of
hundred lines we fully own, and one small C dependency. libpng would add a second
subproject (which needs zlib anyway), a `setjmp` error model that sits badly in
C++, and a large reading and format surface we would never call. What we give up
is libpng's tuned filter selection and its decoder; a per-row filter heuristic
recovers most of the former and nothing here decodes. The encoder is a private
bqui source next to the driver. FreeType keeps its own zlib and png options off.

### The bridge, the client module and inspectorapp

- **Python client module.** The connection class in `bqui_mcp.py` (listen,
  launch, framing, id correlation, notifications) moves to a reusable module
  with no MCP dependency, and gains a helper that pulls a blob to bytes and
  releases it even on error. The bridge and the future Robot library both build
  on it. This PR is independent of the C++ work and can land first.
- **Bridge.** Tools stay generated from `system.describe`; today every result is
  returned as text (`bqui_mcp.py:214`). For a blob-returning method the bridge
  pulls the blob and returns image content for an `image/*` type (text
  otherwise) next to the JSON metadata. `blob.*` is not offered as tools.
- **inspectorapp** drops its dummy-platform override and asks for a headless
  window, so it renders offscreen on the real backend by default.
  `REACTIVE_PLATFORM=dummy` still selects the dummy platform on a machine with
  no GPU, where `canCapture` is false and everything else works as now.

## If the backdrop stack lands first

The unmerged backdrop stack (PRs #172-#177) moves every window's rendering onto
an offscreen sRGB target that is then put on the surface. If it merges before
enabler 4, the readback source becomes that target for all windows:

- on-screen capture stops reading a default framebuffer, so the undefined
  regions, the surface-format caveat and the read-before-swap ordering all go
  away, and headful capture can ship with enabler 4;
- the pixel contract holds unchanged on every backend, WGL included;
- `OffscreenWindow`'s private framebuffer probably folds into the shared target.

The readback command, the one-shot window request and everything above ase are
unaffected.

## Testing

"Dummy" means it runs on every CI leg; "real GL" means CI-dark, run by hand like
`bquiheadlesstest` (built everywhere, registered nowhere, `src/bqui/meson.build`).

- **1, writes (CI).** `transporttest`: large frames to a peer that is not
  reading do not block, resume in order, and pause dispatch; each transport on
  the legs that have it. `runlooptest`: writable, and readable plus writable on
  one socket.
- **2, deferred (CI).** `sessiontest` with a fake app: late resolve, replies out
  of order, a dropped responder, disconnect with requests pending, the gate
  holding back a later inject. *By hand:* `app.step` renders every stepped frame
  on an offscreen GL window.
- **3, blobs (CI).** Store unit tests (ranges, cap, release, ids), protocol
  tests through the first producer, and the describe additions.
- **4, readback.** *CI:* the arming logic on the dummy (consumed by one present,
  exactly one callback, failure on window destruction, "unsupported" from the
  dummy context). *By hand:* `bquiheadlesstest` renders known content and
  asserts corner colours, row order, an sRGB reference value and the pixel size.
- **5, capability (CI).** `window.list` on dummy and fake windows; the
  unsupported error.
- **6, feature.** *CI:* the encoder (inflate and unfilter its output, check CRCs
  and pixels, edge sizes), crop and scale arithmetic, and the whole protocol
  path against a fake `RemoteWindow` returning synthetic pixels. *By hand:* the
  bridge self-test saves a PNG from `inspectorapp`; look at it.

The seam is what keeps this honest: everything except the GL readback itself
runs on CI through a fake window, so the CI-dark part is one render command.

## PR plan

| # | PR | Depends on |
|---|---|---|
| P | tools: factor the Python protocol client out of the bridge | - |
| 1 | btl+bqui: non-blocking transport writes on `RunLoop` writable | - |
| 2 | ase+bqui: deferred replies; `app.step` waits for its frames | 1 |
| 3 | bqui: blob store, `blob.read` / `blob.release`, describe additions | 1, 2 |
| 4 | ase: readback command and one-shot capture on the offscreen window | - |
| 5 | bqui: `canCapture` in `window.list` and the unsupported error | 4 |
| 6 | bqui: `window.screenshot`, PNG encoder, bridge image content, inspectorapp offscreen | 2, 3, 5, P |
| 7 | ase: on-screen capture (follow-up) | 4 |

1 to 5 land in that order as decided; 4 touches only ase and can be developed in
parallel with 1 to 3. `docs/decisions.md` and the ase and bqui `AGENTS.md` get
their entries in the PR that makes each statement true.

## Open questions for the owner

1. **Does the zero-time frame apply pending input?** Proposed: yes, it is an
   ordinary frame with no elapsed time, keeping one frame path. The alternative
   is a redraw-only path in `WindowBridge` that skips the signal update.
2. **Reply ordering.** Proposed: order effects, let replies overtake (a slow
   encode does not block introspection). The alternative is strictly serial
   replies, simpler for a naive client.
3. **A stalled client.** Proposed: throttle forever and drop `frame`
   notifications, never disconnect. The alternative is a stall timeout.
4. **First blob producer in PR 3.** Proposed: an opt-in flag on
   `window.renderTree` to answer with a blob, giving the facility a real
   producer and a test vehicle before the screenshot exists. The alternative is
   a test-only seam into the registry.
5. **Limits.** Proposed 256 MiB outstanding, 1 MiB maximum read, 256 KiB default
   read, a few captures in flight. All are guesses to be tuned.
6. **Auto-release.** Proposed: none; `blob.release` is explicit and the client
   helper guarantees it. The alternative releases on reading the last byte, which
   breaks re-reads.
7. **A size bound besides `scale`.** Proposed: also accept a longest-edge limit
   in pixels, since an agent cares about the result's size, not the ratio.
8. **Opaque RGB output.** Proposed: yes. Straight-alpha RGBA would need
   un-premultiplying sRGB-encoded values, which is lossy and has no consumer.
9. **More in `window.list`.** Proposed: add logical size and scaling factor next
   to `canCapture`; a client needs both to plan a region.
10. **On-screen capture timing.** Proposed: follow-up PR 7, unless the backdrop
    stack lands first and makes it free.
11. **A software-GL CI leg** (Xvfb plus llvmpipe) would light up the CI-dark
    parts. Proposed: not in this track.
12. **Home of the Python module.** Proposed: a plain package under `tools/`,
    asyncio at the core with a thin synchronous wrapper when the Robot library
    arrives; no packaging yet.
13. **`tools/mcp/README.md:4` links `docs/design/inspector-protocol.md`, which
    does not exist.** Proposed: PR P repoints it at the driver header.

## Non-goals

- Live video or streaming; a capture is one frame on request.
- The app writing image files.
- Image formats other than PNG, and decoding images in the app.
- A binary frame kind; the blob API leaves room for it.
- Reading back `TargetImage` renders, though the readback command would serve.
