# Design: `ArraySignal<T>`, `forEach`, and one layout engine

> **Status: proposal — not implemented.** Nothing described here exists in the
> tree yet. This document is the agreed design, recorded so the implementation
> has a target and so the reasoning survives. Once it is built, the stable parts
> move to their normal homes — the model and its traps to
> `docs/conventions.md`, the settled *why* to `docs/decisions.md`, and the API
> contract to Doxygen in the new public headers — and this file goes away.

*Last verified against `93357c3` (2026-07-19).*

## The problem

A widget list whose *membership* changes at runtime cannot be expressed as a
plain derivation. Deriving `vector<AnyWidget>` from `vector<T>` rebuilds every
widget on every change, and rebuilding a widget destroys its state: the text a
user typed, a cursor position, an animation in flight, a stream fold. The
existing answer — `Collection` + `dataSourceFromCollection` + `dataBind` +
`dynamicBox` — works, but it is a special-purpose pipeline bolted to one widget
container, it is not composable, and it lives in `bqui` where only widgets can
use it.

This design replaces that pipeline with three pieces:

- **`ArraySignal<T>`** — a value in `bq` describing a list whose contents and
  structure may both be dynamic. It is the *input* type a list-consuming API
  accepts.
- **`forEach`** — the keyed operator that turns a changing collection into an
  `ArraySignal<U>` with stable per-item identity, invoking a delegate once per new
  item rather than once per change.
- **`KeyedArraySignal<A>`** plus the free functions `extract` and `scatter` — the
  fan-in / fan-out pair that lets a *container* compute over all of its children
  at once and hand each child its own share of the result, without ever exposing
  an identity.

`ArraySignal` is best understood as **its own domain alongside `Signal`**, entered
by `forEach` and left by `values`; that framing is developed below and is what
tells us which operations must exist and what each does to identity.

Two payoffs beyond the code sharing:

- It **fixes a real bug**: `dynamicBox` today supports dynamic *membership* only,
  and silently ignores a changed widget for an existing item.
- It **unifies the static and dynamic layout engines**. `layout()`'s own type
  aliases turn out to be this API already, so `hbox`, `vbox`, `stack`,
  `uniformGrid` and `dynamicBox` become one implementation rather than two that
  drift apart. That finding is the headline of this document; see *One layout
  engine*.

## Naming

The type was called `DynArray<T>` in earlier drafts. It is now **`ArraySignal<T>`**,
and the keyed aggregate is **`KeyedArraySignal<A>`**.

`DynArray` reads like a mutable container — `std::dynarray` was a proposed
`std::vector` sibling — and that mis-sells the type completely: it is immutable,
it carries no storage the caller can poke at, and it belongs in the same
category as `Signal`. Naming it `ArraySignal` puts it there.

### There is deliberately no `AnyArraySignal`

In `bq::signal` the `Any` prefix means **type-erased**: `Signal<TStorage, Ts...>`
carries its storage type in the type (`signal.h:160`) and `AnySignal<Ts...>`
erases it (`signal.h:372`, a class deriving from `Signal<void, Ts...>`). The
storage-typed form exists so a chain of combinators stays free of virtual
dispatch.

`ArraySignal` is **type-erased by construction** and never wants a storage-typed
twin. An array is heap-backed anyway: the per-element indirection that
storage-typing avoids for `Signal` is already paid here, so a
`Signal`-style split would buy nothing and double the surface. **Do not add an
`AnyArraySignal` alias, and do not "restore the symmetry" with `Signal` by
introducing a storage parameter.** The asymmetry is the decision, not an
oversight.

### `KeyedArraySignal` is a sibling, not a subtype

`KeyedArraySignal<A>` is **not** an `ArraySignal<A>` with extra fields, and there
is no subtyping between them. It is a separate type with a different operation
set:

- `ArraySignal<T>` is **per-element**: `forEach`, `map`, `concat`.
- `KeyedArraySignal<A>` is **whole-vector**: `mapValues`, `mapKeyed`, `values`.

Reaching for `forEach` on a `KeyedArraySignal` is the natural mistake and it
will not compile — deliberately. A `KeyedArraySignal` exists to be operated on
*as a vector*, because that is what a layout computation needs; it re-enters the
per-element world only through `scatter`.

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

`ArraySignal<T>` expresses both. Its literal forms (a value, a signal, a nested
braced list) are value-dynamic. Structural dynamism enters a `ArraySignal` only
through `forEach` — or through the reactive-subtree constructor, which is
structural dynamism at the coarsest possible granularity (the whole subtree is
replaced).

## `ArraySignal<T>`

`ArraySignal<T>` lives in `bq`. It has no UI dependency; `bqui` merely consumes it.

### It is a description, not a container

A `ArraySignal<T>` is an **immutable value**. It has no `push_back`, no `erase`, no
mutation of any kind. Dynamism flows *in* from upstream: a `Collection<T>`
converted into a `ArraySignal<T>` stays live because the conversion produced a
signal that the collection drives — mutating the collection changes the
`ArraySignal`'s signal, not the `ArraySignal` object.

This is the same discipline as the rest of the toolkit (`docs/architecture.md`):
descriptions are immutable values, state lives upstream.

### A constructor per supported type — deliberately not a variant

