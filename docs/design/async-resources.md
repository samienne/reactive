# Design: resources are loaded, not touched

> **Status: direction only, nothing implemented or authorised.** Written
> against the unmerged `origin/effect-brushes` (tip `30b6727b`,
> 2026-11-14); every path below points there. Three labels mark status:
> **OWNER**, the owner said it; **PROPOSED**, suggested in discussion and
> not confirmed; **UNVERIFIED**, a factual claim the code was not checked
> for. Nothing here upgrades a label. Lookup results say whether they
> were read in code at that tip or inferred, and inferred ones were not
> run. Names are working names. Builds on `image-brush.md`, whose
> decisions are referenced, not restated.

## Purpose

`image-brush.md` makes an image a future: it cannot be used until it has
loaded. This note records the wider direction that grew out of that:
every resource an application loads from a file is a future, the toolkit
never blocks on one behind the application's back, and an application
has a visible structure for loading. It records what the owner has
decided, what the code does today, and what is still to be looked up. It
is not a staged plan.

## What the owner has said

- **OWNER: loading is explicit.** An image has to be loaded before it can
  be used, unlike toolkits (Qt was the example) where pointing at an image
  resource hangs the UI for the load time. Fonts work the same way. This
  gives applications a default structure: a resource loading step, and
  the UI shows when everything has loaded. Everything is deliberate, so no
  accidental hang can happen; the cost is more burden on the developer.
  Resources are futures, and `wait()` on the load is the one deliberate,
  visible way to block.
- **OWNER: a default font is built in and always embedded.** It is in the
  binary and always available; there is no option to turn it off
  (*Fonts*).
- **OWNER: switching fonts is explicit.** A font from a file produces a
  future, which needs an explicit action to use; switching the font in a
  `then` handler is a very deliberate action, and that is fine.
- **OWNER: fonts from memory are immediate; images from memory are not**
  (*Embedded resources*).
- **OWNER: no embedding helper for now.**
- **OWNER: no waiting mechanism in the toolkit.** A window needs a
  resolved widget (*App exit and the first window*).
- **OWNER, to be tried: a three-state future adapter** (*The future
  adapter*); its accessors and failure handling are PROPOSED.
- **OWNER: a default theme is always available** (*Theme*).
- **OWNER: the app exits on a close, not on an empty window list**, and
  `run(signal)` leaves the public interface; `runUntil`, already private,
  stays the internal mechanism (*App exit and the first window*).
- **OWNER: `App::exitWhenAllWindowsClosed` is on by default**, and the
  window overload is `addWindow(Window, Future<AnyWidget>)`, a failed
  future rethrown from `run()` (*App exit and the first window*).
- **OWNER, decided in `image-brush.md`:** watched futures resolve on the
  thread pool, never the GL thread (*Fences*); the future-to-signal
  adapter lives in bq and is generic, its widget form in bqui; image
  creation has no synchronous form in the first version (*Making an image
  from pixels*); the render context moves to App and exists from App
  construction.

## Where the code is

Read at `30b6727b`; nothing was built or run.

**btl futures** (`src/btl/include/btl/future/`):

- `then` (`future.h:136-230`) runs its continuation on the thread that
  completes the future, or inline on the caller's thread when it is
  already ready (`futurecontrol.h:172-187`, `:214-224`). The continuation
  holds the new future only weakly (`future.h:166-172`), so dropping the
  future `then` returned drops the continuation.
- **Failure propagates.** A failed future rethrows when its value is read
  (`futurecontrol.h:79-80`); `then` catches anything its function or the
  read throws and fails its own future (`future.h:224-227`). `onFailure`
  (`future.h:119-131`) observes the `std::exception_ptr`. There is no
  pending/ready/failed value type; failure is the exception channel.
- `wait()` blocks (`future.h:291-294`); `get()` rethrows a failure;
  `cancel()` drops the control (`future.h:304-307`); `makeReadyFuture`
  makes a resolved one (`future.h:313-319`). `SharedFuture`
  (`sharedfuture.h:16`) is the form with several consumers.
