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
    .windows({ window(bq::signal::constant<std::string>("Title"), ui) })
    .run();
```

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