The natural implementation is `std::variant` over the accepted forms. We are
**not** doing that. Instead `ArraySignal<T>` gets one converting constructor per
supported source type:

| Constructor takes | Meaning |
| --- | --- |
| `T` (or anything convertible to `T`) | a single constant item |
| `AnySignal<T>` | a single item whose value varies |
| `AnySignal<std::vector<T>>` | a varying list of items |
| `std::initializer_list<ArraySignal<T>>` | a fixed list of children |
| `std::vector<ArraySignal<T>>` | ditto, built at runtime |
| `AnySignal<ArraySignal<T>>` | a reactive subtree (see below) |

Two reasons. First, **acceptance is precisely controlled**: each form is opted
into by writing a constructor, so an accidentally-convertible type cannot slip
in, and the error message when something is not accepted names the type rather
than a variant alternative list. Second, **braces stay clean** — a variant
parameter forces the caller to name the alternative at every call site, which
defeats the whole point of a braced literal.

The public surface is deliberately just the constructors; the internal
representation is free to change.

### It is a uniform recursive tree

Because *everything* converts to `ArraySignal<T>`, including a braced list of
`ArraySignal<T>`, the type is a tree: a **leaf** is a single item (constant or
signal) or a signal-of-vector, and a **branch** is a list of children. This
syntax must work:

```cpp
ArraySignal<Widget> content = {
    someWidget,
    getArraySignalOfWidgets(),
    { otherWidget, getMoreWidgets() }
};
```

Nesting is not a special case in the consumer: a consumer operates through
`forEach`, `map`, `extract` and `scatter`, all of which see a flat sequence of
identified elements, and never walks the tree.

Note that the *stated public requirement* is only that `{a, b, c}` compiles for
values of `T`. The `initializer_list` element type being `ArraySignal<T>` rather
than `T` is how that requirement is met **and** how nesting falls out for free,
since every accepted form converts to `ArraySignal<T>`.

### Identity is internal, and the id space belongs to the `ArraySignal`

**Settled:** the id allocator is **constructed at the point the `ArraySignal` is
constructed**, and is scoped to it. Ids are unique within one `ArraySignal` and
mean nothing outside it.

This is a reversal of an earlier draft, which put the allocator at the *exit*
because ids were the consumer's business — a consumer reconciled against them,
so a consumer-scoped, deterministic id space was what tests could assert on. The
operator set below removes that requirement: identity is carried by
`KeyedArraySignal`, `extract` and `scatter` match within a single `ArraySignal`'s
space, and **an id never reaches user code**. Once nobody outside can see an id,
the natural owner of the id space is the array itself, and the tension the
earlier draft was working around disappears with it.

That tension was real, so record why it no longer bites. A *consumer*-owned
allocator cannot be satisfied at construction time: nested `ArraySignal`s are
built before their parent, and the parent before it ever reaches a consumer, so
there is no point during construction at which the consumer's allocator is in
scope. An *array*-owned allocator has no such problem — each `ArraySignal`
allocates in its own space as it is built, and concatenation re-namespaces
children rather than needing a shared counter.

Consequences:

- Ids are **reused across evaluations**, so an item that survives a change keeps
  its id. The allocator is therefore *stateful and durable*: it lives with the
  `ArraySignal`'s durable state, not with a single evaluation.
- **`forEach`'s key→id map lives in that same state.** It is not per-evaluation
  state and it is not something a user-visible value carries. Which mechanism
  holds it (a `DataType` slot in the `SignalContext`, or a control block with its
  own id as `SharedControl` does) is an implementation choice; the requirement is
  only that its lifetime match the `ArraySignal`'s.
- Not a process-wide global, for the same two reasons as before: no global
  mutable state to make thread-safe, and deterministic ids so that a test can
  assert on the id set rather than on whatever the global counter happened to be
  at. (A global atomic in the style of `bq::signal::makeUniqueId`,
  `src/bq/src/signal/datacontext.cpp:7-11`, remains the fallback if the scoped
  form proves unworkable.)

**What still needs an id-revealing exit.** PR #99 gives `App` a window list of
`AnySignal<std::vector<std::pair<size_t, Window>>>`, and `dataBind` already
returns exactly that shape for widgets
(`src/bqui/include/bqui/databind.h:22`). Those consumers reconcile against ids
directly. Under this design they are served by `extract`/`scatter` like any
other consumer — the reconcile *is* apply-once-per-identity — but if a raw
id-pair exit survives for them, it reveals the `ArraySignal`'s own ids rather
than minting a per-consumer space. See *Open questions*.

### The reactive subtree: `AnySignal<ArraySignal<T>>`

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
  unconstrained `template <typename U> ArraySignal(U&&)` is a better match than the
  copy and move constructors for a non-const lvalue `ArraySignal`, and it will also
  swallow `AnySignal<T>` and braced lists that were meant for the dedicated
  constructors. Constrain it on `std::is_convertible_v<U, T>` *and* exclude
  `ArraySignal` itself and every other accepted form. This is the single most
  likely way to get a mysterious compile error or a silently wrong overload.

- **Prefer an explicit `std::initializer_list<ArraySignal<T>>` constructor.** In
  braced initialisation an `initializer_list` constructor wins over everything
  else, so relying on a `vector<ArraySignal<T>>` constructor to catch braces is a
  trap — the `initializer_list` form must exist, and the `vector` form is the
  runtime-built sibling, not a substitute.

