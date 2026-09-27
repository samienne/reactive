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

## Collections — lists of items with identity

A **collection** is a shared, mutable list whose every item has an id that
survives updates and reorderings. Changes are made in a write transaction; each
transaction publishes one immutable snapshot, however many edits it made:

```cpp
#include <bq/signal/collection.h>
#include <bq/signal/collectionsignal.h>

bq::signal::Collection<std::string> todos;
{
    auto transaction = todos.write();
    transaction.pushBack("write docs");
    transaction.pushBack("ship it");
}   // one new snapshot published here
```

`forEach` builds something once per item and hands it the item's value as a
signal, so editing an item updates what was built for it instead of rebuilding
it:

```cpp
auto labels = forEach(todos,
    [](bq::signal::AnySignal<std::string> text, std::uint64_t id)
    {
        return bq::signal::AnySignal<std::string>(
            text.map([](std::string const& t) { return "- " + t; }));
    });
```

If an exception leaves the transaction's scope, its edits are discarded and
nothing is published. Reading never waits for a writer: `todos.read()` returns
a view of the latest published snapshot.

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
  `merge`, and more), and the state containers `Collection` and `SharedVector`.
- `bq/stream/` — streams (`pipe`, `iterate`, `collect`).

For how signals are represented and evaluated internally, and the traps to know
before editing, see `AGENTS.md` in this directory.
