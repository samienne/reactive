# bq — agent notes

*Last verified against `d2e8954` (2026-07-13).*

Internals, entry points, and traps for the reactive core. Concepts and usage are
in `readme.md`; project-wide conventions are in the top-level `docs/`. This file
is the index for `bq`; if it outgrows itself, split topics into a `.agent/`
folder here and leave one-line hooks below.

## Type model

- A signal is `Signal<TStorage, Ts...>` (`bq/signal/signal.h`). `Ts...` is a
  **pack of value types** — a signal can carry more than one value, delivered as
  a `SignalResult<Ts...>` (`bq/signal/signalresult.h`).
- `AnySignal<Ts...>` derives from `Signal<void, Ts...>` — the `void` storage type
  is what erases the concrete storage. Use `AnySignal` in public signatures and
  when storing a signal; the concrete `Signal<TStorage, Ts...>` is what
  `map`/`merge`/etc. return before erasure.

## Evaluation model

- Signals are **stateless graph descriptions**. The actual per-signal state
  (current values, caches) lives in a `SignalContext` (`bq/signal/signalcontext.h`),
  not in the signal object — which is why signals are cheap to copy and share.
- A `SignalContext<TSignals...>` can hold **several** top-level signals over one
  shared `DataContext`; they advance in lockstep and a `.share()` node reachable
  from more than one of them is evaluated once per pass. Address every entry by
  index (`evaluate<I>()`/`didChange<I>()`), even with a single signal — there is
  no non-indexed accessor. Each entry's result is cached by owned value, so
  `evaluate<I>()` never aliases signal data; extract with `.get<N>()`. See
  `docs/decisions.md` for the rationale.
- Evaluation is **change-driven**: a signal is recomputed when its inputs
  actually change, not on a fixed frame tick.

## Threading: the context is the unit of concurrency

A `DataContext` is a private member of one `SignalContext`
(`signalcontext.h:122`), and everything under it is written only by that
context's `initialize` and `update` passes — each a single synchronous call over
that context's entries. So **per-context state needs no lock**: two
instantiations of one description in one context are advanced one after the
other on one thread. What runs concurrently is *contexts*, over a shared
description (`signaltest.cpp`'s `signal.share` drives 1024 of them concurrently
over a thread pool), and each worker's `findData` reaches a different state
object.

What does need a lock is **description-level state written from another
thread**. `InputControl` (`input.h:25`) is the case: `InputHandle::set` writes it
from wherever the caller happens to be. Its per-context `ContextDataType` carries
no lock, and neither does `Weak`'s (`weak.h:17`).

`SharedControl` is the exception to the shape, not to the rule: its mutex
(`sharedcontrol.h:54`) sits on per-context data. Do not read it as the pattern to
copy for a new node's per-context state.

## Entry points

- State in: `makeInput` (`bq/signal/input.h`) → `{signal, handle}`; push with
  `handle.set`.
- Derive/combine: `map` (`bq/signal/map.h`), `merge` (`bq/signal/merge.h`),
  `combine`/`join`/`conditional` (same folder). `constant` (`bq/signal/constant.h`).
- Lists whose *membership* changes: `ArraySignal<T>`, entered by `forEach` and
  left by `join`, with `concat` and `ArraySignal::map` between
  (`bq/signal/arraysignal.h`), built over `bq/signal/detail/pick.h`. Its design
  record, and the parts still outstanding, are in `docs/design/arraysignal.md`.
- Streams: `pipe` (`bq/stream/pipe.h`) → `{handle, stream}`, `handle.push`;
  `iterate` (`bq/stream/iterate.h`) folds a stream into a signal; `collect`.

`bq/signal/detail/` holds work that is built and tested but has no public caller
yet, in namespace `bq::signal::detail`. Currently `pick.h`: `shareKeyed`, `pick`
and `requirePresent`, the groundwork for `ArraySignal` — see
`docs/design/arraysignal.md`.

## Traps

- **`merge()` has no zero-argument form.** It is variadic but `merge<>()` fails
  (`ConcatSignalResults<>` has no result type), and with zero arguments it is not
  even found by ADL. Build a no-input signal as a `constant` instead. The
  principled fix would be a `signal<void>` identity element for `merge` — see
  `docs/decisions.md`.

For cross-cutting rules (the `Any` = type-erasure convention, the include-dir
firewall, symbol visibility) see `docs/conventions.md`; do not restate them here.
