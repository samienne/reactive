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

`DynArray` is best understood as **its own domain alongside `Signal`**, entered
by `forEach` and left by `toSignal`; that framing is developed below and is what
tells us which operations must exist and what each does to identity. It also
fixes a real bug: `dynamicBox` today supports dynamic *membership* only, and
silently ignores a changed widget for an existing item.

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

Nesting is not a special case in the consumer: the consumer sees the exported
`(id, value)` list and never walks the tree.

### It leaves the domain as `AnySignal<std::vector<std::pair<size_t, T>>>`

The single exit from `DynArray<T>` back to `Signal` produces a signal of
`(id, value)` pairs in list order. Consumers reconcile against the ids and never
look at the tree.

This is not a new shape. `dataBind` already returns exactly it for widgets
(`src/bqui/include/bqui/databind.h:22`), and the open PR #99 proposes
`AnySignal<std::vector<std::pair<size_t, Window>>>` for `App`'s window list.
`DynArray` generalises what those two arrived at independently.

### Ids are assigned at the exit, by a scoped allocator

**Settled:** the allocator is constructed at the point the
`AnySignal<std::vector<std::pair<size_t, T>>>` is constructed, and is a
parameter of that operation. **Construction of a `DynArray` assigns no ids at
all**; the exit does. So an id is a property of *one exit's* view of the tree,
and the allocator's lifetime is the exit signal's lifetime.

This resolves what would otherwise be a genuine tension. A consumer-owned
allocator and a per-type constructor set cannot both be satisfied at
construction time: nested `DynArray`s are built before their parent, and the
parent before it ever reaches a consumer, so there is no point during
construction at which the consumer's allocator is in scope. Making the allocator
a parameter of the exit removes the problem instead of threading around it —
`DynArray` values stay pure descriptions that can be built, stored, and passed
anywhere, and nothing needs an ambient allocator.

Ids are **reused across re-exports**, so an item that survives a change keeps
its id. This requires the allocator to be *stateful and durable* — it lives with
the exit signal, not with a single evaluation.

Not a process-wide global, for two reasons: no global mutable state to make
thread-safe, and **deterministic per-exit ids**, which is what lets a test
assert on the id set — as PR #99's window tests do — instead of on whatever the
global counter happened to be at. Ids only need to be unique *within one exit*,
so a per-consumer allocator is sufficient. (A global atomic in the style of
`bq::signal::makeUniqueId`, `src/bq/src/signal/datacontext.cpp:7`, remains the
fallback if the scoped form proves unworkable — but the tension that motivated
that escape hatch is now gone.)

**Consequence for `forEach`'s key→id map.** That map is what makes an id
survive, so it lives wherever the allocator lives: in the durable state hanging
off the exit signal, created and destroyed with it. It is *not* per-evaluation
state, and it is not something the `DynArray` value carries — a `DynArray` used
by two exits legitimately has two independent id spaces and therefore two maps.
Exactly which mechanism holds that state (a `DataType` slot in the
`SignalContext`, or a control block with its own id, as `SharedControl` does) is
an implementation choice; the requirement is only that its lifetime match the
exit's.

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
  one-element list containing `x`" and "the single item `x`"; the two export the
  same list, so pick one reading, document it, and make sure the overload set
  actually produces it rather than leaving it to whichever constructor happened
  to be a better match.

## `DynArray` as a domain alongside `Signal`

`DynArray` is not a helper type bolted onto `Signal` — it is **its own domain**,
with its own operations, entered and left by explicit boundary operations:

```
                forEach(collection, keyFn, delegate)
    Signal  ──────────────────────────────────────────▶  DynArray
            ◀──────────────────────────────────────────
                        toSignal(allocator)
```

Naming this is worth doing because most of the structure is **already latent**
in the design above. Making it explicit tells us which operations must exist,
what their laws are, and — most usefully — what each one does to identity.

### The operations

- **`pure : T → DynArray<T>`** — the single-item constructor. Already in the
  table above.
- **Concatenation** — the `initializer_list` / `vector<DynArray<T>>`
  constructor, with the empty list as identity. This is a **monoid**, and its
  associativity law is not an abstraction: it is literally the brace-nesting
  ergonomics we require, `{a, {b, c}} ≡ {a, b, c}`. The syntax goal and the
  algebraic law are the same statement.
- **`join : DynArray<DynArray<T>> → DynArray<T>`** — already latent. A branch
  node *is* nesting, and collapsing the tree *is* join. It deserves a name
  because `flatMap` then falls out as `map` followed by `join`, with no new
  machinery.
- **`map : (T → U) → DynArray<T> → DynArray<U>`** — a lawful functor. See the
  caveat below: this is emphatically **not** the delegate form `forEach`
  rejects.
