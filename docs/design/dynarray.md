# Design: `DynArray<T>` and `forEach`

> **Status: proposal — not implemented.** Nothing described here exists in the
> tree yet. This document is the agreed design, recorded so the implementation
> has a target and so the reasoning survives. Once it is built, the stable parts
> move to their normal homes — the model and its traps to
> `docs/conventions.md`, the settled *why* to `docs/decisions.md`, and the API
> contract to Doxygen in the new public headers — and this file goes away.

*Written against `221cd3f` (2026-07-19).*

## The problem

A widget list whose *membership* changes at runtime cannot be expressed as a
plain derivation. Deriving `vector<AnyWidget>` from `vector<T>` rebuilds every
widget on every change, and rebuilding a widget destroys its state: the text a
user typed, a cursor position, an animation in flight, a stream fold. The
existing answer — `Collection` + `dataSourceFromCollection` + `dataBind` +
`dynamicBox` — works, but it is a special-purpose pipeline bolted to one widget
container, it is not composable, and it lives in `bqui` where only widgets can
use it.

This design replaces that pipeline with two pieces:

- **`DynArray<T>`** — a value in `bq` describing a list whose contents and
  structure may both be dynamic. It is the *input* type a list-consuming API
  accepts.
- **`forEach`** — the keyed operator that turns a changing collection into a
  `DynArray<U>` with stable per-item identity, invoking a delegate once per new
  item rather than once per change.

## The framing: two kinds of dynamism

This split is the load-bearing idea; everything else follows from it.

**Value dynamism** — the structure is fixed and the contents change. A list of
three `AnySignal<T>` leaves is value-dynamic: there are always exactly three
items, in the same order, and each one's value varies over time. Identity is
**stable by construction** — item *k* is item *k* forever — so no identity
machinery is needed, and none should be paid for.

**Structural dynamism** — membership itself changes: the list grows, shrinks, or
reorders. This is the *only* place identity is needed, because "which item is
this?" no longer has a positional answer. `forEach` is what supplies it.

`DynArray<T>` expresses both. Its literal forms (a value, a signal, a nested
braced list) are value-dynamic. Structural dynamism enters a `DynArray` only
through `forEach` — or through the reactive-subtree constructor, which is
structural dynamism at the coarsest possible granularity (the whole subtree is
replaced).

## `DynArray<T>`

`DynArray<T>` lives in `bq`. It has no UI dependency; `bqui` merely consumes it.

### It is a description, not a container

A `DynArray<T>` is an **immutable value**. It has no `push_back`, no `erase`, no
mutation of any kind. Dynamism flows *in* from upstream: a `Collection<T>`
converted into a `DynArray<T>` stays live because the conversion produced a
signal that the collection drives — mutating the collection changes the
`DynArray`'s signal, not the `DynArray` object.

This is the same discipline as the rest of the toolkit (`docs/architecture.md`):
descriptions are immutable values, state lives upstream.

### A constructor per supported type — deliberately not a variant

The natural implementation is `std::variant` over the accepted forms. We are
**not** doing that. Instead `DynArray<T>` gets one converting constructor per
supported source type:

| Constructor takes | Meaning |
| --- | --- |
| `T` (or anything convertible to `T`) | a single constant item |
| `AnySignal<T>` | a single item whose value varies |
| `AnySignal<std::vector<T>>` | a varying list of items |
| `std::initializer_list<DynArray<T>>` | a fixed list of children |
| `std::vector<DynArray<T>>` | ditto, built at runtime |
| `AnySignal<DynArray<T>>` | a reactive subtree (see below) |

Two reasons. First, **acceptance is precisely controlled**: each form is opted
into by writing a constructor, so an accidentally-convertible type cannot slip
in, and the error message when something is not accepted names the type rather
than a variant alternative list. Second, **braces stay clean** — a variant
parameter forces the caller to name the alternative at every call site, which
defeats the whole point of a braced literal.

The public surface is deliberately just the constructors; the internal
representation is free to change.

### It is a uniform recursive tree

Because *everything* converts to `DynArray<T>`, including a braced list of
`DynArray<T>`, the type is a tree: a **leaf** is a single item (constant or
signal) or a signal-of-vector, and a **branch** is a list of children. This
syntax must work:

```cpp
DynArray<Widget> content = {
    someWidget,
    getDynArrayOfWidgets(),
    { otherWidget, getMoreWidgets() }
};
```

Nesting is not a special case in the consumer: the consumer sees the flattened
form and never walks the tree.

### It flattens to `AnySignal<std::vector<std::pair<size_t, T>>>`

The single lowering of a `DynArray<T>` is a signal of `(id, value)` pairs in
list order. Consumers reconcile against the ids and never look at the tree.

This is not a new shape. `dataBind` already returns exactly it for widgets
(`src/bqui/include/bqui/databind.h:22`), and the open PR #99 proposes
`AnySignal<std::vector<std::pair<size_t, Window>>>` for `App`'s window list.
`DynArray` generalises what those two arrived at independently.

### Ids come from a scoped allocator

Ids are minted by an **allocator owned by the consumer** — `App` owns the one
used for the window list — and are **reused across re-flattens**, so an item
that survives a change keeps its id.

Not a process-wide global, for two reasons: no global mutable state to make
thread-safe, and **deterministic per-tree ids**, which is what lets a test
assert on the id set — as PR #99's window tests do — instead of on whatever the
global counter happened to be at. Ids only need to be unique *within one tree*,
so a per-consumer allocator is sufficient.

If the scoped allocator proves too awkward to thread through — it must reach
every nested `DynArray` construction — falling back to a global atomic
(mirroring `bq::signal::makeUniqueId`, `src/bq/src/signal/datacontext.cpp:7`) is
an acceptable retreat. It is the fallback, not the plan.

### The reactive subtree: `AnySignal<DynArray<T>>`

This constructor makes a whole subtree swappable — conditional branches, a
section of a list that changes shape wholesale. The rule on swap:

> The new subtree's items are a **new set** and get **fresh ids**, unless
> upstream supplied identity (i.e. the new subtree contains a `forEach` that
> keys them).

That is the honest default. The framework cannot know that an item in the old
subtree "is" an item in the new one; guessing would be structural matching,
which this design rejects for the reasons in *Rationale* below. If a caller
wants continuity across a swap, it puts the keying inside the subtree where the
keys are actually known.

### C++ pitfalls to get right

These are design constraints on the implementation, not incidental details.

- **The "convertible to `T`" constructor must be SFINAE-constrained.** An
  unconstrained `template <typename U> DynArray(U&&)` is a better match than the
  copy and move constructors for a non-const lvalue `DynArray`, and it will also
  swallow `AnySignal<T>` and braced lists that were meant for the dedicated
  constructors. Constrain it on `std::is_convertible_v<U, T>` *and* exclude
  `DynArray` itself and every other accepted form. This is the single most
  likely way to get a mysterious compile error or a silently wrong overload.

- **Prefer an explicit `std::initializer_list<DynArray<T>>` constructor.** In
  braced initialisation an `initializer_list` constructor wins over everything
  else, so relying on a `vector<DynArray<T>>` constructor to catch braces is a
  trap — the `initializer_list` form must exist, and the `vector` form is the
  runtime-built sibling, not a substitute.

- **Define `{}` and `{x}` deliberately.** `{}` must mean *the empty list*, not a
  default-constructed `T` — say so and test it. `{x}` is ambiguous between "a
  one-element list containing `x`" and "the single item `x`"; the two flatten
  identically, so pick one reading, document it, and make sure the overload set
  actually produces it rather than leaving it to whichever constructor happened
  to be a better match.

## `forEach`

```
forEach(collection, keyFn, delegate) -> DynArray<U>
```

`forEach` is the producer of structural dynamism, and the replacement for the
`Collection` / `DataSource` / `dataBind` plumbing.

**collection** accepts `std::vector<T>`, `AnySignal<std::vector<T>>`, an
initializer list, or the existing `Collection<T>`.

**keyFn** maps an item to a key. The key must be usable in `std::map`, i.e.
ordered and comparable — `std::map` is the internal store, chosen for simplicity
over a hash map (no hash requirement on user keys, no `std::hash`
specialisations to write).