- **Combining exists.** `whenAll` (`whenall.h:149-150`) joins
  heterogeneous futures into one; `merge` (`merge.h:108`) turns a vector
  of `Future<T>` into a `Future<std::vector<T>>`. In both the first
  failure fails the whole (`whenall.h:93-102`, `merge.h:57-67`). `join`
  (`join.h:17`) flattens a future of a future.
- **The pool** is a process-wide static (`threadpool.h:204-206`);
  `btl::async` (`async.h:21`) posts to it through `asyncJob`
  (`:13-18`, `:44`).

**Fonts** (`src/avg`, `src/bqui`):

- `avg::Font` is made from a file path and a face index (`font.h:42`).
  The constructor loads **synchronously** through one static
  `FontManager` (`font.cpp:20-25`): `getFontImpl`
  (`fontmanager.cpp:265-294`) calls `FT_New_Face` on the path (`:281`)
  and throws `std::runtime_error` if it fails (`:283-284`). Loaded faces
  are cached weakly by path and face.
- **No load from memory.** `FT_New_Face` is the only face creation in
  `src/`; nothing calls `FT_New_Memory_Face` or `FT_Open_Face`.
- **No font is in the binary.** The repository carries
  `data/fonts/OpenSans-*.ttf` with an `Apache License.txt` beside them,
  read at a path relative to the working directory.
- **The theme names the font by path.** bqui's theme constructs
  `avg::Font("data/fonts/OpenSans-Regular.ttf", 0)` (`theme.cpp:62`,
  `:116`), so making a default theme loads a file synchronously and
  throws when the working directory lacks it. `Theme` has `getFont()`
  (`theme.h:34`) and no font setter; `label.cpp:31,51` and
  `textedit.cpp:50,150` read it.
- UNVERIFIED: whether FreeType reads glyph data lazily from the file
  after `FT_New_Face`, so that later text layout also touches the disk.

**The app's run and its windows** (`src/bqui`):

- `App::addWindow(Window, AnyWidget)` (`app.h:47`) is documented as
  callable before and while the app runs, and lock-guarded so a worker
  thread may call it (`app.h:35-47`, `:105-111`). As read: it updates the
  window list under its lock, queues the mount under `pendingMutex_` and
  wakes the loop (`app.cpp:239-274`); the loop thread builds the window
  on its next sync (`app.cpp:393-414`). The wake is
  `PlatformBase::requestFrame`, an atomic flag and a run loop post
  (`platformbase.cpp:242-252`). So adding a window from a `then` on a pool
  thread works today, as read; UNVERIFIED by a test of that case (the
  closer thread in `apptest.cpp:573` calls `addWindow` and `close` from
  another thread).
- **`run()` is a state test.** `run()` is `runUntil` of "the window list
  is not empty" and `run(signal)` is `runUntil(signal)`
  (`app.cpp:309-321`); `runUntil` is private (`app.h:128-129`). An app
  that starts with no windows stops at once.
  `App.runWithNoWindowsReturnsImmediately` (`apptest.cpp:547-551`) tests
  exactly that, and is the only test or example relying on it; every
  other caller of `run()` adds a window first. Read: the stop comes in
  the first tick, before any window renders. `runUntil` makes the signal
  context (which evaluates the condition, `signalcontext.h:41-46`) and
  mounts pending windows before the loop (`app.cpp:369`, `:427`); the
  first tick updates, syncs and evaluates the condition, and a false
  stops the loop before `renderDirtyWindows` (`app.cpp:433-443`,
  `platformbase.cpp:103-112`). So windows are created and one update
  runs, but no frame is drawn.
- **The platform is made inside the run.** `runUntil` creates the
  platform (`app.cpp:323-338`) and then the render context
  (`app.cpp:363`), so nothing can be uploaded before `run()`.