- **Define `{}` and `{x}` deliberately.** `{}` must mean *the empty list*, not a
  default-constructed `T` — say so and test it. `{x}` is ambiguous between "a
  one-element list containing `x`" and "the single item `x`"; the two export the
  same list, so pick one reading, document it, and make sure the overload set
  actually produces it rather than leaving it to whichever constructor happened
  to be a better match.

## The API surface: two layers

Most callers need four things: build one, iterate it, concatenate, transform.
Layout implementors need two more, and those two are sharp enough that an
ordinary caller reaching for them is almost always a mistake. So the surface is
split across **two headers**:

| Header | Namespace | Contents |
| --- | --- | --- |
| `<bq/signal/arraysignal.h>` | `bq::signal` | `ArraySignal<T>` and its constructors (`std::initializer_list`, `std::vector<T>`, `AnySignal<...>`); `forEach(keyFn, delegate)`; `concat`; `map` |
| `<bq/signal/arraysignallayout.h>` | a nested namespace, e.g. `bq::signal::layout` | the free functions `extract` and `scatter`, plus `KeyedArraySignal<A>`'s operations |

### Why not the `src/`-vs-`include/` firewall

The project's usual way to hide something is to keep it out of
`src/<lib>/include/`, which makes it uncompilable from any other library
(`docs/conventions.md`). **That mechanism cannot be used here.** `ArraySignal`
lives in `bq`; layouts live in `bqui`; `bqui` can only include `<bq/...>` public
headers. Anything internal to `bq` is unreachable from a layout, and layouts are
precisely the code that needs `extract` and `scatter`. The firewall is
inter-library, and this split is intra-audience — the wrong tool.

So the separation is by **header and namespace**, which is advisory rather than
enforced. That is the correct strength: an implementor who writes the extra
include has declared intent, and grep finds every such site.

### Why the advanced operations must be free functions

In C++ a class's member functions are all declared in one class definition.
There is no way to add members in a second header, and no way to make a member
available only to code that opted in. **Gating the surface therefore requires
that the gated operations not be members.** `extract` and `scatter` are free
functions for that reason, not for aesthetics.

`KeyedArraySignal` being a separate type does most of the work for free:
`mapValues`, `mapKeyed` and `values` are members of a type an ordinary caller
never obtains, so they are already out of reach. Only `extract` and `scatter` —
which take or return an `ArraySignal` — need moving off the class.

### The layering test

**`forEach` must be implementable in terms of the advanced operations.** If the
basic layer needs machinery the advanced layer cannot express, the split is
cosmetic and the "advanced" header is not actually the lower layer — it is a
sibling, and the document should say so instead.

Concretely, what the test comes down to is that `forEach`'s
once-per-identity delegate invocation and `extract`'s once-per-identity
initialisation are **the same primitive**, used once for fan-out and once for
fan-in. This is not yet demonstrated; see *Open questions*.

## The operator set

Two groups. The first is the everyday surface; the second is what a container
implementation needs.

### Basic

- **`ArraySignal<T>{...}` / from `vector<T>` / from `AnySignal<...>`** — the
  constructors. The single-item form is `pure : T → ArraySignal<T>`.
- **`concat`** — the `initializer_list` / `vector<ArraySignal<T>>` form, with the
  empty array as identity. This is a **monoid**, and its associativity law is not
  an abstraction: it is literally the brace-nesting ergonomics we require,
  `{a, {b, c}} ≡ {a, b, c}`. The syntax goal and the algebraic law are the same
  statement.
- **`map : (T → U) → ArraySignal<T> → ArraySignal<U>`** — a lawful functor,
  value-level. See *`forEach` vs `map`*.
- **`forEach(keyFn, delegate)`** — the keyed operator; identity-level.
- **`join : ArraySignal<ArraySignal<T>> → ArraySignal<T>`** — latent in the tree
  shape: a branch node *is* nesting, and collapsing the tree *is* join. Worth a
  name because `flatMap` then falls out as `map` followed by `join`, with no new
  machinery.
- **`filter`** — not required by anything here, but obviously wanted soon.
  Identity-preserving for the items that survive.

### Advanced: the fan-in / fan-out pair

- **`extract(f : T → AnySignal<A>) → KeyedArraySignal<A>`** — order-preserving
  fan-in. `f` is applied **once per identity**, so the per-element signal it
  returns is initialised once and thereafter merely updates. For a layout, `f` is
  "give me this child's size hint".
- **`mapValues(g : vector<A> → vector<B>, extras...) → KeyedArraySignal<B>`** —
  the maths. `g` sees every child's value at once and returns one value per
  child; identity rides along untouched. `extras...` are ambient signals injected
  alongside the vector — for a layout, the container's own size. This is where a
  layout's obb computation goes.
- **`mapKeyed(vector<pair<Key, A>> → vector<pair<Key, B>>)`** — the same, for
  computations that **reorder or drop**. `Key` is opaque: comparable and hashable,
  and otherwise meaningless. Nothing may be inferred from its value, its ordering
  relative to other keys, or its stability across runs.
- **`values() → AnySignal<vector<A>>`** — the one-way escape back to a plain
  signal, discarding identity. **Every** layout needs it, for upward size-hint
  propagation: the container's own hint is a pure function of its children's
  hints and has no per-child result to scatter back.