**delegate** is `(AnySignal<T>) -> U`, and that is the **only** form.
A `(T) -> U` shorthand was considered and deliberately rejected; the *Rationale*
section is that argument, and it is the most important part of this document.

### Behaviour

- **The delegate is invoked only when a new id is introduced.** It is never
  re-invoked because a value changed — a value change arrives through the
  `AnySignal<T>` the delegate already holds. This is the whole point.
- **The delegate is the last parameter** so it can be defaulted when only keying
  is wanted. *Wrinkle to settle:* with a signal-taking delegate, the identity
  default gives `U = AnySignal<T>`, so the defaulted call yields
  `DynArray<AnySignal<T>>` rather than `DynArray<T>`. That is defensible but not
  obviously what a caller expects; decide at implementation time whether the
  default flattens, or whether the defaulted overload is simply not offered.
- **`keyFn` runs for every item whenever the array signal changes.** The
  algorithm computes the new key set, drops the ids whose keys are absent, and
  preserves the id for every key that is still present.
- **A changed key means a new item.** The old key vanishes, so its id is evicted
  and its item removed; the new key is new, so it gets a new id and the delegate
  runs. This is a consequence of the rule above, not a separate one.
- **Eviction is strict.** When an item disappears its key→id mapping is dropped
  immediately; the resulting `DynArray` is a strict 1:1 mapping of the source
  array, with no lingering entries for items that might come back.
- **Duplicate keys**: assert in debug. In release, tolerate gracefully — the
  result may be suboptimal (an item may lose its identity and be rebuilt) but it
  must not crash, corrupt the mapping, or drop items.

### No index is passed to the delegate

Deliberately. An index is *positional*, and position is exactly what keyed
identity abandons. A delegate that closes over an index would have to re-run for
every item whose position shifted — inserting at the front would rebuild the
entire list, which is the behaviour `forEach` exists to avoid.

Callers that genuinely need an index get a small helper that **injects indices
upstream**, turning `AnySignal<vector<T>>` into `AnySignal<vector<pair<size_t,
T>>>` before `forEach` sees it. With that comes a warning worth stating loudly:

> **Key on the item's identity, not the injected index.** Keying on the index
> makes every key change when the list reorders, which discards every id and
> rebuilds everything — precisely the failure mode indices were avoiding.

### Skipping repeats is a property of the data, not a requirement on `T`

`bq::signal::Signal` already has the facility: **`tryCheck()`**
(`src/bq/include/bq/signal/signal.h:114` and `:133`), with the strict sibling
**`check()`** (`:128`), implemented by the `Check` node
(`src/bq/include/bq/signal/check.h`).

The semantics, verified:

- `check()` exists **only** when the signal's value type is equality-comparable
  (`btl::IsEqualityComparable`, `src/btl/include/btl/typetraits.h:78`); calling
  it on a non-comparable type is a hard "no member named `check`" error.
- `tryCheck()` exists always. When the type is comparable it *is* `check()`;
  when it is not, it is a **silent passthrough** returning an equivalent signal
  with no `Check` node inserted — no SFINAE failure, no diagnostic.
- What `Check` suppresses is the **change notification**, not the value: its
  `update()` downgrades the inner signal's `didChange` to `false` when the newly
  evaluated value compares equal to the cached one
  (`src/bq/include/bq/signal/check.h:44`). `nextUpdate` passes through.
- Comparison of a multi-value `SignalResult<Ts...>` is **all-or-nothing**, not
  per element (`src/bq/include/bq/signal/signalresult.h:117`): the trait is true
  only if every `T` is comparable, and a change in any element propagates all of
  them.

So "skip repeated values" is opt-in at the point of use, via `.tryCheck()` on
the per-item signal, and it degrades to a no-op rather than a compile error for
types without `operator==`. `forEach` therefore imposes **no `operator==`
requirement on `T`** — it neither calls `tryCheck()` on the caller's behalf nor
demands comparability. Note also that `tryCheck()` on a non-comparable type is
*silently* a no-op, which is a usability trap in its own right: a caller who
adds `.tryCheck()` expecting deduplication gets nothing and no warning. Worth a
Doxygen note on `tryCheck` when this lands.

## What this supersedes

The existing pipeline, and what `forEach` does instead:

**`Collection<T>`** (`src/bqui/include/bqui/collection.h:196`) is a mutable,
lock-guarded container whose writer API (`pushBack`, `update`, `erase`, `swap`,
`move`, `sort` — `collection.h:307-461`) mutates the vector under a spin lock and
synchronously invokes registered callbacks. Each item's `size_t` id is the
address of its heap-allocated value —
`reinterpret_cast<size_t>(iter_->ptr())` (`collection.h:138-141`) — which is
stable across reallocation but **not reproducible between runs**. That is
another argument for a scoped allocator with dense, deterministic ids: a test
can assert on the id set. `forEach` accepts a `Collection<T>` directly, so it
stays useful as a *source*; what goes away is its role as the only entry point
to a dynamic list.

**`DataSource<T>`** (`src/bqui/include/bqui/datasource.h:13`) is the event
protocol between the two: an `evaluate` function returning the current
`vector<pair<size_t, T>>` plus a stream of
`variant<Insert, Update, Erase, Swap, Move, Refresh>` (`datasource.h:53-56`).
**`dataSourceFromCollection`** (`src/bqui/include/bqui/datasourcefromcollection.h:11`)
is the only adapter into it. `forEach` needs no such protocol: it diffs key sets
from the array signal, so any `AnySignal<vector<T>>` is a source, and the six
event kinds collapse into one code path.

**`dataBind`** (`src/bqui/include/bqui/databind.h:22`) is `forEach` specialised
to `DataSource` and hard-wired to `AnyWidget`. It returns exactly the flattened
shape (`databind.h:22`). `forEach` is the same operator with the delegate
generalised to any `U`, the source generalised past `DataSource`, and the
identity supplied by a key function instead of by the collection.

**`dynamicBox`** (`src/bqui/include/bqui/dynamicbox.h:23`, the implementation
behind `hbox`/`vbox` — `src/bqui/src/widget/hbox.cpp:18`,
`src/bqui/src/widget/vbox.cpp:20`) consumes that flattened signal. It keeps its
job: `DynArray<AnyWidget>` flattens to the very signal it already takes, so it
needs at most an overload. What it should shed is its build-cache (below).

Existing callers are `src/bqui/test/databindtest.cpp:18` and
`src/testapp1/adder.cpp:84`; both are `Collection` → `dataSourceFromCollection`
→ `dataBind` → `vbox`, and both become a single `forEach`.

## Rationale: why the delegate must take a signal

A `(T) -> U` delegate is the obvious ergonomic shorthand, and every list API
that has one eventually wants "rebuild the item when its value changes, but
preserve state when the new structure matches the old." That is what we are
rejecting. It is not a matter of taste — it is infeasible in this architecture,
and the failure mode of approximating it is silent data corruption.

### Rebuilding mints new identities, and state is keyed by identity

A widget rebuild walks `Widget → Builder → Element → Instance`
(`docs/architecture.md`) and constructs **new signal control blocks** along the
way. Those controls mint a `btl::UniqueId` **in their own constructor**, from a
global counter:

- `InputControl`'s constructor: `id_(makeUniqueId())`
  (`src/bq/include/bq/signal/input.h:19`).
- `SharedControl`'s constructor: `id_(makeUniqueId())`
  (`src/bq/include/bq/signal/sharedcontrol.h:142`).
- `bq::signal::makeUniqueId` is a monotonically increasing global atomic
  (`src/bq/src/signal/datacontext.cpp:7-11`).

Per-context state is looked up by exactly that id:
`InputSignal::initialize` calls `context.findData<ContextDataType>(control_->id_)`
and, on a miss, creates fresh state (`src/bq/include/bq/signal/input.h:126-131`);
`SharedControl::initialize` does the same (`sharedcontrol.h:148-154`).
`DataContext::findData` is a plain map lookup keyed by `DataId = btl::UniqueId`
(`src/bq/include/bq/signal/datacontext.h:79-100`).

So: **a new control block is a new id is a miss in `findData` is fresh state.**
State is keyed by object identity, and rebuilding destroys that identity by
construction. This is not an oversight to be patched; it is how the context
works.

### Rebuilding inside the same `SignalContext` does not help