- **`filter`** — not required by anything here, but obviously wanted soon.
  Identity-preserving for the items that survive.

### The identity algebra

The functor and monoid laws say the operations compose predictably. The more
actionable guarantee is what each operation does to **ids** — this is the table
to reason from when asking "will this rebuild?":

| Operation | Effect on identity |
| --- | --- |
| `pure` | **creates** one id |
| concatenation / nesting | **namespaces** children — child ids stay distinct, order is preserved |
| `map` | **preserves** — same ids, transformed values |
| `filter` | **preserves** for survivors; dropped items' ids are evicted |
| `join` / `flatMap` | **namespaces** inner ids into the outer id space |
| `forEach` | **creates** ids, derived from keys |
| `toSignal` | **reveals** ids; creates nothing |

Read it as a single rule: **only `pure` and `forEach` mint identity.**
Everything else either carries identity through unchanged or re-namespaces it
without inventing any. That is what makes it possible to look at a pipeline and
say whether a given item will keep its state.

One clarification the table needs, given that ids are assigned at the exit:
inside the domain, identity is **symbolic** — a position in the tree, or a key —
and it is `toSignal` that resolves each symbolic identity to a concrete
`size_t`. "`pure` creates an id" means it introduces a new identity that the
exit will number; "`map` preserves ids" means it does not disturb the symbolic
identities, so the same items get the same numbers as they would have without
it. The two statements — "construction assigns no ids" and "`map` preserves
ids" — are consistent only under this reading, so state it in the Doxygen too.

### The boundary is asymmetric

Entering the domain **requires a key**; leaving it is free.

That asymmetry is not an accident of the API — it is forced. The `Signal` domain
does not carry identity: `AnySignal<std::vector<T>>` is a changing sequence of
values with no notion of which value "is" which across a change. Identity has to
come from somewhere, and only the caller knows what makes two items the same
thing. Hence `forEach` takes a `keyFn`.

Leaving is free because by then the ids exist; `toSignal` only has to *reveal*
them, and its allocator parameter is about which id space to reveal them in, not
about inventing identity.

This is the one-sentence answer to "why does `forEach` need a key function and
the exit does not?"

### Naming: `join`/`flatten` vs `toSignal`

Two distinct collapses exist and must not share a name:

- **`join`** (or `flatten`) — `DynArray<DynArray<T>> → DynArray<T>`. Stays
  inside the domain; collapses nesting.
- **`toSignal(allocator)`** — `DynArray<T> → AnySignal<vector<pair<size_t, T>>>`.
  Leaves the domain; assigns and reveals ids.

`resolve(allocator)` is an acceptable alternative for the second. What is not
acceptable is calling both of them `flatten`, which is the natural drift given
that both "flatten" something — and which would make every sentence about ids
ambiguous. Earlier drafts of this document used "flatten" for the exit; that
usage is retired.

### One operation, three call sites: apply-once-per-id

The pattern **build once per id → cache the result → reuse it on later
emissions** currently exists in three places:

1. `dynamicBox` caches built elements by id (`dynamicbox.h:104-159`).
2. `App`'s window reconcile in PR #99: create a `WindowGlue` on a new id,
   destroy it when the id vanishes, keep it otherwise.
3. `forEach` would be the third.

Three hand-rolled copies of one algorithm is two too many. **`DynArray` should
own a generic "apply `f` once per id, cache the result, reuse on later
emissions" operation**, and all three collapse onto it.

Layering survives this cleanly, which is the point worth stating: `bq` cannot
know how to build a widget or open a window, but it does not need to. "Apply
once per identity" is entirely generic; the widget-specific or window-specific
function stays at the call site in `bqui`. This is the same split that lets
`map` live in `bq` while the mapped function is the caller's.

Semantics, fixed to match the rest of the design:

- The cached value is **evicted when its id disappears** — consistent with the
  strict 1:1 mapping `forEach` guarantees.
- A **re-appearing key rebuilds.** There is no resurrection of evicted state; a
  key that goes away and comes back is a new item, exactly as a changed key is.

### `.map` is value-level; the build delegate is structure-level

This looks contradictory and is the most important caveat in the document: a
`(T) -> U` function is **banned** as the `forEach` delegate and **fine** as
`.map`. Same signature — different role.

- **`.map(f)` is value-level.** It is implemented as a per-element `sig.map(f)`,
  so the `Map` node is created **once per identity**, and on a value change `f`
  re-runs *inside* the already-built signal graph. Nothing is constructed, so
  nothing is destroyed. Lawful functor, no state loss.
- **The build delegate is structure-level.** Its `f` *constructs* something
  stateful — a widget with an input, a stream fold, a window. Re-invoking it
  means a rebuild, and a rebuild is what destroys state (see *Rationale*).

