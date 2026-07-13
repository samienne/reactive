# Reactive

[![build](https://github.com/samienne/reactive/actions/workflows/build-workflow.yaml/badge.svg)](https://github.com/samienne/reactive/actions/workflows/build-workflow.yaml)

Reactive is a C++17 graphical user interface toolkit built on **functional
reactive programming**. You describe your interface as a composition of values
that change over time, and the toolkit keeps the screen in sync for you — no
separate UI language, no manual wiring of callbacks between widgets.

The design is inspired by the [Elm](https://elm-lang.org/) programming language
and SwiftUI: interfaces are plain data, built by composing small pieces, and the
framework — not you — decides when to recompute and redraw.

The interface toolkit is the `bqui` library, built on top of `bq` — a
standalone reactive-signal library that forms the project's core. You will see
both in the examples: `bq::signal` and `bq::stream` for the reactive plumbing,
and `bqui::` for windows and widgets. (`bq` is a nod to the becquerel, keeping
the reactive/nuclear theme going.)

> **Status: early development.** The library is not production ready and the API
> changes regularly. Expect rough edges.

---

## A first program

```cpp
#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/widget/label.h>

using namespace bqui;

int main()
{
    return app()
        .windows({
            window(
                bq::signal::constant<std::string>("Hello"),
                widget::label("Hello, world!")
            )
        })
        .run();
}
```

`app()` creates an application, `.windows({ ... })` gives it one or more windows,
and `.run()` opens them and runs until they close. Each `window` takes a title
and a widget to show.

---

## Building

### Requirements

- **Meson** and **Ninja**
- A C++17 compiler — GCC 10+, Clang 11+, MSVC 2022, or clang-cl
- On Linux, the OpenGL development headers (e.g. `libgl1-mesa-dev`)

Third-party dependencies (Eigen, FreeType, and a few others) are downloaded
automatically by Meson the first time you configure, so an internet connection
is needed for the initial setup.

### Linux

```sh
git clone --recursive https://github.com/samienne/reactive.git
cd reactive
meson setup build
ninja -C build

./build/src/testapp1/testapp1   # a small playground application
ninja -C build test             # run the unit tests
```

### Windows

Run the commands from a *"x64 Native Tools Command Prompt for VS 2022"* so that
Meson can find the compiler (use `CXX=clang-cl` beforehand to build with
clang-cl instead of MSVC):

```sh
meson setup build
ninja -C build
```

Each library is built into its own directory, so the playground executable can't
find its DLLs on its own. Launch it through Meson's development environment,
which puts them on the path:

```sh
meson devenv -C build
testapp1
```

### Platform support

| Platform        | Status                                   |
| --------------- | ---------------------------------------- |
| Linux (GLX)     | Supported                                |
| Windows (WGL)   | Supported                                |
| macOS           | Builds and runs unit tests (no window)   |

---

## Concepts

### Signals

A **signal** is a value that changes over time. An `AnySignal<int>` is not an
`int`; it is a description of an `int` that the framework re-evaluates whenever
the things it depends on change. (The `Any` prefix simply means the signal's
type is erased, so you can store and pass it around freely.)

The simplest way to create one is an **input**, which gives you a read-only
signal together with a handle you can push new values into:

```cpp
auto count = bq::signal::makeInput(0);

count.handle.set(42);           // push a new value
// count.signal is a read-only AnySignal<int>
```

You derive new signals from existing ones with `map`. The derived signal updates
automatically whenever its source does:

```cpp
auto text = count.signal.map([](int n)
    {
        return std::to_string(n);
    });
// text is an AnySignal<std::string> that always mirrors count
```

A **constant** signal never changes and is handy wherever a signal is expected:

```cpp
auto title = bq::signal::constant<std::string>("My App");
```

You never poll or read a signal yourself; you hand it to a widget and the
framework evaluates it when it needs to draw.

### Streams

Where a signal represents a *value* over time, a **stream** represents a sequence
of *events* — every value pushed into it is delivered, and none are dropped.
Streams are how user actions and other one-off events enter the system.

Create a stream with `pipe`, push events through its handle, and fold those
events back into a signal with `iterate`:

```cpp
auto events = bq::stream::pipe<int>();

// events.handle.push(1) sends an event; events.stream is the read end.

auto total = bq::stream::iterate(
    [](int sum, int delta) { return sum + delta; },
    0,                 // initial value
    events.stream);
// total is an AnySignal<int> holding the running sum of everything pushed so far
```

`iterate` is the main bridge from events to state: give it a function, a starting
value, and a stream, and it maintains a signal that evolves as events arrive.

### Widgets and layout

Every element on screen is a **widget**. Widgets are descriptions, not live
objects — you build them and hand them to the framework, which realises them.
Most of the time you combine ready-made widgets with layout containers:

```cpp
#include <bqui/widget/label.h>
#include <bqui/widget/button.h>
#include <bqui/widget/hbox.h>
#include <bqui/widget/vbox.h>

auto ui = widget::vbox({          // stack children vertically
    widget::label("Name:"),
    widget::hbox({                // ... and horizontally
        widget::label("First"),
        widget::label("Last"),
    }),
});
```

Some of the widgets available today: `label`, `button`, `textEdit`, `hbox`,
`vbox`, `uniformGrid`, `scrollView`, `hScrollBar` / `vScrollBar`, and the
flexible spacers `filler` / `hfiller` / `vfiller`.

### Modifiers

A **modifier** changes a widget's appearance or behaviour. You apply modifiers
with the pipe operator, and they compose left to right:

```cpp
#include <bqui/modifier/frame.h>
#include <bqui/modifier/margin.h>
#include <bqui/modifier/onclick.h>

auto boxed = widget::label("Click me")
    | modifier::margin(10.0f)
    | modifier::frame()
    | modifier::onClick(0, [](auto const&)
        {
            // left mouse button pressed
        });
```

A few of the modifiers available: `frame`, `background`, `margin`, `clip`,
`transform`, `setSize`, `setSizeHint`, `setMinimumSize`, `setGravity`,
`onClick`, `onHover`, the `onPointer*` handlers, and `setTheme`. Because
modifiers are just functions joined with `|`, you can write your own without
changing the widget types they apply to.

### Shapes

Shapes let you draw vector graphics that scale to whatever space they are given.
A shape becomes a widget once you style it with `fill`, `stroke`, or
`fillAndStroke`:

```cpp
#include <bqui/shape/circle.h>
#include <bqui/shape/rectangle.h>

auto dot   = shape::circle().fill(avg::Color(0.85f, 0.20f, 0.20f));
auto badge = shape::rectangle()          // a rounded rectangle
                 .rotate(0.2f)
                 .fill(avg::Color(0.20f, 0.50f, 0.90f));
```

The built-in shapes are `rectangle` (with an optional, animatable corner
radius), `circle`, and `ellipse`. Before styling, a shape can be transformed
(`translate`, `rotate`, `scale`), resized, and combined with other shapes. Every
parameter accepts either a plain value or a signal, so shapes animate for free.

### Animation

Animations are expressed as ordinary value changes; you just ask the framework to
interpolate them. `withAnimation` returns a **guard**: any signal changes made
while the guard is alive are animated, and the animation is applied when the
guard goes out of scope.

```cpp
#include <bqui/withanimation.h>
#include <avg/curve/curves.h>

{
    auto anim = withAnimation(0.3f, avg::curve::easeInOutCubic);
    open.handle.set(true);      // animates over 0.3s when the block ends
}
// changes made outside the scope are applied immediately
```

There is also a lambda form — `withAnimation(0.3f, curve, [&]{ ... })` — that
animates whatever changes you make inside the callback, if you would rather not
rely on scope.

---

## A larger example

A counter that increments and decrements — showing streams, `iterate`, and
buttons working together:

```cpp
#include <bqui/app.h>
#include <bqui/window.h>
#include <bqui/widget/label.h>
#include <bqui/widget/button.h>
#include <bqui/widget/hbox.h>

#include <bq/stream/pipe.h>
#include <bq/stream/iterate.h>
#include <bq/signal/constant.h>

#include <string>

using namespace bqui;

widget::AnyWidget counter()
{
    // The buttons push +1 / -1 into this stream of increments.
    auto events = bq::stream::pipe<int>();

    // Fold the increments into the current count.
    auto count = bq::stream::iterate(
        [](int current, int delta) { return current + delta; },
        0,
        events.stream);

    return widget::hbox({
        widget::button("-", bq::signal::constant(
            [handle = events.handle]() { handle.push(-1); })),

        widget::label(count.map([](int n) { return std::to_string(n); })),

        widget::button("+", bq::signal::constant(
            [handle = events.handle]() { handle.push(1); })),
    });
}

int main()
{
    return app()
        .windows({
            window(bq::signal::constant<std::string>("Counter"), counter())
        })
        .run();
}
```

Clicking a button pushes an event, `iterate` updates the count signal, and the
label — which is derived from that signal — redraws itself. You never wrote code
to update the label; the data flow does it.