- **`scatter(keyed, f) → ArraySignal<U>`** — fan-out, where `f` is
  `(AnySignal<T>, AnySignal<B>) -> U`: exactly `map`'s delegate plus the
  identity-matched slice of the aggregate. For a layout, `f` is "build this child
  against the obb that was computed for it".

### `scatter` is `map` with one extra, identity-matched argument

That is the sentence that makes the operator obvious. `map` gives the delegate
the element's own value; `scatter` gives it the element's own value *and* the
element's own share of a previously computed aggregate.

It can be hand-rolled as `map` plus an id lookup — that is exactly what
`dynamicBox` does today, at `dynamicbox.h:108-122`, where each child maps the
shared obb list and linearly searches it for its own id. Three things are wrong
with the hand-rolled form:

- **It is O(n) per element per emission**, so O(n²) per layout change.
- **Missing entries are undefined.** `dynamicBox` asserts and returns a
  default-constructed `avg::Obb` (`dynamicbox.h:118-119`) — in release, a
  zero-sized box, silently.
- **Nothing stops a lookup in a foreign id space.** The ids are plain `size_t`;
  a list from a different container type-checks perfectly.

`scatter` does the match **once, in one pass**, at construction.

### The distinguished type is what removes the hazard

The aggregate could have been a plain `AnySignal<vector<pair<Id, A>>>`. Making it
a **distinguished type** — `KeyedArraySignal<A>` — is what turns the matching
hazard into a compile error:

- Only something `extract` produced can be scattered back.
- Ids never surface, so nothing can be keyed on them by accident and no caller
  can invent one.
- The alignment invariant is **carried by the type** rather than by
  `assert(obbs.size() == builders.size())` (`dynamicbox.h:70`).

That last point is the whole argument for a new type rather than a type alias.

### Multi-value `ArraySignal` was considered and rejected

`Signal<Ts...>` is variadic, and mirroring that with `ArraySignal<Ts...>` was
considered, mainly to avoid making callers write `std::pair` when an element
naturally carries two things.

That motivation **evaporated once `scatter` took a function**: the delegate
receives two separate signal arguments, so the common case — value plus
aggregate slice — never needed a pair in the first place. The general case is
recovered with a `makePair`-style helper at the call site. Variadic
`ArraySignal` would multiply every operator signature above for a problem that
no longer exists.

### The identity algebra

The functor and monoid laws say the operations compose predictably. The more
actionable guarantee is what each operation does to **ids** — this is the table
to reason from when asking "will this rebuild?":

| Operation | Effect on identity |
| --- | --- |
| `pure` | **creates** one id |
| `concat` / nesting | **namespaces** children — child ids stay distinct, order is preserved |
| `map` | **preserves** — same ids, transformed values |
| `filter` | **preserves** for survivors; dropped items' ids are evicted |
| `join` / `flatMap` | **namespaces** inner ids into the outer id space |
| `forEach` | **creates** ids, derived from keys |
| `extract` | **preserves** — the `KeyedArraySignal` carries the input's ids |
| `mapValues` | **preserves** — identity rides along, values change |
| `mapKeyed` | **preserves** for the keys the function returns; others are dropped |
| `values` | **discards** — the result is a plain signal with no identity |
| `scatter` | **preserves** — re-enters `ArraySignal` with the ids it matched on |

Read it as a single rule: **only `pure` and `forEach` mint identity.**
Everything else either carries identity through unchanged, re-namespaces it
without inventing any, or drops it entirely (`values`). That is what makes it
possible to look at a pipeline and say whether a given item will keep its state.

### The boundary is asymmetric

Entering the domain **requires a key**; leaving it is free.

That asymmetry is not an accident of the API — it is forced. The `Signal` domain
does not carry identity: `AnySignal<std::vector<T>>` is a changing sequence of
values with no notion of which value "is" which across a change. Identity has to
come from somewhere, and only the caller knows what makes two items the same
thing. Hence `forEach` takes a `keyFn`.

Leaving is free because by then the identities exist and `values()` merely
forgets them. Forgetting needs no information; remembering does.

This is the one-sentence answer to "why does `forEach` need a key function and
`values()` does not?"

### Naming: `join` vs `values`

Two distinct collapses exist and must not share a name:

- **`join`** (or `flatten`) — `ArraySignal<ArraySignal<T>> → ArraySignal<T>`.
  Stays inside the domain; collapses nesting; identity survives.
- **`values()`** — `KeyedArraySignal<A> → AnySignal<vector<A>>`. Leaves the
  domain; identity is discarded.

What is not acceptable is calling both of them `flatten`, which is the natural
drift given that both "flatten" something — and which would make every sentence
about identity ambiguous. Earlier drafts used "flatten" for the exit; that usage
is retired.

### One operation, three call sites: apply-once-per-id

The pattern **build once per id → cache the result → reuse it on later
emissions** currently exists in three places:

1. `dynamicBox` caches built elements by id (`dynamicbox.h:85-141`).
2. `App`'s window reconcile in PR #99: create a `WindowGlue` on a new id,
   destroy it when the id vanishes, keep it otherwise.
3. `forEach` would be the third.

Three hand-rolled copies of one algorithm is two too many. **`ArraySignal` should
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

## `forEach` vs `map` — the central distinction