- **The app has no exit.** Nothing ends `run()` other than its condition.
- `run(signal)` has eight callers, all tests in `src/bqui/test`:
  `apptest.cpp` (six: `:166`, `:245`, `:315`, `:402`, `:482`, `:533`),
  `headlessrendertest.cpp:49` and `inputreentrancytest.cpp:95`. Nothing
  outside `app.cpp` calls `runUntil` directly (read).

**bq and other threads** (`src/bq`):

- **No future-to-signal piece exists.** `signal.h:19` and `shared.h:11`
  include the btl future header, but nothing in `src/bq` uses it.
- **Setting an input from another thread is safe for the value and the
  context, as read.** `InputHandle::set` writes the value under the
  input's recursive mutex (`input.h:66-83`); the update takes the same
  mutex and copies the value into the context's own data, which
  `evaluate` reads without a lock (`input.h:151`, `:182-190`,
  `:233-239`). `set` never touches the context; the loop thread pulls.
  Observers fire outside the lock, on the setter's thread, through
  `ObserveControl::fire` (`datacontext.h:24-46`). No test sets an input
  from another thread: the threaded tests set it on the test thread.
- **The wake it causes is the weak link.** Two observers exist, as read:
  - **The app's own wake** (an input the app's running signal reads):
    `wakeLoop` takes a mutex and calls the platform's thread-safe
    `requestFrame` (`app.cpp:227-232`, `:371`;
    `platformbase.cpp:242-252`). It causes a frame on every platform.
  - **A window's wake** (an input a widget or title reads) sets an
    atomic and calls the ase window's `requestFrame`
    (`windowbridge.cpp:61-67`, `windowbridge.h:133`). On a generic
    window that writes a plain `std::optional`
    (`genericwindow.cpp:102-105`) that the loop also writes in
    `GenericWindow::frame` (`:117-124`): a data race, the one
    `image-brush.md` makes safe in its frame request stage (fixed on the
    local branch `bugfix/requestframe-race`). WGL and GLX windows then
    wake the platform (`wglwindow.cpp:502-506`, `glxwindow.cpp:454-458`);
    dummy and offscreen windows do not (`dummywindow.cpp:42-45`,
    `offscreenwindow.cpp:42-45`). Offscreen windows back headless
    windows on every backend, and macOS uses the dummy platform. There,
    inferred, an idle loop stays blocked until something else wakes it.
  - Inferred: a set landing between `onFrame`'s exchange of the atomic
    and `frame()` clearing the request is lost; and the window's wake
    captures the bridge's raw `this`, so a bridge torn down on the loop
    thread while another thread fires can be called after destruction.
- **Consequence.** The future adapter, and `App::exit` from another
  thread if its input is reached from a window, need the race fix, a
  platform wake for dummy and offscreen windows, and a test that sets an
  input from another thread. An input reached only by the app's running
  signal wakes safely today.

**Exceptions** (`src/btl`, `src/ase`, `src/bqui`):

- **An exception during signal evaluation leaves `App::runUntil`, as
  read.** Frames run as tasks of the run loop on every backend, never
  from a window procedure or an X11 callback, and the path from
  `RunLoop::run` through the platform tick to the App's frame callback
  and `WindowBridge::onFrame` has no `catch` and no `noexcept`
  (`runloopimpl.h:42-46`, `platformbase.cpp:73-116`, `app.cpp:433-443`,
  `windowbridge.cpp:386`). The only `catch` in `src/bqui/src` is the
  remote driver's, around every RPC: under `app.step` it turns an
  `RpcError` or a `std::exception` into an error reply and the app
  carries on from a half-updated frame; any other exception still
  escapes (`remote/remotedriver.cpp:393-408`). Its running-state tick
  does not catch.
- **What it leaves behind** (inferred from the code read):
  - an injected run loop stays marked running (`running_` is set with no
    guard, win32 `runloopimpl.cpp:121`, posix `:90`), so a second run on
    it throws; the owned default loop is destroyed, so no effect there;
  - the platform's tick and step callbacks capture `run()`'s locals by
    reference (`platformbase.cpp:75-76`, `:120-125`) and are cleared only
    on a normal return (`:159-160`), so they dangle; a later
    `requestFrame` on a surviving platform is undefined behaviour if its
    loop runs;
  - if the render queue's `finish()` throws during the unwind
    (`app.cpp:161-169`), the process terminates; not determined whether
    it can.
  - Separately, WGL input handlers run inside the window procedure
    (`wglplatform.cpp:40-50`, `:336-344`), so a throwing event handler
    crosses a Win32 callback. Not the frame path.
