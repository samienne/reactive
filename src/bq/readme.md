# bq

`bq` is a small C++17 **functional-reactive dataflow** library: signals (values
that change over time) and streams (sequences of events). It has no UI
dependency and is usable on its own for any declarative data-flow — game logic,
document models, or the UI toolkit (`bqui`) that is built on top of it.

## Signals — values over time

A **signal** describes a value that changes over time. You do not read a signal
directly; you describe how values relate, and the surrounding system evaluates
them when needed.

State enters through an **input**, which returns a read-only signal and a handle
to push new values:

```cpp
#include <bq/signal/input.h>

auto count = bq::signal::makeInput(0);
count.handle.set(42);           // push a new value
// count.signal is the read-only signal
```

Derive new signals with `map`, and combine several with `merge`:

```cpp
auto doubled = count.signal.map([](int n) { return n * 2; });

auto area = merge(width.signal, height.signal)
    .map([](float w, float h) { return w * h; });
```

A **constant** never changes, which is handy wherever a signal is expected:

```cpp
auto title = bq::signal::constant<std::string>("hello");
```

## Streams — events over time

A **stream** carries discrete events; every value pushed into it is delivered
(unlike a signal, where only the latest value matters). Streams model events —
user actions, timers, anything discrete.

Create one with `pipe`, push through its handle, and fold events into a signal
with `iterate`:

```cpp
#include <bq/stream/pipe.h>
#include <bq/stream/iterate.h>

auto events = bq::stream::pipe<int>();
events.handle.push(1);          // send an event

auto total = bq::stream::iterate(
    [](int sum, int delta) { return sum + delta; },
    0,                          // initial value
    events.stream);
// total is a signal holding the running sum
```

`iterate` is the main bridge from events to state.

## Layout

- `bq/signal/` — signals and their combinators (`makeInput`, `constant`, `map`,
  `merge`, and more).
- `bq/stream/` — streams (`pipe`, `iterate`, `collect`).

For how signals are represented and evaluated internally, and the traps to know
before editing, see `AGENTS.md` in this directory.