The rule to hold onto:

> **`.map` for transforming values; the `(AnySignal<T>) -> U` build form for
> constructing stateful things.**

The type system cannot tell these apart — `(T) -> U` is `(T) -> U` whether it
adds one to a number or spins up a text field with a cursor — so the
documentation has to. A `.map` whose function constructs a widget is the
footgun; that is the one thing to say in the Doxygen for `map`.

### `map` with extra signals

`dynArray.map(f, sig1, sig2)` — where `f` receives the item's value plus the
current values of those signals — should be supported. It is **`combine` sugar
per element**: the node is still created once per identity, and only values
recompute. It introduces **no new footgun class**, because the dangerous case is
already covered by the rule above ("don't construct in `.map`") and is
unaffected by how many values `f` receives.

The cost is worth stating: a shared signal feeding `n` items fans out to O(n)
value recomputations when it changes. That is inherent, not a design flaw — all
`n` outputs genuinely depend on it. It is a reason to reach for the alternative
below when the dependency is not really per-item, not a reason to omit the
feature.

**The complement, not the alternative.** Capturing signals in the lambda and
returning something that *observes* them is the **structure-level** form: the
signal is wired into the item once, at build time, and changes flow through the
existing graph rather than through a recomputation of `f`. Use `map`-with-extra-
signals when the item's *value* depends on them; use capture-and-observe when
the item's *construction* does. The two cover different needs.

**What `bq` has today**, verified:

- `Signal::map` takes **only** a function — `map(TFunc&& func)`, in `const&` and
  `&&` overloads (`src/bq/include/bq/signal/signal.h:225-237`). There is **no**
  extra-signals form.
- `combine` is homogeneous and vector-shaped: `combine(std::vector<AnySignal<T>>)
  → AnySignal<std::vector<T>>` (`src/bq/include/bq/signal/combine.h:73`). It
  fuses N signals *of the same type*.
- The heterogeneous fuse is `merge`, variadic over signal types, as a free
  function (`src/bq/include/bq/signal/merge.h:104`) and a member
  (`signal.h:307-310`). So the way to write "map over these extra signals"
  today is `a.merge(b, c).map(f)` — which is exactly the sugar being proposed,
  spelled out.

**Constraint:** `DynArray::map` must **mirror `Signal::map` exactly** — same
argument order, same arity handling, same name. The symmetry between the two
domains is the payoff of this whole framing; a `DynArray::map` that takes extra
signals while `Signal::map` does not would break it at the first thing anyone
tries.

Since `Signal::map` does **not** currently take extra signals, that constraint
argues for **adding the extra-signals form to `Signal::map` first** (as sugar
over `merge().map()`), and only then giving `DynArray::map` the matching shape.
Doing it the other way round would make the two domains diverge exactly where
the framing promises they agree.

## `forEach`

```
forEach(collection, keyFn, delegate) -> DynArray<U>
```

`forEach` is the producer of structural dynamism, the **entry** into the
`DynArray` domain, and the replacement for the `Collection` / `DataSource` /
`dataBind` plumbing. It is the only operation other than `pure` that mints
identity.

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
  default unwraps it, or whether the defaulted overload is simply not offered.
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
  must not crash, corrupt the mapping, or drop items. The concrete release rule
  is still open (see *Open questions*).
- **The delegate is run by the generic apply-once-per-id operation** described
  above, not by machinery of its own; `forEach` supplies the key→id mapping and
  delegates the caching to it.

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

C++20's defaulted `operator==` and `operator<=>` would soften this considerably
— a user could opt a type into deduplication with one line rather than writing
a comparison. The project targets **C++17**, so the design must not depend on
that, and the silent-no-op trap is real for us today. Noted only so that the
ergonomics are understood to improve on their own if the standard level ever
moves.

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
to `DataSource` and hard-wired to `AnyWidget`. It returns exactly the exported
shape (`databind.h:22`). `forEach` is the same operator with the delegate
generalised to any `U`, the source generalised past `DataSource`, and the
identity supplied by a key function instead of by the collection.

**`dynamicBox`** (`src/bqui/include/bqui/dynamicbox.h:23`, the implementation
behind `hbox`/`vbox` — `src/bqui/src/widget/hbox.cpp:18`,
`src/bqui/src/widget/vbox.cpp:20`) consumes that exported signal. It keeps its
job: `toSignal` on a `DynArray<AnyWidget>` produces the very signal it already
takes, so it needs at most an overload. What it should shed is its build-cache (below).

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

### `dynamicBox` supports dynamic membership only — and `forEach` fixes it

This is a correctness finding, not just a design argument.