- **Consequence.** Exiting on a failure works as a way to end the
  process. The App should not be reused after such a throw unless that
  cleanup is made exception safe.

## The model

- **Every resource loaded from a file is a future.** Making one never
  blocks the caller; the value is usable once the future resolves. A
  failed load fails the future.
- **One deliberate block.** `wait()` on the future is the only way to
  block, visible at the call site, for tools, tests and the startup case
  where blocking is the point. Nothing in the toolkit calls it for the
  application, and the toolkit has no other waiting mechanism (OWNER).
- **Fonts from memory are immediate** (OWNER), the default font among
  them. Images from memory are still futures (*Embedded resources*).
- Continuations run on the pool (OWNER, `image-brush.md`); the way back
  to the UI is the bq adapter (*The future adapter*).

The cost the owner named is real: every application writes a loading
step. The transformations below exist to keep that step short.

## Fonts

- **OWNER: a default font in the binary, always.** There is no option to
  turn it off.
- **OWNER: its coverage.** Basic Latin, Latin-1 Supplement and common
  symbols: the euro sign, typographic quotes, dashes and the ellipsis. Be
  liberal about what goes in, and scale back only if size turns out to be
  a problem. Large scripts such as Chinese stay out. The size of the
  subset is UNVERIFIED; the estimate is a few tens of kilobytes.
- **PROPOSED: which font.** A subset of an open font under Apache 2.0 or
  the OFL, embedded as a byte array and loaded through FreeType's memory
  load (`FT_New_Memory_Face`, unused today). Open Sans is already in the
  repository with an Apache licence file; whether its licence suits
  embedding is UNVERIFIED.
- **OWNER: a font from memory is immediate.** Opening a face from memory
  reads only its headers, and glyphs rasterise on demand, so there is
  nothing to wait for. Measured (FreeType 2.13.2, MSVC x64, Open Sans
  Regular, 217 KB): opening and closing a face from memory takes about
  7.5 us in a release build, 18 us in debug; from a file path about
  370-390 us with a warm cache, dominated by opening the file; the 95
  ASCII glyphs at 16 px about 136 us release, 295 us debug. This
  confirms an embedded font can be immediate. A cold-disk file load was
  not measured. This covers any font loaded from memory, not only the
  default one.
- **OWNER: fonts from files are futures**, and switching to one happens
  in a `then` handler. PROPOSED: the switch goes through the theme
  (*Theme*).

## Embedded resources

- **OWNER: no embedding helper for now.** An application that embeds a
  resource writes its own byte array. A build helper can come later if
  the pattern shows up in use.
- **Fonts from memory are immediate** (*Fonts*).
- **Images from memory are futures** (OWNER). They skip the file read,
  but the upload remains and image creation has no synchronous form
  (`image-brush.md`, *Making an image from pixels*).

## Theme

- **OWNER: a default theme is always available.** It uses the embedded
  default font, so the hardcoded `data/fonts/OpenSans-Regular.ttf` path
  (`theme.cpp:62`, `:116`) and its working directory dependence go away.
- **OWNER: making a new theme is mostly asynchronous**, because its font
  inputs come from futures.
- **PROPOSED: a theme built only on fonts from memory can be immediate**,
  and the theme holds a settable font value, so a font future resolving
  in a `then` handler changes it.
