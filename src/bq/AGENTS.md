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
- `AnySignal<Ts...>` is literally `Signal<void, Ts...>` — the `void` storage type
  **is** the type erasure. Use `AnySignal` in public signatures and when storing
  a signal; the concrete `Signal<TStorage, Ts...>` is what `map`/`merge`/etc.
  return before erasure.

## Evaluation model

- Signals are **stateless graph descriptions**. The actual per-signal state
  (current values, caches) lives in a `SignalContext` (`bq/signal/signalcontext.h`),
  not in the signal object — which is why signals are cheap to copy and share.
- Evaluation is **change-driven**: a signal is recomputed when its inputs
  actually change, not on a fixed frame tick.

## Entry points

- State in: `makeInput` (`signal/input.h`) → `{signal, handle}`; push with
  `handle.set`.
- Derive/combine: `map` (`signal/map.h`), `merge` (`signal/merge.h`),
  `combine`/`join`/`conditional` (same folder). `constant` (`signal/constant.h`).
- Streams: `pipe` (`stream/pipe.h`) → `{handle, stream}`, `handle.push`;
  `iterate` (`stream/iterate.h`) folds a stream into a signal; `collect`.

## Traps

- **`merge()` has no zero-argument form.** It is variadic but `merge<>()` fails
  (`ConcatSignalResults<>` has no result type), and with zero arguments it is not
  even found by ADL. Build a no-input signal as a `constant` instead. The
  principled fix would be a `signal<void>` identity element for `merge` — see
  `docs/decisions.md`.

For cross-cutting rules (the `Any` = type-erasure convention, the include-dir
firewall, symbol visibility) see `docs/conventions.md`; do not restate them here.