`dynamicBox`'s cache is keyed by id and consulted *before* anything looks at the
widget. For each entry in the incoming builder list it searches the previous
result for the same id; on a hit it reuses the old element and `continue`s
(`src/bqui/include/bqui/dynamicbox.h:113-124`). The builder that stage 1 just
produced from the *current* widget is dropped on the floor.

So if an existing id's **widget changes** — same item, different widget — the
cache hits, the new widget is discarded, and **the change is silently ignored**.
No assert, no diagnostic, no visible failure: the old widget simply stays on
screen. `dynamicBox` presents itself as a container of dynamic widgets, but what
it actually supports is dynamic **membership**. Items can come and go; an item's
widget cannot change.

And it **cannot do better as written**. The only way to honour the new widget is
to use the new builder — that is, to rebuild — and rebuilding is exactly what
destroys the item's state (the whole of this section). Ignoring the change and
losing the user's typing are the only two options on offer. The cache picks the
first, which is the less bad one, but both are wrong.

`forEach`'s `(AnySignal<T>) -> U` **closes the hole** rather than choosing
between the two failures. The widget is built once per identity, and value
changes flow *through the item signal into the already-built widget* instead of
arriving as a replacement widget that has to be either applied or discarded.
There is nothing left to ignore, because a changed value never presents itself
as a new widget in the first place.

That makes `forEach` a **fix**, not merely a tidier factoring of the same
behaviour — and it is the reason this work is worth doing beyond the
code-sharing argument.

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
This is the *performance* half of the same cache; the correctness half — that
the discarded builder may carry a genuinely changed widget — is in *Rationale*
above and is not merely a cleanup.

**PR #99 (dynamic windows)** is open, and makes `App`'s window list
`AnySignal<std::vector<std::pair<size_t, Window>>>` with a caller-assigned
stable id. It is expected to be reworked onto `DynArray<Window>`, with `App`
owning the id allocator.

## Open questions

Points the design does not yet settle. Flagged rather than guessed.

- **The defaulted delegate's return type.** As noted above, defaulting a
  `(AnySignal<T>) -> U` delegate to identity yields `DynArray<AnySignal<T>>`,
  not `DynArray<T>`. Decide whether to unwrap it, to require the delegate, or to
  offer the default only where `DynArray<AnySignal<T>>` is what the consumer
  wants.
- **`{x}` — one-element list or single item?** They export the same list, so the
  choice is about which constructor wins and what the error messages say. Pick
  one and write the test.
- **Duplicate-key tolerance is under-specified.** "Suboptimal but not
  catastrophic" needs a concrete rule. *Candidate:* the first occurrence keeps
  the id; subsequent duplicates get fresh ids on each export, and therefore
  rebuild. That satisfies "must not fail catastrophically" and keeps the 1:1
  mapping, at the cost of churn for the duplicates. Not adopted — state the rule
  before implementing, so the release behaviour is testable rather than
  accidental.
- **Whether `Collection<T>` and `DataSource<T>` are removed or kept.** `forEach`
  supersedes the *plumbing*; `Collection` remains a reasonable mutable source.
  Note that `Collection`'s ids are pointer values, so a `Collection` source must
  either feed its ids through as keys or be keyed on item content.
- **The reactive subtree looks like a third minting site.** The identity algebra
  says only `pure` and `forEach` mint identity, but the `AnySignal<DynArray<T>>`
  constructor hands out **fresh ids on every swap**. Either that constructor is
  an acknowledged third minting site — in which case the table needs a row and
  the "single rule" needs rewording — or the swap should be modelled as `join`
  over a signal-of-`DynArray`, with the fresh ids coming from re-entering the
  new subtree's own `pure`s. The second is tidier and probably intended, but it
  is not what the current wording says. Reconcile before implementing.
- **`filter` and the apply-once-per-id cache disagree about eviction.** Two
  rules collide when an item is filtered out and later filtered back in.
  Upstream, `forEach`'s key→id map still holds the item (it never left the
  *source* array), so its identity is unchanged. Downstream, the cache evicted
  the built value when the id vanished from its input, and "a re-appearing key
  rebuilds." The result is that the delegate re-runs for an id that was never
  retired — which contradicts "the delegate is invoked only when a new id is
  introduced," and means a filtered-out widget silently loses its state. Decide
  whether `filter` evicts from the shared identity space or only masks its own
  output, and say which guarantee survives.

## Implementation order

Two commits, in this order:

1. **`DynArray<T>` core** — the type, its constructors, the tree, `join`,
   `toSignal` and the scoped id allocator, and the generic apply-once-per-id
   operation. Independently useful: `App::windows` can consume it before
   `forEach` exists.
2. **`forEach`** — the keyed operator on top, plus the retirement path for
   `dataBind` / `dataSourceFromCollection` / `dynamicBox`'s caching layer.