- **How the theme reaches widgets, as read.** It is a build parameter:
  `BuildParams` maps a tag to a signal (`buildparams.h:14-56`), and
  `ThemeTag` defaults to `constant(Theme())` (`settheme.h:9-15`).
  Widgets read it through `provideTheme()` (`providetheme.h:9-11`). An
  application supplies one with the `setTheme` modifier, taking a `Theme`
  or a signal of `Theme`, for that subtree (`settheme.h:19-27`); a signal
  changes it at run time, as `testapp1` does (`adder.cpp:154-163`). There
  is no App or window level theme. `Theme` has one setter,
  `setSecondary` (`theme.h:16-43`), and no font setter.
- **The window background ignores it.** The window wraps the user widget
  in a background built with empty `BuildParams` (`windowbridge.cpp:25-36`,
  `:49-52`), so the background always uses the default theme.
- PROPOSED, fitting the design above: the default theme on the embedded
  font, and a settable font value added to `Theme`, changed through the
  `setTheme` signal. Changing it rebuilds what reads the theme; the cost
  of that was not looked up.

## The future adapter

- **OWNER, to be tried: a future becomes a signal of a three-state
  value.** Placeholder names: `fromFuture`, giving a signal of
  `AsyncResult<T>`, a btl type. A signal always needs a value, and
  `std::expected` is C++23 while the project is C++17, hence a type of
  its own with three states:
  - **pending**, the first value;
  - **ready**, holding the `T`;
  - **failed**, holding the `std::exception_ptr` the future failed with.
- **PROPOSED: one change.** The value goes from pending to ready or to
  failed once, and that change causes a frame.
- **PROPOSED: accessors.** `isPending`, `hasValue` and `hasError` never
  throw.
  `value()` gives the `T` when ready, rethrows the stored exception when
  failed, and throws a distinct exception while pending, since reading a
  pending value is a programming error. `valueOr(fallback)` gives the
  fallback while pending and rethrows on failure; the placeholder helper
  uses it. `error()` gives the `std::exception_ptr`, for code that wants
  to show an error widget.
- **PROPOSED: loud by default.** Code that handles only the happy path
  exits on a failure instead of showing nothing. As read, an exception
  thrown during signal evaluation leaves `App::run`, except under the
  remote `app.step` RPC, which swallows a `std::exception`; the App is
  not safe to reuse afterwards (*Where the code is*, exceptions).
- `image-brush.md` builds on this value (*A placeholder*, and the
  `widget::image` widget); its weak hold on the input is that note's.

## Transformations