Both take a function from an element to something else. They are not
interchangeable, and confusing them is the single easiest way to write code that
looks right and silently throws away user state.

- **`forEach` is identity-level.** Its delegate is `(AnySignal<T>) -> U`, and it
  is invoked **once per key**. It is *not* re-invoked when the item's value
  changes — the new value arrives through the signal the delegate already holds.
  Nothing is constructed on a value change, so nothing is destroyed. **This is
  what preserves widget state.**
- **`map` is value-level.** Its function is `T -> U`, re-run on every value
  change. Implemented as a per-element `sig.map(f)`, so the `Map` node is created
  once per identity and `f` re-runs *inside* the already-built graph. Perfect for
  data; ruinous for widgets, because building a widget here reconstructs it — and
  everything it holds — on every change.

The rule, stated plainly, is the one sentence to carry out of this document:

> **Transform data in `map`; build widgets in `forEach`.**

The type system cannot tell these apart — `(T) -> U` is `(T) -> U` whether it
adds one to a number or spins up a text field with a cursor — so the
documentation has to. A `map` whose function constructs a widget is the footgun;
that is the one thing to say in the Doxygen for `map`.

### Why `map` stays public

The tempting safety move is to hide `map` and let `forEach` cover everything.
That is worse. `forEach` **mints an identity** for every element, and an identity
is a commitment: a key to compute, a key→id entry to keep, an eviction rule to
reason about. Forcing a pure `T -> U` data transform through `forEach` pays all
of that for something that has no state to preserve, and — more importantly —
teaches callers that `forEach` is the default, which makes the identity-level /
value-level distinction invisible exactly where it matters.

`map` is public because the choice between the two must be *made*, not defaulted
into.

### `map` with extra signals

`arraySignal.map(f, sig1, sig2)` — where `f` receives the item's value plus the
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

**Constraint:** `ArraySignal::map` must **mirror `Signal::map` exactly** — same
argument order, same arity handling, same name. The symmetry between the two
domains is the payoff of this whole framing; a `ArraySignal::map` that takes extra
signals while `Signal::map` does not would break it at the first thing anyone
tries.

Since `Signal::map` does **not** currently take extra signals, that constraint
argues for **adding the extra-signals form to `Signal::map` first** (as sugar
over `merge().map()`), and only then giving `ArraySignal::map` the matching shape.
Doing it the other way round would make the two domains diverge exactly where
the framing promises they agree.

## `forEach`

```
forEach(collection, keyFn, delegate) -> ArraySignal<U>
```

`forEach` is the producer of structural dynamism, the **entry** into the
`ArraySignal` domain, and the replacement for the `Collection` / `DataSource` /
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
  `ArraySignal<AnySignal<T>>` rather than `ArraySignal<T>`. That is defensible but not
  obviously what a caller expects; decide at implementation time whether the
  default unwraps it, or whether the defaulted overload is simply not offered.
- **`keyFn` runs for every item whenever the array signal changes.** The
  algorithm computes the new key set, drops the ids whose keys are absent, and
  preserves the id for every key that is still present.
- **A changed key means a new item.** The old key vanishes, so its id is evicted
  and its item removed; the new key is new, so it gets a new id and the delegate
  runs. This is a consequence of the rule above, not a separate one.
- **Eviction is strict, and evicted items are destroyed.** When an item
  disappears its key→id mapping is dropped immediately and whatever the delegate
  built for it is destroyed — no retention, no pool, no resurrection. The
  resulting `ArraySignal` is a strict 1:1 mapping of the source array. A key that
  goes away and comes back is a new item, exactly as a changed key is.
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

## Contracts and traps

Each of these is a rule an implementor or a caller can get wrong without any
diagnostic firing. They are separated out so they can be lifted into
`docs/conventions.md` verbatim when this lands.

### `mapValues`: same size, positionally aligned, against the *current* input

**Contract.** The vector `g` returns must have the same size as the vector it was
given, and its element *i* must correspond to input element *i*. The input is the
current membership, not the membership at the time the node was built.

**Enforcement** is the size check — exactly the strength of today's
`assert(obbs.size() == builders.size())` (`dynamicbox.h:70`). This design does
**not** claim to make the contract statically checkable, and it would be
dishonest to imply it does.

What improves is *where* the assumption lives. Today it spans a re-keying step,
a `withPrevious` element cache and a per-child id lookup, so a violation can be
introduced in any of three places and only shows up at the assert. Under
`mapValues` the assumption is confined to **one pure function that holds both
vectors at once** — it can be read, unit-tested, and reasoned about without a
signal graph.

Positional alignment is exactly right for a layout, because a layout function is
order-preserving and same-size *by construction*: one obb per child, in child
order. When a computation genuinely reorders or drops elements, that is what
`mapKeyed` is for — reach for it rather than trying to smuggle a permutation
through `mapValues`.

### Folding over a `KeyedArraySignal` is position-keyed, and positions are not stable

This is the trap most likely to be walked into, because the code that walks into
it looks like an obvious optimisation.

`withPrevious` hands the fold a `prev` vector alongside the current one. Those
two vectors are aligned **by position**, and position is not stable across a
membership change: after an insert at the front, or a reorder, current slot *i*
and previous slot *i* are different elements.