The tempting fix is "rebuild, but in the same context, so the state is still
there." It does not work, and there is already a live demonstration.

`dynamicBox` rebuilds its children inside a `.map()` — `std::move(widget.second)(params)`
invokes each child's whole build function (`src/bqui/include/bqui/dynamicbox.h:35`).
That map node is created **once**, when `WindowGlue` constructs its
`SignalContext` (`src/bqui/src/app.cpp:81-84`, member declared at `:438-439`),
and that context lives for the entire run — it is pumped once per frame at
`app.cpp:269` and torn down only at `app.cpp:509`. So every list change
re-executes the build inside the *same*, never-reinitialised context, and the
rebuilt children still get fresh state. The context is shared; the *ids* are
not, because the rebuild constructed new controls.

### Most state is not in the `DataContext` at all

`DataContext` holds only the state of id-bearing nodes (inputs, shared nodes).
The state of every ordinary combinator — `map`, `merge`, `withPrevious`, and the
`iterate` fold that turns a stream into a signal — lives in the **`DataType`
tree**, a plain nested struct hierarchy owned by the `SignalContext`:

- `Map::DataType` is `{ typename TSignal::DataType signalData; ... }` — the
  child's `DataType` embedded by value (`src/bq/include/bq/signal/map.h:23-32`),
  built by `initialize()` recursing into the child (`map.h:40-43`).
- `WithPrevious::DataType` holds the fold's running value (`currentResult`,
  `hasPrevious`) alongside the child's data
  (`src/bq/include/bq/signal/withprevious.h:59-78`).
- `SignalContext` builds that whole tree **once**, in its constructor, and owns
  it as `std::tuple<SignalDataTypeT<TSignals>...> data_`
  (`src/bq/include/bq/signal/signalcontext.h:41-43`, `:76-81`, `:124`).

That tree has no ids. It is addressed **positionally**, by the shape of the
`initialize()` recursion. It is created with the signal and dies with it. So
even a hypothetical id-preserving rebuild would still reset every `map` cache,
every `withPrevious`, and every `iterate` fold in the item's subgraph.

### Therefore structural matching is not just hard — it is wrong

"Preserve state when the structure matches" needs a key derived from the shape
of the signal graph. Two different items in a list produce *structurally
identical* graphs by design — that is what makes them a list. So a structural
key **collides**, and the consequence is not a rebuild, it is **silent
mis-association**: widget A's text and cursor position appearing inside widget B
after a reorder.

Streams are the worst case. An `iterate` fold and the `pipe` feeding it are
separate nodes; nothing guarantees they are preserved or recreated together, so
a structural match can preserve one and recreate the other — a fold reading from
a pipe that no longer feeds it. The type check in `findData`
(`datacontext.h:87-93`) does not save this: it only catches collisions between
*differently*-typed data, and same-shaped list items are the same type by
construction.

Silently attaching one item's state to another is worse than rebuilding. So the
delegate takes a signal, is invoked once per identity, and the question never
arises.

### The pattern already exists

`forEach` is not a new idea in this codebase; it is a generalisation of what the
existing machinery already does by hand.

**`dataBind` already builds each item's widget exactly once.** On the initial
evaluation and on every `Insert` it creates a per-item `makeInput<T>` and calls
`delegate(input.signal, id)`, storing the resulting widget together with the
input's handle (`src/bqui/include/bqui/databind.h:48-53`, `:69-77`). A later
`Update` does **not** call the delegate — it finds the item by id and pushes the
new value through that stored handle: `i->valueHandle.set(std::move(update.value))`
(`databind.h:83-91`). `Erase`, `Swap`, `Move`, and `Refresh` only reorder or
drop entries (`databind.h:93-187`); none of them rebuilds. The final `.map()`
projects the state to `vector<pair<size_t, AnyWidget>>` (`databind.h:199-209`).

That is the signal-taking delegate, hand-rolled: build once per identity, push
values through a handle, never rebuild.

**`dynamicBox` caches the built element by item id.** Its `withPrevious` fold
keeps, for each id present in the new builder list, the element it built
previously, and only constructs an element for ids it has not seen
(`src/bqui/include/bqui/dynamicbox.h:104-159` — the `prev.first == id` hit at
`:116-120` reuses the old element and `continue`s at `:123`).