- **A future of a widget and a placeholder widget give a widget** (bqui;
  OWNER in `image-brush.md`, the owner's earlier plan). It shows the
  placeholder until the future resolves, built on `valueOr`.
- **OWNER: further helpers grow from patterns found in use.** Candidates
  seen so far, none decided:
  - **Latest of a changing request.** A signal of requests gives a signal
    of the newest finished result; a result for a request that has since
    changed is dropped. This is the font switch and the avatar that
    follows a selection.
  - **Combine loads.** `whenAll` and `merge` exist in btl; the gap is
    ergonomics for "load everything, then show the UI", for example a
    named set of resources resolving into one value the UI is built from.

## App exit and the first window

A window needs a resolved widget. An application either adds the window
once its load has finished, or adds it at once with the placeholder
helper. The toolkit offers nothing else to wait with (OWNER).

**Exit reacts to a close (OWNER).** `run()` no longer tests "the window
list is not empty"; it ends on a window closing, under the rules below.
An app with no windows yet keeps running.

- **PROPOSED: `Window::exitOnClose(true)`.** Closing that window ends
  `run()`; the other windows go with the app.
- **`App::exitWhenAllWindowsClosed(bool)`.** When a close leaves no
  windows, `run()` ends. It can be changed at any time. OWNER: on by
  default.
- **PROPOSED: `App::exit(int code = 0)`**, an explicit exit; the app has
  none today. The details too: the exit code, and that it may be called
  from any thread. If it sets a bq input internally, setting it from another
  thread is safe and wakes the loop as long as only the app's running
  signal reads it; reached from a window, it needs the wake fixes in
  *Where the code is* (bq and other threads).

Which closes count, and how a window still waiting for its widget
counts, are open (*Lookups before implementation*).

**`run(signal)` leaves the public interface (OWNER).** It conflicts with
the exit methods above, and in practice the condition is an input of
`bool` set to false, which is `exit()`. Only tests use it today (*Where
the code is*); they move to `exit()` or to a non-public entry point.
`runUntil` is already private and stays the internal mechanism (OWNER).
Recorded loss: the app evaluated the condition in its own signal context
even with no window open. An app-level observe can be added later if a
real use appears.

**Adding a window with a future.** OWNER: `addWindow(Window,
Future<AnyWidget>)`, since today's form takes a window and a widget
(`app.h:47`) and the window value is normally known up front. The window
opens when the widget resolves.

- OWNER: a failed future is rethrown from `run()`. With exit on close,
  this overload is not needed to keep the app alive; its job is to carry
  the failure.
- Calling `addWindow` inside a `then` still works, since `addWindow` is
  lock-guarded, but then the developer owns the failure path: without an
  `onFailure` that calls `App::exit()`, a failed load leaves a running app
  with no window. Docs lead with the future overload.
- Nothing is on screen until the load finishes. For a long load the
  placeholder helper is the better choice.
- The platform is made inside `runUntil` today (`app.cpp:323-338`), so
  uploads during loading rely on the move of the backend and the render
  context into the App constructor, already decided in `image-brush.md`.
- `App.runWithNoWindowsReturnsImmediately` (`apptest.cpp:547-551`) tests
  the old rule and changes with it.

## Application shapes

Sketches, not real signatures.

**(a) Load, then open the window.** The window opens when the widget
resolves; a failed load comes out of `run()`.

```cpp
App app;
app.addWindow(window("Main").exitOnClose(true),
    whenAll(loadFont(...), loadImage(context, ...))
        .then([](Font font, Image logo) { return mainUi(font, logo); }));
app.run();
```

**(b) Open at once, fill in as loads finish.** The window shows
placeholders; each resource arrives through the bq adapter.

```cpp
auto logo = fromFuture(loadImage(context, ...)); // AsyncResult<Image>
app.addWindow(window("Main"), header(logo, placeholderIcon()));
app.run();
```

**(c) The deliberate block.** For tools and tests; the `wait` is visible.

```cpp
auto font = loadFont(...);
font.wait();
```

## Lookups before implementation

- Whether FreeType keeps reading the font file after `FT_New_Face`.
- The licence and subset size of the embedded default font.
- A cold-disk font load from a file, if the warm figure is not enough.

Open questions for the owner, not decided here:

- **What counts as a close** for `exitOnClose` and
  `exitWhenAllWindowsClosed`. A window closes through its title bar
  (`windowbridge.cpp:84`, then `removeWindow` at `app.cpp:185`), through
  `Window::close()` or through `App::removeWindow`. Either every route
  counts, so a program that closes its last window exits, or only the
  user's close does, and a program closing windows calls `exit()` itself.
- **A window whose widget future is still pending.** Whether it counts
  as open, which decides whether another window's close leaves "no
  windows"; and what happens if it is closed or removed, or the app
  exits, before the future resolves, and to a failure that arrives after
  `run()` has returned. One option drops the continuation with the
  window and ignores a late failure; another keeps the app running until
  pending windows resolve.

Done, with results in place: setting an input from another thread and
its wake, `run()` with no windows and exceptions (*Where the code is*),
the font load time (*Fonts*) and theme delivery (*Theme*).
Work they found:

- The window frame request race fix, a platform wake from dummy and
  offscreen windows, and a test setting an input from another thread,
  before the future adapter or an any-thread `App::exit` rely on it.
- Making the run's cleanup exception safe, only if an App is to be
  reused after a throw.

## Non-goals

- Blocking loads hidden inside the toolkit, and lazy loading on first use.
- A waiting mechanism in the toolkit beyond `wait()` on a future.
- A resource manager or cache beyond what each loader keeps.
- A resource-embedding helper, for now.
- File decoders inside avg (`image-brush.md`).
- Network loading, hot reload and resource packs.