So the natural optimisation — "reuse the previous obb for any element whose hint
did not change" — compares element X's *current* hint against element Y's
*previous* one. The sizes match. The assert passes. The layout is quietly wrong
for a frame, and then usually right again, which is the worst possible failure
signature.

**Frame this as a correspondence bug, not a timing one.** Nothing arrived late;
every value in both vectors is current-frame. What is wrong is *which element
each slot refers to*. Waiting a frame, adding a `check()`, or forcing an extra
update pass fixes nothing.

The two correct forms:

- **Fold per element, after `scatter`** — each element's fold sees only its own
  history, so there is no correspondence to get wrong.
- **Use `mapKeyed`** — the key travels with the value, so the fold can match on
  it explicitly.

### There is no frame-lag hazard

Worth stating positively, because it is the first objection anyone raises about
splitting a layout into fan-in, compute and fan-out nodes.

`extract → mapValues → scatter` is **acyclic**. Every node in it settles within
one update pass, so the arity the `scatter` sees is always the arity the
`extract` produced. There is no frame in which the aggregate is stale with
respect to membership.

Two clarifications that prevent this from being over-claimed:

- **Cycles do cause cross-frame variation, and they are pre-existing and
  orthogonal.** `scrollView` seeds `viewSize` as an input with a placeholder
  (`scrollview.cpp:33`) and feeds the measured size back through
  `modifier::trackSize(viewSize.handle)` (`scrollview.cpp:83`). That loop settles
  over frames today and will continue to. It is not introduced by this design and
  is not fixed by it.
- **A stale ambient input yields wrong *values*, never wrong *arity*.** Arity
  comes from membership and propagates synchronously through the whole chain; the
  `extras...` of `mapValues` only feed the maths. So the failure mode of a cycle
  is a frame of wrong sizes, not a mismatched fan-out.

**Do not claim animation causes lag.** `avg::Animated<T>` is an ordinary value as
far as the signal system is concerned — it propagates like any other value, and
the interpolation happens in the render tree at draw time. It introduces no
signal-graph delay.

### Once-per-identity initialisation is load-bearing

`extract`'s "`f` is applied once per identity" is not an optimisation. It is what
stops a sibling insert from resetting surviving children.

The mechanism to be careful of is `bq::signal::Join`. Its `update` re-evaluates
the outer signal and, **when the outer changed, discards and re-initialises the
inner signal and its data**:

```cpp
// src/bq/include/bq/signal/join.h:85-97
if (r.didChange)
{
    data.innerSignal = makeInnerSignal(sig_.evaluate(context, data.outerData));
    data.innerData = data.innerSignal.initialize(context, frame);
}
```

A naive fan-in — `builders.map(...combine...).join()`, which is what `dynamicBox`
writes at `dynamicbox.h:42-53` — puts membership in the outer signal. Any change
to membership therefore re-initialises *every* child's inner signal, resetting
whatever `DataType` state hangs off it. `extract` must not be built that way: the
per-identity signal has to be created once, when the identity appears, and
survive membership changes to its siblings.

### First value wins, and it cannot be asserted

With `ArraySignal<Widget>`, the delegate has already run by the time a second
`Widget` value arrives at a fixed key. There is nothing to do with it: applying
it means rebuilding, and rebuilding loses the state the whole design exists to
protect. **The rule is that the first value wins; the key is the unit of
change.**

Honesty about enforcement: **this cannot be asserted.**

- `Widget` has no `operator==`, so "did the value actually change?" is not a
  question that can be asked.
- Asserting merely on "the signal emitted again" would false-positive on any
  upstream that emits without a `check()` — which is most of them, since
  `tryCheck()` is a silent no-op for non-comparable types.

This is not new behaviour. It is today's silent `continue` in `dynamicBox`'s
element cache (`dynamicbox.h:105-106`) promoted from an undocumented accident to
a **stated contract**. The improvement is that a caller who needs a widget to
change now has a correct expression of it — change the key — instead of a
behaviour that appears to work and does not.

### `tryCheck()` where it helps, silently nothing where it does not

`Check` suppresses the *change notification*, not the value: its `update()`
downgrades `didChange` to `false` when the newly evaluated value compares equal
to the cached one (`src/bq/include/bq/signal/check.h:44-59`). Use `.tryCheck()`
freely on per-element signals — it takes advantage of `operator==` where the type
has one and passes silently through where it does not. See *Skipping repeats* above
for the details and for the usability trap in that silence.

## One layout engine

This is the headline finding, and it was not the goal when the design started.
`bqui` has **two** layout engines: the static one (`layout()`, used by `box`,
`stack` and `uniformGrid`) and the dynamic one (`dynamicBox`, used by `hbox` and
`vbox` when the child list is a signal). The operator set above does not add a
third. It **collapses the two into one**.

### `layout()`'s own type aliases already are this API

`src/bqui/include/bqui/widget/layout.h` declares exactly two things a layout must
supply:

```cpp
// layout.h:11-13
using ObbMap = btl::Function<
    std::vector<avg::Obb>(ase::Vector2f size,
            std::vector<SizeHint> const&)>;

// layout.h:22-24
using SizeHintMap = btl::Function<
    SizeHint(std::vector<SizeHint> const&)
    >;
```

Read those against the operator set:

| `layout()` alias | Operator |
| --- | --- |
| `ObbMap` — `(Vector2f, vector<SizeHint>) -> vector<Obb>` | `mapValues(g, size)` — `g` is `vector<A> -> vector<B>`, `size` is the ambient extra |
| `SizeHintMap` — `vector<SizeHint> -> SizeHint` | `values().map(...)` |

`ObbMap` is `mapValues` with the container size as the injected extra, down to
the argument order. `SizeHintMap` is `values()` followed by an ordinary
`Signal::map`. The abstraction was not invented for this design; it was already
sitting in the static engine's signature, un-named and un-reused.

`layout()`'s implementation completes the picture: it fans in with
`bq::signal::combine` over the children's hints (`layout.cpp:21-26`), computes
once (`layout.cpp:31`), and fans out per child (`layout.cpp:34-55`). That is
`extract`, `mapValues`, `scatter`, hand-written for the static case.

### The static engine expresses nothing the dynamic one cannot — the tuple path is dead

There is one thing the static engine could in principle do that a homogeneous
`ArraySignal<T>` cannot: hold children of **heterogeneous types** in a
`std::tuple`, keeping each child's concrete type through the layout. `box.h`
contains the machinery for it:

- `accumulateSizeHintsTuple` (`box.h:116-121`), returning
  `AccumulateSizeHint<dir, std::tuple<Ts...>>`.
- `MapObbs<dir>` (`box.h:163-202`), a struct whose `operator()` is templated on
  the hint container and flattens it with `btl::forEach`.

**Both are dead code.** A repo-wide search for either name over all `.h`, `.hpp`
and `.cpp` files returns only their own definitions — `box.h:117` and `box.h:164`
— and nothing else in the tree. `box()` uses the vector forms
(`accumulateSizeHints<dir>` and `&mapObbs<dir>`, `box.h:238`); so do `stack`
(`stack.cpp:25`) and `uniformGrid` (`uniformgrid.cpp:82-86`).

So the only capability the static path has over a unified engine is one **no
caller uses**. That removes the last reason to keep two implementations.

### Cost: the static path is lighter at construction, not at steady state

The fair objection is that a static list should not pay for identity it does not
need. The honest accounting:

- **Construction.** The unified engine costs one extra node in the graph and N
  key allocations. Once.
- **Steady state.** For a never-changing membership, `extract`'s `Join`
  initialises its inner signal once and thereafter behaves as a plain `combine` —
  the re-initialisation branch at `join.h:88-92` is only taken when the *outer*
  signal changes, and for fixed membership it never does. `scatter` matches
  identity once at construction, not per emission. **Per frame, both paths do the
  same work.**

A per-frame cost would be a real argument against unifying. A one-off
construction cost is not.

### Safety: keyed fan-out removes two unchecked arity contracts

The static path fans out with `obbs.at(index)` on a captured index
(`layout.cpp:37-47`). `at()` throws rather than corrupting, but the contract —
"the obb vector has one entry per builder, in builder order" — is unchecked at
the type level and is asserted nowhere. It is the same unchecked contract as
`dynamicBox`'s `assert(obbs.size() == builders.size())` (`dynamicbox.h:70`),
written a different way.

Keyed fan-out removes both: `scatter` cannot be handed an aggregate that did not
come from the matching `extract`.

### The maintenance argument is the strongest one

`dynamicBox` is missing `handleGravity()`. `layout()` applies it to every child
(`layout.cpp:84-87`); `dynamicBox` does not apply it anywhere. `dynamicBox` also
has the element-cache correctness bug described under *Rationale*.

Neither is a hard problem. Both exist **because `dynamicBox` is a second
implementation of `layout()` that drifted** — every fix to one has to be
remembered for the other, and it was not. One engine makes drift structurally
impossible, which is worth more than either individual fix.

## Worked example: `dynamicBox`

`dynamicBox` is ~150 lines of `dynamicbox.h`. Under the operator set it is
roughly 15. The mapping, stage by stage:

| Current implementation | Becomes |
| --- | --- |
| Apply `BuildParams` to each child widget (`dynamicbox.h:28-40`) | `map` |
| Fan in each builder's size hint (`dynamicbox.h:42-53`) | `extract(&Builder::getSizeHint)` |
| Container hint from children's hints (`dynamicbox.h:55-59`) | `values().map(accumulateSizeHints<dir>)` |
| Obb computation (`dynamicbox.h:63-65`) | `mapValues(&mapObbs<dir>, size)` |
| Re-key obbs positionally back onto builder ids (`dynamicbox.h:66-82`) | **deleted** — identity is carried by `KeyedArraySignal` |
| `withPrevious` element cache and per-child id lookup (`dynamicbox.h:85-141`) | `scatter` |
| Fan in each element's instance (`dynamicbox.h:143-156`) | `extract(&Element::getInstance).values()` |

Two of those rows are the interesting ones.

**The re-keying block disappears entirely.** `dynamicbox.h:66-82` exists only to
re-attach ids that were dropped when the hints were fanned in: it asserts the two
vectors are the same length and zips them by position. That is the alignment
invariant being re-established by hand after having been thrown away. `extract`
never throws it away, so there is nothing to re-establish.

**The element cache becomes an operator.** `dynamicbox.h:85-141` is a
`withPrevious` fold that, for each incoming builder id, linearly scans the
previous result for a match, reuses it if found, and otherwise builds a new
element whose obb comes from an O(n) search of the shared obb list
(`dynamicbox.h:108-122`). That is apply-once-per-identity plus identity-matched
fan-out, hand-rolled — `scatter`.

