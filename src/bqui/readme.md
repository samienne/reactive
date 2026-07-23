# bqui

`bqui` is the UI toolkit — widgets, layout, modifiers, shapes, theming, and the
application/window loop. It is built on `bq` (signals/streams), `avg` (vector
graphics), and `ase` (rendering), and is the library an application links
against.

## Widgets and layout

A widget is a **description** (`AnyWidget`), realised by the framework. Combine
ready-made widgets — `label`, `button`, `textEdit`, `hbox`/`vbox`,
`uniformGrid`, `scrollView`, scrollbars, and the flexible spacers — with layout
containers:

```cpp
auto ui = widget::vbox({
    widget::label("Name:"),
    widget::hbox({ widget::label("First"), widget::label("Last") }),
});
```

Layout is a size-hint negotiation (minimum / natural / stretch per axis), with
**gravity** aligning a widget within its allocated space.

## Modifiers

A **modifier** adds appearance or behaviour, applied with the pipe operator and
composed left to right:

```cpp
widget::label("Hi") | modifier::margin(10.0f) | modifier::frame()
```

Modifiers cover framing, background, margins, clipping, transforms, sizing,
gravity, input handlers (`onClick`, `onHover`, the pointer handlers), and theming.

## Shapes

Vector shapes scale to the space they are given and become widgets when styled:

```cpp
shape::circle().fill(avg::Color(0.85f, 0.2f, 0.2f));
shape::rectangle().rotate(0.2f).stroke(pen);   // rectangle, circle, ellipse
```

## Application

```cpp
return app()
    .addWindow(window(bq::signal::constant<std::string>("Title")), ui)
    .run();
```

A `Window` is a small handle: `window(title)` mints one, and the widget is
supplied to `addWindow` alongside it. The app owns the windows added this way,
and `run()` with no arguments runs until none of them is left. Windows may be
added while it runs, so opening a second window is a button handler away.

A window closes by leaving the app's collection. Its title bar does that by
itself, and so does `Window::close()`; `App::removeWindow` takes an identity
instead. Closing one of several windows removes it; the app stops when the last
one closes.

A close button *inside* the window captures the very window that owns it. The
window holds none of its contents — the widget lives in the app — so this closes
no cycle:

```cpp
Window w = window(bq::signal::constant<std::string>("Details"));

app().addWindow(w, widget::button("Close",
        bq::signal::constant(std::function<void()>(
                [w]() { w.close(); }))));
```

The collection is imperative: `App::addWindow` opens a window and
`removeWindow`/`Window::close` close it, before and while the app runs. Removing
a window unmounts its widget but keeps the window's own data, so a held `Window`
still reads its props and can be added again with a fresh widget.
`App::getWindowsSignal` observes the set for a UI that follows it — a window
list, or a title that counts — but that signal reports the collection, it does
not drive it.

Signal changes made inside a `withAnimation` scope are animated.

## Layout (of the source)

- `widget/` — widgets and the build pipeline.
- `modifier/` — modifiers.
- `shape/` — the shape builder and concrete shapes.
- `provider/` — the environment (theme and scoped parameters).
- `app.h`, `window.h`, `withanimation.h` — the application loop and animation.

For the widget pipeline, the shape builder's internals, and the traps to know
before editing, see `AGENTS.md` in this directory. The root `readme.md` has a
fuller tour with runnable examples.