`forEach` formalises exactly this pattern and moves it below the UI layer, where
it works for any `U` rather than only for widgets.

## Future directions (explicitly out of scope)

**Widgets carry no identity field.** `Widget`, `Builder`, and `Element` have no
identity of their own. Even with a per-item key from `forEach`, a delegate that
conditionally swaps subtree shapes — "show a text field when editing, a label
otherwise" — cannot express "the text field in branch A is the same text field
as in branch B." Threading identity through the widget chain is the sound route
to SwiftUI-style state preservation, and is strictly better than structural
matching because the identity is *declared* rather than inferred. Out of scope
here; noted so the option stays visible.

**`dynamicBox` wastes work.** Its two stages disagree. Stage 1 (widget →
builder, `dynamicbox.h:28-40`) re-invokes **every** child's build function on
**every** list change, allocating throwaway inputs, pipes, and ids. Stage 2
(builder → element, `dynamicbox.h:104-159`) then caches by id, so for every
pre-existing item the builder stage 1 just produced is dropped unused at the
`continue` on `dynamicbox.h:123-124`. The cache makes `dynamicBox` *correct*,
not *cheap*: one insert costs an O(n) build-and-discard. `.share()`
(`dynamicbox.h:40`) bounds it to once per changed frame, but not per changed
item. Worth cleaning up when this area is reworked; `forEach` removes the reason
for the pattern entirely. (Related, and cheap to fix while there: the per-id
`hints` signal at `dynamicbox.h:55-72` is computed, `.share()`d, and never used.)

**PR #99 (dynamic windows)** is open, and makes `App`'s window list
`AnySignal<std::vector<std::pair<size_t, Window>>>` with a caller-assigned
stable id. It is expected to be reworked onto `DynArray<Window>`, with `App`
owning the id allocator.

## Open questions

Points the design does not yet settle. Flagged rather than guessed.

- **The defaulted delegate's return type.** As noted above, defaulting a
  `(AnySignal<T>) -> U` delegate to identity yields `DynArray<AnySignal<T>>`,
  not `DynArray<T>`. Decide whether to flatten, to require the delegate, or to
  offer the default only where `DynArray<AnySignal<T>>` is what the consumer
  wants.
- **`{x}` — one-element list or single item?** They flatten identically, so the
  choice is about which constructor wins and what the error messages say. Pick
  one and write the test.
- **Where the id allocator is threaded.** "Owned by the consumer" is clear for
  `App`, but a `DynArray<T>` is constructed *before* it reaches a consumer, and
  nested `DynArray`s are constructed before their parent. Either flattening —
  not construction — assigns ids (the allocator is a parameter of `flatten`), or
  construction needs an ambient allocator. The first is cleaner and is the
  assumption the rest of this document makes; confirm it survives contact with
  `forEach`, which must remember key→id across flattens and therefore needs
  somewhere durable to keep that map.
- **Where `forEach`'s key→id map lives.** It is per-`forEach`-node state that
  must survive across updates, which in this architecture means a `DataType` in
  the `SignalContext` or a control block with an id. Which one is not decided,
  and it interacts with the previous point.
- **Duplicate-key tolerance is under-specified.** "Suboptimal but not
  catastrophic" needs a concrete rule — e.g. first occurrence wins the id,
  subsequent duplicates get fresh ids each flatten (and therefore rebuild).
  State the rule so the release behaviour is testable rather than accidental.
- **Whether `Collection<T>` and `DataSource<T>` are removed or kept.** `forEach`
  supersedes the *plumbing*; `Collection` remains a reasonable mutable source.
  Note that `Collection`'s ids are pointer values, so a `Collection` source must
  either feed its ids through as keys or be keyed on item content.

## Implementation order

Two commits, in this order:

1. **`DynArray<T>` core** — the type, its constructors, the tree, flattening,
   and the scoped id allocator. Independently useful: `App::windows` can consume
   it before `forEach` exists.
2. **`forEach`** — the keyed operator on top, plus the retirement path for
   `dataBind` / `dataSourceFromCollection` / `dynamicBox`'s caching layer.