### The missing `handleGravity()` is fixed structurally

`layout()` pipes every child through `modifier::handleGravity()` before building
it (`layout.cpp:84-87`). `dynamicBox` does not, so a gravity set on a child of an
`hbox`/`vbox` with a dynamic child list is silently ignored.

Unifying fixes this by construction rather than by remembering to add a line.
`handleGravity` is a three-pass negotiation — it asks the child for
`getWidth()`, clamps against the outer width, asks `getHeightForWidth()` with the
result, clamps again, then asks `getWidthForHeight()` and clamps a third time
(`handlegravity.h:16-34`) — and then offsets by the child's gravity
(`handlegravity.h:37-46`). Its inputs are the child's own size hint, the outer
size assigned to it, and the child's gravity.

The `scatter` delegate has all three: the child's value (hence its builder, hence
`getSizeHint()` and `getGravity()`), and the identity-matched obb, which *is* the
assigned outer size. So `handleGravity` composes into the delegate with nothing
threaded around.

## Layout audit

Every collection layout in `bqui` was checked against the operator set. All of
them are expressible. Recording the audit is worth more than recording the
conclusion, because several of them *look* like counterexamples.

The reason none of them is: the apparent difficulties all live **inside the
`vector<A> -> vector<B>` function body**, and `mapValues` takes an arbitrary
function. No primitive beyond it is needed.

| Apparent difficulty | Where it actually lives |
| --- | --- |
| The prefix scan with a running accumulator in `mapObbs`, with the y-axis running backwards (`box.h:213-230`) | A local `float acc` in a loop. No `scan` primitive needed. |
| `getSizes` computes a container-level aggregate — the three-element `multiplier` — and then redistributes it per child (`box.h:24-52`) | One function that already receives every child's hint at once. That is exactly the `mapValues` shape. |
| A second fan-in round *inside* the hint computation: `AccumulateSizeHint::getHeightForWidth` re-queries every child at its resolved width, zipping via a mutable counter (`box.h:70-90`) | Inside the `SizeHint` value, not in the signal graph. |
| The lazy, re-entrant `SizeHint` (`AccumulateSizeHint`, `box.h:54-107`; `StackSizeHint`, `stacksizehint.h:8-15`) that the parent may re-query at arbitrary widths | The laziness is **inside the value**. The child hints are captured synchronously when the aggregate is built; re-querying never touches the signal graph. |
| `stack` needs zero per-child data downward but all of it upward (`stack.cpp:12-25`) | `values()` for the upward direction; a `mapValues` that returns the same obb for every child for the downward one. |

Two layouts actively shaped the design:

- **`uniformGrid` carries per-child non-hint metadata.** Each child has a grid
  `Cell` (`uniformgrid.cpp:21-22`), and the cells live in a **parallel vector**
  held in correspondence with the widgets by an invariant that only
  `UniformGrid::cell()` maintains. The obb map then captures `cells` and iterates
  it positionally (`uniformgrid.cpp:55-80`) — a second unchecked alignment
  contract, distinct from the hint one. An `ArraySignal` element that carries its
  own metadata dissolves both the parallel vector and the invariant.
- **`scrollView` composes children with synthetic extras.** Its final layout is
  `vbox({ hbox({view, vScrollBar}), hbox({hScrollBar, hfiller | setSizeHint}) })`
  (`scrollview.cpp:136-141`) — the filler has no corresponding input child at
  all. This is what makes **`concat` load-bearing rather than theoretical**: a
  container must be able to place framework-supplied children alongside the
  caller's without them sharing an identity space.

**Not collection layouts, and out of scope:** `bin`, `scrollView` itself,
`background`/`foreground` (`modifier/background.h:13-18`) and `scrollBar`. They
take a fixed number of children with fixed roles and stay exactly as they are.

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
job: `toSignal` on a `ArraySignal<AnyWidget>` produces the very signal it already
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
stable id. It is expected to be reworked onto `ArraySignal<Window>`, with `App`
owning the id allocator.

## Open questions

Points the design does not yet settle. Flagged rather than guessed.

- **The defaulted delegate's return type.** As noted above, defaulting a
  `(AnySignal<T>) -> U` delegate to identity yields `ArraySignal<AnySignal<T>>`,
  not `ArraySignal<T>`. Decide whether to unwrap it, to require the delegate, or to
  offer the default only where `ArraySignal<AnySignal<T>>` is what the consumer
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
  says only `pure` and `forEach` mint identity, but the `AnySignal<ArraySignal<T>>`
  constructor hands out **fresh ids on every swap**. Either that constructor is
  an acknowledged third minting site — in which case the table needs a row and
  the "single rule" needs rewording — or the swap should be modelled as `join`
  over a signal-of-`ArraySignal`, with the fresh ids coming from re-entering the
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

1. **`ArraySignal<T>` core** — the type, its constructors, the tree, `join`,
   `toSignal` and the scoped id allocator, and the generic apply-once-per-id
   operation. Independently useful: `App::windows` can consume it before
   `forEach` exists.
2. **`forEach`** — the keyed operator on top, plus the retirement path for
   `dataBind` / `dataSourceFromCollection` / `dynamicBox`'s caching layer.
