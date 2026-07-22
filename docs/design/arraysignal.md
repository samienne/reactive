# Design: `ArraySignal<T>`, `forEach`, and one layout engine

> **Status: revised design, implementation started.** Steps 0 to 4 of
> *Implementation order* have landed — all of `bq`, plus `App`'s window list;
> the layout rework is outstanding. This document has been amended to describe
> what was built rather than what was first proposed. The earlier
> implementation on PR #100 is
> a **superseded spike**:
> it put the per-identity table in the description rather than in the
> `SignalContext`, which is a correctness defect, not a limitation. This revision
> relocates that state and, as a consequence, removes five operators. Read the
> spike only for the two `bq` `join` fixes it found and for its test cases.
>
> This document remains the design record until the work lands, at which point
> the stable parts move to their normal homes — the model and its traps to
> `docs/conventions.md`, the settled *why* to `docs/decisions.md`, and the API
> contract to Doxygen in the new headers — and this file goes away.

*Last verified against `77e391f` (2026-07-22).*

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
- **`scatter`** and **`join`** — the fan-out / fan-in pair that lets a
  *container* compute over all of its children at once and hand each child its
  own share of the result, without ever exposing an identity.

`ArraySignal` is best understood as **its own domain alongside `Signal`**, entered
by `forEach` and left by `join`; that framing is developed below and is what
tells us which operations must exist and what each does to identity.

Two payoffs beyond the code sharing:

- It **fixes a real bug**: `dynamicBox` today supports dynamic *membership* only,
  and silently ignores a changed widget for an existing item.
- It **unifies the static and dynamic layout engines**. `layout()`'s own type
  aliases turn out to be this API already, so `hbox`, `vbox`, `stack`,
  `uniformGrid` and `dynamicBox` become one implementation rather than two that
  drift apart. That finding is the headline of this document; see *One layout
  engine*.

## What the first implementation proved — and where it was wrong

The `bq` half was built against the first draft of this document. The result is
best read as a **spike**: it established that the operator set is expressive
enough to carry every layout, and it uncovered two real `bq` bugs. It also got
the most important thing wrong, and that is why this revision exists.

### The defect that forced the revision: state outside the context

`bq`'s own rule is that a signal is a **stateless graph description**, and that
all per-signal state lives in a `SignalContext`/`DataContext` (`src/bq/AGENTS.md`).
`SharedControl` is the reference implementation of that rule: the description
carries a `btl::UniqueId`, and `initialize` looks its state up with
`context.findData<ContextDataType>(id_)`, creating it only on a miss
(`sharedcontrol.h:147-166`). State is therefore per-context, and two contexts
over the same description never see each other.

The spike's `OncePerId` does the opposite. It holds

```cpp
std::mutex mutex_;
std::map<ArrayId, AnySignal<U>> cache_;
```

in a `shared_ptr` **captured by a `map` lambda** (`arraysignal.h:185-191`). That
cache lives in the *description*. Two `SignalContext`s over the same
`ArraySignal` share one table and race for it — the mutex is there because the
design already knew the table was shared, which is the tell. Per
`docs/conventions.md` on context isolation, contexts are parallel; sharing
durable state between them is not a tidiness issue but a correctness one.

The first draft acknowledged this as a bounded limitation ("realising the same
`ArraySignal` in two `SignalContext`s whose membership diverges makes the two
evict each other's entries… churn, not corruption"). That assessment was too
generous. The entries are not merely evicted: they are *widget state* — a
delegate's built `U` — so two contexts evicting each other destroys user-visible
state in one context because of what happened in another.

Every `.share()` placement puzzle in the first draft was a symptom of the same
thing. `.share()` was being used to stop the cache from being instantiated
twice, which is precisely the job `DataContext` keying does for free.

### The framing error underneath it

The first draft opened by calling `ArraySignal<T>` "an analog of
`AnySignal<std::vector<T>>` with identity preservation". **That framing is
withdrawn.** It is the source of most of the complexity this revision removes:
it invites you to look for a vector-shaped value inside the array, and the
operator set then grows an `extract`/`mapValues`/`scatter` round trip whose only
job is to get that vector out and put it back.

`ArraySignal<U>` is not a signal of a vector. It is a **reactive collection of
stable identities, each carrying a `U` that was built once**. It collapses to
`AnySignal<std::vector<X>>` only when `U` is itself an `AnySignal<X>` — and that
is the single exit.

### What survived unaltered

These findings from the spike stand and are not re-litigated below.

- **The delegate takes the element's signal, not its value** (`AnySignal<T> → U`).
  Reaching a `T` means evaluating a signal, which an operator cannot do while
  building the graph. See *Rationale*.
- **Element equality is identity-only**, so `tryCheck()` on an element sequence
  means exactly "membership is unchanged".
- **Two pre-existing `bq` bugs in `join`**, both of which made `join()` fail on a
  type-erased signal, and both of which must be re-landed by the rewrite:
  `Signal::join()` instantiated `Join<TStorage>` where `TStorage` is `void` for
  an `AnySignal` (it must use `StorageType`); and `detail::toSignal`'s overload
  pair picked the forwarding `toSignal(T&&)` for an `AnySignal`, wrapping it in a
  `constant` instead of joining (it must be one function with an `if constexpr`).
  These are independent of this design and are worth landing on their own.
- **The layout audit** below, in full. Nothing in it depended on the operator
  names that changed.

## Naming

The type was called `DynArray<T>` in earlier drafts. It is now
**`ArraySignal<T>`**.

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

## The framing: two kinds of dynamism

This split is the load-bearing idea; everything else follows from it.

**Value dynamism** — the structure is fixed and the contents change. Three items,
in the same order forever, each one's value varying over time. Identity is
**stable by construction** — item *k* is item *k* forever.

**Structural dynamism** — membership itself changes: the list grows, shrinks, or
reorders. This is the *only* place identity is needed, because "which item is
this?" no longer has a positional answer. `forEach` is what supplies it.

Both enter an `ArraySignal` through **`forEach`**, and value dynamism lives
*inside* an element rather than at the element: the delegate is handed a signal,
and the element's value — what the delegate built from that signal — never
changes afterwards. `ArraySignal`'s own constructors are all constant. The
alternative, a constructor taking a bare `AnySignal<T>`, is rejected under
*Construction*: an element with no delegate has nowhere to put the variation but
the element itself.

The reactive-subtree constructor is structural dynamism at the coarsest possible
granularity (the whole subtree is replaced), and is still unbuilt.

## `ArraySignal<T>`

`ArraySignal<T>` lives in `bq`. It has no UI dependency; `bqui` merely consumes it.

### It is a description, not a container

A `ArraySignal<T>` is an **immutable value**. It has no `push_back`, no `erase`, no
mutation of any kind. Dynamism flows *in* from upstream: a `Collection<T>`
converted into an `ArraySignal<T>` stays live because the conversion produced a
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
| `std::initializer_list<ArraySignal<T>>` | a fixed list of children |
| `std::vector<ArraySignal<T>>` | ditto, built at runtime |
**Every one of them is constant**, and that is the rule: anything that varies
enters through `forEach`, which asks for the key that says which item is which.
The one candidate that would break the rule, a reactive subtree taking
`AnySignal<ArraySignal<T>>`, is unbuilt and unsettled — see *The reactive
subtree* and *Open questions*.

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
`ArraySignal<T>`, the type is a tree: a **leaf** is a single constant
item, and a **branch** is a list of children. This syntax must work:

```cpp
ArraySignal<AnyWidget> content = {
    someWidget,
    getArraySignalOfWidgets(),
    { otherWidget, getMoreWidgets() }
};
```

Nesting is not a special case in the consumer: a consumer operates through
`forEach`, `map`, `scatter` and `join`, all of which see a flat sequence of
identified elements, and never walks the tree.

Note that the *stated public requirement* is only that `{a, b, c}` compiles for
values of `T`. The `initializer_list` element type being `ArraySignal<T>` rather
than `T` is how that requirement is met **and** how nesting falls out for free,
since every accepted form converts to `ArraySignal<T>`.

### Identity is internal, and an id is minted where its element is

**Settled:** an id is allocated at the point the element it names comes into
existence. For a `forEach` element that is inside the node, in its per-context
table, when the key first appears — it can be nowhere else, since the key is not
known until then. For a constant item it is when the `ArraySignal` is
constructed, because the element exists from that moment and never changes.

So the description does carry the constant items' ids. That is harmless, and the
reason is the same one that makes the whole scheme safe: **an id never reaches
user code**. No operator hands one out, nothing outside can construct or compare
one, and an id names nothing but an entry in the per-context tables that match
it. Two contexts over one constant element look their own values up under the
same id and never meet.

An earlier draft required *every* id to be minted per context. That bought
nothing for a constant — it would mean a node whose only job is to allocate an
id — and the rule it was protecting is about where the *tables* live, not the
ids.

Two consequences worth stating, because both simplify the design relative to the
first draft:

- **`concat` needs no re-namespacing.** Every node that builds an array mints
  its own ids, so ids from different sources are distinct by construction. The
  first draft's id-space-plus-index scheme, and the pointer-derived ids that
  made ids irreproducible between runs, both go away.
- **The user-facing key contract weakens, which is a gain.** `TKey` no longer has
  to be unique across the whole array — only **within a single `forEach`**. That
  is a far easier promise for a caller to keep, and it makes duplicate detection
  a local check inside one node rather than a whole-array invariant. The spike's
  occurrence-index disambiguation (`(key, n)` entries) existed only to satisfy
  global uniqueness and is deleted.

### No identity surfaces at the exit

**Settled by PR #99's rework, and it is the answer *Identity is internal*
predicted.** Nothing outside an array needs an id, so there is **no id-pair
exit**: a consumer of a changing list takes an `ArraySignal<T>` and builds what
it needs per identity with `map`, rather than being handed
`AnySignal<vector<pair<size_t, T>>>` and reconciling by hand.

`App` is the worked case. It now takes an `ArraySignal<Window>` and builds one
`WindowGlue` per identity; the by-id reconcile loop, the producer-assigned
`size_t`, and the `WindowList` alias are all deleted. What replaced them is not
a translation of the loop — the loop *is* `ArrayOnce`, and `App` no longer
contains one. The other consumer the open question named, `dataBind`, is
retired wholesale at step 6 rather than re-typed, so nothing there reopens it.

Four findings from doing it, because each one generalises past `App`:

- **The consumer's `map` is the reconcile.** `map` runs once per identity and
  its result is destroyed when the identity leaves, which is exactly
  create-on-arrive / destroy-on-depart. A consumer whose per-item object is
  expensive or owns an OS resource needs no more than that.
- **A non-signal value leaves as a constant.** `App`'s per-item value is a glue,
  not a signal, and `join` is the only exit. `map`ping it to
  `constant(glue)` and joining reads oddly the first time and is what the exit
  is for: *One layout engine* fans in `AnySignal<SizeHint>`s that were already
  signals and so never met this, which is why `join`'s Doxygen now says so.
- **The exit gives the consumer a plain vector, and that is enough.** Everything
  `App` did with ids — iterating live glues, tearing down departed ones — is a
  fold over that vector. The id was never carrying information the consumer
  could not read off the list.
- **The consumer owes no change notification either.** The first attempt kept
  the window-list observer PR #99 had and only re-typed what it reported. That
  was backwards: the list is the source of truth, so what an item offers is
  wired *back into what the list is built from* — a window's close callback
  removes its own key — and there is nothing left for a notification to be
  for. A consumer that thinks it needs one is usually holding a second copy of
  the membership. Nothing of the kind shipped.

### An element's signal may not be read on a clock of its own

**A constraint this design did not state, and the first `bqui` consumer broke
it.** *The pick construction* requires that a pick be dropped "no later than the
update that observes the key leaving". A consumer that reads the element's
signal only inside the array's own update pass gets that for free: the source
changes, the node retires the key, and nothing evaluates the departed pick
again. The unified layout engine will be such a consumer, which is why the
obligation looked like a formality.

`App` was not. Each `WindowGlue` holds the window's title and widget signals in
its **own** `SignalContext`, updated from OS input handlers. An `Input` is
global to every context, so a handler that removed a window's key — a window
with its own close button — made that window's next update read a pick whose key
had just left, and `requirePresent` threw out of the event loop and took the
process down. Reproduced in `testapp1`, whose second window is now exactly that
button.

The fix is not to soften `requirePresent`; a latch here would hand back a stale
value, which *The pick construction* already rejects. It is to give the element's
signals one clock: `WindowGlue`'s input handlers now request a frame instead of
transacting inline, so every per-window update happens after the array has been
reconciled and a departed window's glue is gone before anything can update it.

State it as a rule for the next consumer:

> **Read an element's signal only where the array is updated.** A consumer that
> drives per-element signals on a schedule of its own must reconcile the array
> first, and must destroy what it built for a departed identity before that
> schedule comes round again.

Note what the fix does *not* rest on. The close path — the idiom for removing a
window — never transacts at all: the glue's close callback only invokes the
window's own callbacks and returns, so removal from `onClose` is safe by
construction. The deferral above is what makes removal safe from a *widget* in
the window, which is the other half of the idiom.

`App` still does not fully satisfy the rule for a caller-supplied array, and the
remainder is recorded in `src/bqui/AGENTS.md` rather than hidden:
`AnimationGuard` brackets a `withAnimation` scope with two synchronous
transactions of every window, and a list changed from inside the frame phase is
reconciled only on the next pass. Both need the app loop's update model changed
rather than a local guard, which is why they are not in this change. The lesson
for the design is the rule above: a consumer with its own clock is the hard
case, and the layout engine's being free of it is a property of the layout
engine, not of the array.

The one thing the caller loses is the ability to have a single `forEach` build
*differently shaped* items, since the delegate is handed the item's value and
not its key. That is not a gap: the caller writes one array per shape and
concatenates them, which is what the braced-list constructor is for.

### Reading an element's signal once, versus driving it on a clock

**Settled when `App` grew its own window collection.** The rule above forbids
*driving* an element's signal on a schedule of the consumer's own. It does not
forbid **reading it once**, when the identity appears, which is what a consumer
that builds something per element wants anyway.

`App` is the worked case in both directions. `App::run` maps
`ArraySignal<AnySignal<Window>>` to one `WindowGlue` per identity, and the glue
holds the element signal in a `SignalContext` that it evaluates in its
constructor and **never updates**. There is nothing later to read — an element
cannot vary at a fixed identity — so the read that would encounter a departed
key never happens. Everything the glue drives per frame is the *window's own*
signals, its title and its widgets, which came out of that one read and are not
the array's.

That is what makes self-removal safe on this path, and it is worth being precise
about why, because the tempting wrong answer is to freeze the value into the
array. An operator that handed back the item's value rather than its signal
would discard later values under the same key, silently, for every caller — a
different operator wearing the identity delegate's name. The freeze belongs to
the consumer that knows its elements do not vary, not to the vocabulary.

So the rule stands unchanged, with its scope made explicit:

> An element's signal may be **read once**, when its identity appears. What it
> may not be is *driven* — updated on a clock the array does not control.

### The reactive subtree: `AnySignal<ArraySignal<T>>`

**Unbuilt.** It is the one proposed constructor that is not constant, and the
identity algebra does not yet account for it — see *Open questions*. Recorded
here because the shape of the answer is settled even though the constructor is
not.

It would make a whole subtree swappable — conditional branches, a
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
  copy and move constructors for a non-const lvalue `ArraySignal`, and it would
  swallow braced lists meant for the dedicated constructors. Constrain it on
  `std::is_convertible_v<U, T>` *and* exclude `ArraySignal` itself. This is the
  single most likely way to get a mysterious compile error or a silently wrong
  overload.

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

## Where state lives

This is the rule the whole design turns on, and the one the first implementation
broke.

> An `ArraySignal` is a **description that builds a signal**. It never
> participates in a `SignalContext` itself. Every piece of durable state the
> built signal needs — the key→id map, the per-key `U` table, the membership
> order, each element's instantiated data — lives in that signal's per-context
> data.

`SharedControl` is what supplies that, and the array uses it rather than
reimplementing it: the nodes hold their tables in an ordinary `DataType`, and the
array holds those nodes behind a `.share()`. `SharedControl` carries the
`UniqueId`, does the `findData` on initialise and creates on miss, so the tables
are per-`DataContext` without any node naming an id of its own. See *Sharing is
intrinsic*.

Four consequences, all of which the rest of this document depends on:

- **Two `SignalContext`s over one description yield two independent tables.**
  Copies of the description do *not*: an array holds its elements behind a
  `.share()`, so every copy carries the same `UniqueId` and every consumer within
  one context finds the one table. See *Sharing is intrinsic*.
- **The `U`s are unobservable from outside the graph.** They are built inside the
  node, in context state, and no operator hands one out. Anything you want to do
  with a `U` must therefore be expressed as an operator that runs *inside*.
- **That dissolves the `getSizeHint` obstacle.** The blocker for the `bqui` work
  was that fanning in size hints means reaching into each `Builder` for its
  `AnySignal<SizeHint>`, which you cannot do if the `Builder`s live in node
  state. But `map` runs once per identity *inside the node*, so
  `array.map([](Builder const& b){ return b.getSizeHint(); })` is legal and
  yields the joinable form. The obstacle was an artefact of the old shape.
- **Nothing needs a bespoke signal type.** See *The pick construction*.

### The API surface

Three groups, by who writes the call:

- **Callers** write constructors and braced lists. Nothing else.
- **Producers of dynamic lists** write `forEach`.
- **Container authors** write `map`, `scatter` and `join`.

The first draft split these across two headers and a nested namespace to keep
`scatter` out of casual reach. That is retained in spirit but is no longer
load-bearing: with the round trip gone there is no operator whose misuse
corrupts identity, so the split is documentation, not a safety barrier. Put the
container-author operators in a separate header if it reads better; do not build
a namespace fence around them.

## The operator set

Six things. Every one has an existing `bq` pattern to copy.

### Construction

The accepted forms are listed under *A constructor per supported type*, and they
are all constant. `{a, b, c}` compiles, nesting falls out for free, and the type
is a uniform recursive tree as described above.

**`AnySignal<std::vector<T>>` is deliberately not a constructor.** In the spike
it was, implemented as `source.map(idx).share()` (`arraysignal.h:450-457`),
keying elements **by position**. That is the one construction whose outer signal
fires on every emission, and it is what made a single sibling insert take the
build count from 1 to 4 and reset the survivor's fold. Positional keying is not
identity preservation; it is the bug this design exists to remove. To turn a
varying list into an `ArraySignal` you must go through `forEach` and say what the
key is.

**The single-item `AnySignal<T>` constructor is gone too**, and for the same
reason one step down. Earlier drafts kept it and accepted that a changed value
rebuilds that element. But an element built from a bare signal has nowhere for
the variation to live *except* the element: there is no delegate, so nothing was
handed a signal to keep. Every other entry to the array puts the variation inside
what the delegate built, which is why the delegate and every `map` run exactly
once and stay run.

Keeping it would have cost the whole identity algebra. A value change at fixed
identity is silently ignored by everything downstream — `dynamicBox`'s bug — so
the constructor would have had to mint a *new* identity per value change, adding
a third minting site and making "`map` runs once per identity" mean two different
things depending on which constructor an element came from. A single varying item
goes through `forEach` with a key, like every other varying thing.

So: **only construction of a constant item and `forEach` mint identity**, and an
element's value never changes once built.

### `forEach` — the entry

```
forEach(AnySignal<std::vector<T>> source,
        keyFn   : T const& -> TKey,
        delegate: AnySignal<T> -> U)          -> ArraySignal<U>
```

The delegate runs **once per identity, per context**. `U` need not be a signal;
it is typically something that *wraps* signals — a `Widget`, a `Builder` — which
is the whole point.

### `map` — value-level, but still once per identity

```
map(f : U const& -> V)                        -> ArraySignal<V>
```

`f` runs once per identity, not once per value change, because `U` does not
change: the delegate produced it once and it wraps the signals that vary. This is
the operator that lets a container reach *inside* the node.

### `concat`

Joins arrays without touching ids. **Every node that builds an array mints its
own**, `map` included, so ids from two arrays are distinct however the two are
related: `concat(a, a.map(f))` — the shape of the branch a container makes and
rejoins — is well-formed. Concatenating an array with *itself* is not, and the
exit says so rather than giving one identity two places in the list.

### `scatter` — the fan-out

```
scatter(ArraySignal<U> arr,
        AnySignal<std::vector<W>> aggregate,
        f : (U const&, AnySignal<W>) -> V)    -> ArraySignal<V>
```

Each element is handed **its own slice** of an aggregate computed over all
elements. This is the only genuinely new primitive, and it is what makes a
container expressible: fan the children's hints in, compute over all of them, and
give each child its share back.

The aggregate is positional and must be aligned with the current membership; the
node checks its size on every update in which a slice is read. That is the single
remaining runtime contract, and it lives in one place instead of being re-asserted
at each call site. What that check does and does not reach is *Alignment is a
promise, and only its arity is checked* below.

**`scatter` is `forEach`'s node with a different source**, as this document
predicted, and it needed no new state of its own: the built array is `ArrayOnce`
keyed by the element's identity, exactly as `map` is, and the slice signal is the
`pick` construction over a `merge` of the array's own elements with the
aggregate, keyed by identity and shared. The size check is the map that builds
that keyed source.

### `join` — the only exit

```
join(ArraySignal<AnySignal<X>>)               -> AnySignal<std::vector<X>>
```

**The element type must be a signal to leave the domain.** There is no general
`ArraySignal<U> -> AnySignal<std::vector<U>>`, and adding one would reintroduce
exactly the hazard this design removes. `ArraySignal<AnyWidget>` is well-formed
and simply cannot be exited; you `map` it to something joinable first.

This restriction was checked against the real work, not assumed — see *One layout
engine*, where both fan-ins land in this form and no third shape is needed.

#### `join` is a node, not a `Join`

**Settled, and the one place where reusing an existing operator is wrong.** The
obvious implementation is to `map` the elements into a `combine` and let
`Signal::join` follow it. It does produce the right values, and it is a trap:
`Join::update` rebuilds and **re-initialises its whole inner signal whenever the
outer one changes** (the `didChange` branch of `Join::update`), and here the
outer signal changes on every
membership change. Every surviving element would be re-initialised — every `map`
cache, every `withPrevious` fold, every `iterate` inside a survivor's `U` back to
its initial value — because one neighbour arrived.

That failure is invisible in the values; it shows only in accumulated state,
which is exactly the state this whole design exists to protect. It is also the
spike's original failure mode wearing a different hat.

So the exit is its own node, holding **one inner data per identity** in its
per-context `DataType`, keyed by the element's id. An update carries a survivor's
data across untouched, initialises only what arrived, and releases only what
left. A survivor is never re-initialised, so nothing downstream has to defend
against it.

The tempting patch — leave the generic `Join` in place and `.share()` each
element's signal once per identity so re-initialisation finds its state again —
works, and should be recognised as the wrong fix. It makes correctness rest on
every element being shared, and it mints a `UniqueId` per identity at update
time, which grows `DataContext` at membership-churn rate. The node has neither
property: it adds no `DataContext` entry at all.

### What is gone

`KeyedArraySignal`, `extract`, `mapValues`, `mapKeyed` and `values` are all
deleted. `mapValues` was doing value-level work on the flattened vector; under
this model that is an **ordinary `bq::map`** on `AnySignal<std::vector<A>>`, with
no `ArraySignal` involvement at all. The `extract → mapValues → scatter` round
trip existed only because the first draft had no node that could hold state, and
so had to leave the domain and come back to do anything collective.

## The pick construction

`forEach` must hand its delegate an `AnySignal<T>` carrying *this element's*
value. That signal has to read the node's per-context state, and a description
cannot carry a per-context pointer — which looked, in an earlier revision of this
document, like the one piece with no precedent in `bq`.

It has one. The node builds

```
source.map(toKeyed).share()      // AnySignal<std::map<TKey, T>>
```

once, and then, per identity as each key first appears,

```
shared.map(pick(key))            // AnySignal<T>
```

`pick(key)` is an ordinary recipe carrying only a key by value, and both steps
are plain `map`s. `.share()` already supplies per-context state keyed by a
`UniqueId` through `SharedControl`, and its frame check (`sharedcontrol.h:187`)
means the source is updated once per pass no matter how many pick-signals pull on
it. **No custom signal node, and no per-context state of its own** — that is the
whole point of the construction, and anything that reintroduces either should be
treated as a sign the construction has been misread.

**Key it, do not scan it.** Sharing a `vector<pair<TKey, T>>` forces each pick to
search — O(n) per element per update, O(n²) per frame across the array. Sharing a
`std::map` (or a sorted vector) makes it O(n log n) total. Order is not lost by
this: the node needs the membership order in its own state regardless, and that
is where it belongs.

**Share the map behind a pointer, not by value.** Keying removes the scan but not
the copy: `SharedControl::DataType` holds the shared node's value **by value**
(`sharedcontrol.h:103`) and re-copies it into every consumer on every changed
frame (`sharedcontrol.h:199`). Sharing the container itself therefore costs O(n)
per consumer per update — O(n²) again for an array where every element is picked,
and n resident copies besides. What the shared signal carries must be a
`shared_ptr` to an immutable map, so the per-consumer copy is a refcount bump.
This is a property of `Shared`, not of this design; a by-reference evaluate path
for shared nodes would remove the constraint, and is worth having independently.
A second `Shared` note while it is in view: `SharedControl::update` commits
`lastFrame` and `updateResult` *before* re-evaluating (`sharedcontrol.h:187-196`),
so an inner evaluation that throws leaves the node reporting a change with its
previous value for the rest of the frame. Unreachable through `SignalContext`,
which never catches, but it is the reason a throw is a discard-the-context error
rather than a recoverable one.

**An absent key yields no value, and asking for it anyway is a hard error.**
`pick(key)` produces an `AnySignal<std::optional<T>>`, empty for any update in
which the key is not in the source. A consumer that needs the element — the
`forEach` delegate does, since it is handed an `AnySignal<T>` — unwraps with a
second plain `map` that throws when the value is missing.

An earlier revision of this document specified a **latch** here instead: the pick
would hold the last value it saw, so that a departed key produced a stale value
rather than nothing. That is withdrawn. The key's presence cannot be encoded in
the type system and cannot be guaranteed by construction, so its absence has to
surface at runtime; a latch converts that into a **silently stale value**, which
is a worse failure than a loud one and hides the bug that caused it. The latch
also did not buy what it promised — it removed the ordering argument from
`evaluate` only by moving it to `initialize`, where a context holding no live
instantiation had nothing to latch and had to fail anyway.

The obligation the latch was meant to soften therefore stands, and is now
enforced rather than papered over: **build a pick when its key appears, and drop
it no later than the update that observes the key leaving.** `forEach` owes that.
A subtree swapped out and back by `join` or `conditional` breaks it, which is
consistent with the reactive subtree minting fresh identity on a swap.

**The construction is built.** `pick`, `requirePresent` and `shareKeyed` live in
`src/bq/include/bq/signal/detail/pick.h` — three function templates over
`map`/`share`, no node and no context data — with `src/bq/test/picktest.cpp`
covering the cases step 1 called for. Two results settle points this document had
inferred or stated too strongly:

- Two `SignalContext`s over one `pick` description are wholly independent, and
  one description instantiated twice into **one** context finds the shared
  source's state already there and evaluates it once per pass, exactly as
  inferred from `SharedControl`.
- A pick reports a change whenever **any** element of the source changes, not
  only when the picked element moves, because that is what the shared source
  reports and the element type carries no comparison requirement. Suppressing
  the repeats is `tryCheck()`'s job, at the point of use — the same answer this
  document already gives under *Skipping repeats is a property of the data*.

Note also that the project builds with `b_ndebug` off, so `assert` is live in
every configuration here. A debug-assert / release-throw pair does not exist for
us: the assert would always win and the throw would be unreachable. Hard errors
of this kind are therefore a throw alone — that goes for a duplicate key as much
as for a missing one.

**A throw out of the graph ends the pass, not just the node.** `requirePresent`
is the first thing in `bq` that makes evaluation throw, and where it surfaces
depends on what is downstream: a `check` or `share` above it pulls the
evaluation into `update`, so the exception comes out of
`SignalContext::update()` with only some of the context's entries advanced and
`swapFrameData()` skipped. Nothing is corrupted — the next frame recovers — but
these are not exceptions to catch and continue from; the context should be
discarded. Worth knowing before anything else in the tree learns to throw.

**`scatter` is the same node.** Its aggregate is positional rather than keyed, but
the scatter node knows the current key order, so it zips aggregate-with-keys into
an `AnySignal<std::map<ArrayId, W>>`, shares that, and hands each identity the
identical `pick`. `forEach` and `scatter` differ only in where their source comes
from.

## Sharing is intrinsic

The layout pipeline **branches**. The same `ArraySignal<Builder>` feeds the size
hint fan-in and, after the obb round trip, the element fan-in. With a
description that is instantiated independently by each consumer, those two exits
build two nodes, two key→`U` tables, and call the delegate twice — duplicating
exactly the widget state that identity preservation exists to protect.

So sharing is **structurally required**, not an optimisation — and there is no
separate `SharedArraySignal` type, because keeping the table in the node's
`DataType` behind a `.share()` *is* sharing. `SharedControl` keys that data by a
`UniqueId` carried in the description, so every consumer within one context
finds one table, and two contexts share nothing.

**Sharing within one context is a supported, existing mechanism, not something
this design has to invent.** `SharedSignal` is the reference implementation —
follow it. Sharing *between* contexts is the thing that must not happen, and the
two are easy to conflate: the first implementation's failure was that it had no
way to tell them apart, because its table was not keyed by context at all.

Be precise about what that does and does not cover. **The built values are
shared** — the delegate and every `map` run once between the two branches, which
is the whole requirement above. **The elements' instantiated state is not**: each
exit gives every element its own inner data, exactly as instantiating any signal
twice in one context does. A `withPrevious` inside a delegate's value therefore
exists once per exit unless the delegate shares it, and the per-element
evaluation is done once per exit rather than once per pass. That is `bq`'s
ordinary rule rather than an exception to it, and it is the price of the exit
being a node that owns its elements' data — see *`join` is a node, not a
`Join`*.

**Implement only the shared form.** The consuming variant would have no user on
day one, since every container branches. Its `map` therefore takes `U const&`
rather than consuming. The consuming variant can be added later if a linear
consumer ever appears; naming can be revisited then.

## `forEach` vs `map` — the central distinction

Both take a function from an element to something else. They are not
interchangeable, and confusing them is the single easiest way to write code that
looks right and silently throws away user state.

- **`forEach` is identity-level.** Its delegate is `(AnySignal<T>) -> U`, and it
  is invoked **once per key**. It is *not* re-invoked when the item's value
  changes — the new value arrives through the signal the delegate already holds.
  Nothing is constructed on a value change, so nothing is destroyed. **This is
  what preserves widget state.**
- **`map` is value-level.** Its function is `U -> V`, and it runs exactly once
  per identity, because the `U` it reads cannot change — the delegate produced it
  once and it wraps the signals that vary. What makes `map` the wrong place to
  build a widget is not that it re-runs; it is that it is handed a *value* and
  never the item's signal, so a widget built here can only be rebuilt when the
  identity is replaced, and has no way to receive a changed value otherwise.

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
per element**, and it is the one place `f` would run more than once per identity:
`f` is re-run when one of those extra signals changes, inside a node built once
when the identity appeared. That does not weaken the rule that an *element's
value* never changes — the extra signals are not the element — and it introduces
**no new footgun class**, because the dangerous case is already covered by the
rule above ("don't construct in `.map`").

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
domains is the payoff of this whole framing; an `ArraySignal::map` that takes extra
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
  is wanted. No default is offered: with a signal-taking delegate the identity
  default gives `U = AnySignal<T>`, so the defaulted call would yield
  `ArraySignal<AnySignal<T>>` rather than `ArraySignal<T>` — defensible, but not
  what a caller expects. Requiring the delegate costs a caller who only wants
  keying one identity lambda and keeps the result type obvious.
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
- **Duplicate keys throw.** Keys need only be unique **within this `forEach`**,
  so a duplicate almost always means the caller keyed on the wrong thing. The
  spike defined a release rule — disambiguate each occurrence by its index,
  giving `(key, n)` entries — which worked but existed only to satisfy a
  global-uniqueness requirement that no longer applies. It is deleted rather
  than kept, because it added a concept to the model in exchange for defining
  behaviour after a programming error.

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
  (`btl::IsEqualityComparable`, `src/btl/include/btl/typetraits.h:82-89`); calling
  it on a non-comparable type is a hard "no member named `check`" error.
- `tryCheck()` exists always. When the type is comparable it *is* `check()`;
  when it is not, it is a **silent passthrough** returning an equivalent signal
  with no `Check` node inserted — no SFINAE failure, no diagnostic.
- What `Check` suppresses is the **change notification**, not the value: its
  `update()` downgrades the inner signal's `didChange` to `false` when the newly
  evaluated value compares equal to the cached one
  (`src/bq/include/bq/signal/check.h:44-59`). `nextUpdate` passes through.
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

Five, down from the first draft's eight. Three of the originals were properties
of operators that no longer exist.

### Keys are unique within one `forEach`

Not across the array — see *Identity is internal*. A duplicate key means the
caller keyed on the wrong thing, so it **throws**, in every build. Defining a
recovery instead (the spike's occurrence-index scheme) bought nothing and cost a
concept in the public model; asserting instead would be the same failure with
less to go on, since `b_ndebug` is off here and an assert takes the process down
without a message.

### Alignment is a promise, and only its arity is checked

`scatter`'s aggregate is positional, and the node checks that it has one entry
per element. This is the same strength as the spike's
`assert(obbs.size() == builders.size())` (`dynamicbox.h:70`) and the static
engine's unchecked `obbs.at(index)` contract (`layout.cpp:37-47`), except that it
is stated once and enforced in one place. A size check cannot catch a
permutation, so it is worth being exact about what is left.

**Through the intended construction, misalignment is unreachable.** When the
aggregate is derived from the same array — `map` into `join`, then an ordinary
`map` over the fanned-in vector — both the aggregate and the sequence `scatter`
zips it against read the array's *one shared* element signal, which is evaluated
once per pass. Every step between preserves order. So within a pass the two
orders are the same order, by construction rather than by promise.

Three things break that, and none is caught:

- An aggregate derived from a **different array**, or supplied out of band. The
  count can match while the entries describe other elements.
- An **order-changing step** between the fan-in and `scatter` — a sort, or a
  filter that is padded back to length.
- An aggregate that describes an **earlier update's membership**, because it is
  an input the caller pushes separately or because it is routed through a fold.
  A membership change that keeps the count — one element swapped for another —
  then hands the arrival its predecessor's slice.

The last is the one a caller reaches by accident, and it is the same failure as
*Nothing may be position-keyed across updates*, one level up.

**A stronger cheap check was considered and rejected.** The node could require
that the aggregate reports a change in every update in which the membership
changed, which catches all three cases above. It also fires on a correct
program: an aggregate that legitimately does not vary with a same-size
membership change reports no change, and `stack` — which repeats one obb for
every child (`stack.cpp:12-25`) — is exactly that. A hard error here takes the
context down and the caller cannot suppress it, so a check that rejects correct
code is worse than the hazard it removes. The size check is therefore the
strongest check available that does not reintroduce keyed aggregates.

**Folding the fan-in into `scatter` is the structural fix, and is not worth it
yet.** A `scatter` that took a per-element contribution and an aggregate
function would join internally, so the vector the aggregate function receives
would be this array's current membership by construction — the third case above
disappears, and the second becomes local (the function may still permute the
vector it was just handed, and only its length can be checked). It does not
remove the positional contract; it moves it one call inward, into the same
`ObbMap`-shaped function that holds it today.

Against that: a container needs the joined contributions for its **own upward
size hint** as well as for the fan-out, so an internally-joining `scatter` forces
a second `join` over the same array. Each exit gives every element its own inner
data (*Sharing is intrinsic*), so that is a per-frame cost and a duplicated
per-element instantiation, not a one-off — the kind of cost *Cost: the static
path is lighter at construction, not at steady state* says is the one that
counts. It could be avoided by having the operator also return the joined vector,
at the price of a two-part return. **Recommendation: keep the positional
aggregate.** Revisit if the layout rework produces a real misalignment, at which
point the joined-vector-returning form is the shape to reach for.

### Nothing may be position-keyed across updates

Positions are the membership order and are **not stable** across membership
changes. Folding or `withPrevious`-ing over the flattened vector correlates
element *i* of one update with element *i* of the next, which after an insertion
is a different element. This is a **correspondence** bug, not a timing one, and it
is unaffected by which fold operator is used. Anything per-element that must
persist belongs inside the `U`, where identity holds it.

### The delegate and `map` run once per identity **per context**

Not once per identity globally. Two contexts realise two independent sets of
`U`s, which is correct: contexts are parallel, and widget state must not leak
between them. Do not write code that assumes a global call count.

### An element's value never changes

Not a limitation but the invariant everything else rests on: a value is built
when its identity appears and is kept until the identity leaves. `map` therefore
runs exactly once per identity, and what varies varies *inside* the value,
through the signal the delegate was handed.

An earlier draft recorded the opposite here — that a value change at fixed
identity resets that element's state, and that this was accepted. It described
the `AnySignal<T>` constructor, which no longer exists (see *Construction*). The
underlying point stands where it belongs: a `forEach` whose *key* changes retires
the old identity and builds a new one, and *Rationale* is why matching them up
instead would be wrong rather than merely hard.

### There is no frame-lag hazard

Retained from the first draft: the pipeline is acyclic and settles in one update
pass, and `avg::Animated<T>` is an ordinary value that does not animate in the
signal domain — it is interpreted by the render tree. The layout pipeline is a
diamond (membership → builders → hints → obbs → elements), not a cycle.

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

| `layout()` alias | Where it goes |
| --- | --- |
| `ObbMap` — `(Vector2f, vector<SizeHint>) -> vector<Obb>` | a plain `bq::map` over the joined hints, whose output is `scatter`'s aggregate |
| `SizeHintMap` — `vector<SizeHint> -> SizeHint` | a plain `bq::map` over the joined hints |

Both are ordinary functions over the fanned-in vector — which is what `join`
produces and what `scatter` consumes. Neither needs an `ArraySignal` operator of
its own; needing one was an artefact of the round trip this revision removed. The
abstraction was not invented for this design either: it was already sitting in
the static engine's signature, un-named and un-reused.

`layout()`'s implementation completes the picture: it fans in with
`bq::signal::combine` over the children's hints (`layout.cpp:21-26`), computes
once (`layout.cpp:31`), and fans out per child (`layout.cpp:34-55`). That is
`join`, a plain map, and `scatter` — hand-written for the static case.

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
- **Steady state.** For a never-changing membership the fan-in initialises each
  element once and thereafter does what a plain `combine` does: the reconcile
  branch is only taken when the membership changes, and for a fixed list it never
  does. `scatter` matches identity once at construction, not per emission. **Per
  frame, both paths do the same work.**

A per-frame cost would be a real argument against unifying. A one-off
construction cost is not.

### Safety: keyed fan-out removes two unchecked arity contracts

The static path fans out with `obbs.at(index)` on a captured index
(`layout.cpp:37-47`). `at()` throws rather than corrupting, but the contract —
"the obb vector has one entry per builder, in builder order" — is unchecked at
the type level and is asserted nowhere. It is the same unchecked contract as
`dynamicBox`'s `assert(obbs.size() == builders.size())` (`dynamicbox.h:70`),
written a different way.

Keyed fan-out removes both: `scatter` matches by identity, and checks the
aggregate's size against the current membership in one place.

### The maintenance argument is the strongest one

`dynamicBox` is missing `handleGravity()`. `layout()` applies it to every child
(`layout.cpp:84-87`); `dynamicBox` does not apply it anywhere. `dynamicBox` also
has the element-cache correctness bug described under *Rationale*.

Neither is a hard problem. Both exist **because `dynamicBox` is a second
implementation of `layout()` that drifted** — every fix to one has to be
remembered for the other, and it was not. One engine makes drift structurally
impossible, which is worth more than either individual fix.

### The unified engine takes `ArraySignal<AnyWidget>`

**Settled.** Not `ArraySignal<Widget>` or any concrete widget type.

- A layout's children are **differently typed by nature** — a label next to a
  button next to a nested box — so a homogeneous array of a concrete widget type
  could not hold them. Type erasure is not a compromise here; it is the only
  thing that describes the input.
- Both existing engines already take `AnyWidget` (`layout.h:33-34`,
  `dynamicbox.h:23-24`), so nothing is lost and no caller changes shape.
- `ArraySignal` is **type-erased regardless** (*There is deliberately no
  `AnyArraySignal`*), so a concrete element type would buy no devirtualisation
  even where the children happened to agree.

The consequence for the `scatter` delegate is that it receives an
`AnySignal<AnyWidget>` rather than a concrete widget type. That is enough: it
needs `getSizeHint()` and `getGravity()`, which the erased interface provides,
and those are exactly what `handleGravity` and the hint fan-in consume.

## Worked example: `dynamicBox`

`dynamicBox` is ~150 lines of `dynamicbox.h`. Under the operator set it is
roughly 15. The mapping, stage by stage:

| Current implementation | Becomes |
| --- | --- |
| Apply `BuildParams` to each child widget (`dynamicbox.h:28-40`) | `map` |
| Fan in each builder's size hint (`dynamicbox.h:42-53`) | `map(&Builder::getSizeHint)`, then `join` |
| Container hint from children's hints (`dynamicbox.h:55-59`) | `map(accumulateSizeHints<dir>)` on the joined hints |
| Obb computation (`dynamicbox.h:63-65`) | `map(&mapObbs<dir>)` on the joined hints and the size |
| Re-key obbs positionally back onto builder ids (`dynamicbox.h:66-82`) | **deleted** — `scatter` matches by identity |
| `withPrevious` element cache and per-child id lookup (`dynamicbox.h:85-141`) | `scatter` |
| Fan in each element's instance (`dynamicbox.h:143-156`) | `map(&Element::getInstance)`, then `join` |

Two of those rows are the interesting ones.

**The re-keying block disappears entirely.** `dynamicbox.h:66-82` exists only to
re-attach ids that were dropped when the hints were fanned in: it asserts the two
vectors are the same length and zips them by position. That is the alignment
invariant being re-established by hand after having been thrown away. `scatter`
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
plain function over the fanned-in vector**, and that function is arbitrary. No
primitive beyond `join` and `scatter` is needed.

| Apparent difficulty | Where it actually lives |
| --- | --- |
| The prefix scan with a running accumulator in `mapObbs`, with the y-axis running backwards (`box.h:213-230`) | A local `float acc` in a loop. No `scan` primitive needed. |
| `getSizes` computes a container-level aggregate — the three-element `multiplier` — and then redistributes it per child (`box.h:24-52`) | One function that already receives every child's hint at once. That is exactly the shape of a plain map over the joined hints. |
| A second fan-in round *inside* the hint computation: `AccumulateSizeHint::getHeightForWidth` re-queries every child at its resolved width, zipping via a mutable counter (`box.h:70-90`) | Inside the `SizeHint` value, not in the signal graph. |
| The lazy, re-entrant `SizeHint` (`AccumulateSizeHint`, `box.h:54-107`; `StackSizeHint`, `stacksizehint.h:8-15`) that the parent may re-query at arbitrary widths | The laziness is **inside the value**. The child hints are captured synchronously when the aggregate is built; re-querying never touches the signal graph. |
| `stack` needs zero per-child data downward but all of it upward (`stack.cpp:12-25`) | `join` for the upward direction; an aggregate that repeats the same obb for every child for the downward one. |

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

## The mutable source: `SharedVector<T>`

`forEach` consumes a signal; something has to drive that signal. `SharedVector<T>`
(`src/bq/include/bq/signal/sharedvector.h`) is that something: an implicitly
shared, lock-guarded `std::vector<T>` that exports its contents as an
`AnySignal<std::vector<T>>`. It is the ergonomic replacement for `Collection` as
the *state* end of a changing list, and it is additive — `Collection`,
`DataSource` and `dataBind` stay until the layout rework removes them with their
call sites.

### It lives in `bq`

`Collection` is in `bqui`, which is where the old pipeline's whole problem
starts: a list whose membership changes is not a UI concern, and putting it
above `bq` meant only widgets could have one. `SharedVector`'s consumer,
`forEach`, is in `bq`; the type itself needs nothing from `ase`, `avg` or
`bqui`; and `bq` sits below all three, so `bqui` keeps its access either way
while `bq`'s own tests and any future non-UI consumer gain it. Everything
`Collection` reached upward for is dropped rather than followed: it needed
`bqui::Connection` only for its callback registry, and `SharedVector` has no
callback registry at all.

The contract itself — scoped handles, the emission rule, the coherence
guarantee, what the locks do not do — is documented on the type. What follows is
only the reasoning behind it.

### Why the write scope, and not the mutation, is the unit of emission

Emitting per mutation is the obvious alternative and it is worse in every
direction: it makes the cheapest possible edit the most expensive one, it gives
a caller no way to express a compound change as one, and a batching mode bolted
on afterwards ends up being this rule with a worse name. Publishing at scope end
is also *exact* — it needs no dirty tracking and no comparison against the
previous contents to decide whether anything happened, which is why a scope that
mutates nothing still emits.

### It runs no *unbounded* user code under the lock, which is what `Collection` got wrong

`Collection` invokes its registered callbacks synchronously while holding a
`btl::SpinLock` (`collection.h:336-352`), so arbitrary caller code runs under a
spin lock; a callback reaching an `InputHandle::set` puts the collection's lock
above `InputControl`'s `recursive_mutex`, and anything taking those two in the
other order deadlocks.

`SharedVector` has no callbacks to invoke. Publication is
`InputHandle::set(contents)`, and that function takes `InputControl::mutex`,
assigns a value and bumps a counter. Caller code does run under the lock —
copying and destroying a `T` — but it is *bounded* code the caller did not
choose to run there and cannot use to take another lock of its own; that is a
different thing from a registered callback, and it is the difference that
matters. Replacing a callback registry with a data sink is what removes the
hazard; the write lock is not what was wrong with `Collection`.

The lock order is therefore always `SharedVector::mutex` → `InputControl::mutex`
with no reverse edge — and it is worth writing down *why*, because it is a
property of this use of an input rather than of `InputControl` in general.
`InputSignal::initialize`, `update` and `observe` do run graph code under
`InputControl::mutex`, but only when a signal has been pushed into the input
with `InputHandle::set(Signal)`. `SharedVector` keeps its handle private and
never hands it out, so that cannot happen. Exposing the handle would reopen the
edge silently.

One consequence to accept rather than fix: `InputSignal::update` copies the
whole vector under `InputControl::mutex`, so a context updating on one thread
briefly blocks a writer, which in turn blocks readers. Readers are concurrent
with each other, not with everything.

### Coherence between `read()` and the signal

Publishing **inside** the exclusive write lock is what buys the guarantee, and
it is worth being precise about why it is safe to do it there rather than after
releasing: publishing outside the lock lets two writers commit in one order and
publish in the other, which does not merely delay the signal but leaves it
holding a state older than `read()` shows, permanently. Under the lock, the two
orders are the same order.

What remains is not a gap this design introduces: a `SignalContext` picks the
new contents up at its **next update pass**, so between the write scope ending
and that pass, `read()` and the graph disagree. That is change-driven evaluation
working as designed, identical to writing to a `makeInput` from another thread,
and the only alternative would be for a mutation to drive a frame — which is a
scheduler's decision, not a container's.

The handles are neither recursive nor upgradeable: opening a second scope on a
thread that already holds one deadlocks. Documented rather than solved, because
a recursive write scope would also have to decide what publishing means for an
inner scope, and nothing needs one.

### It copies the contents once per write scope

Publication copies the whole vector. That is `O(n)` plus `n` element copies per
write scope, and it is the honest cost to accept today, for a reason that has
nothing to do with the container: `shareKeyed` rebuilds its whole keyed map on
every emission (*The pick construction*), so the pipeline is `O(n log n)` per
update regardless. An incremental container — one that published a delta instead
of a snapshot — would buy nothing without an incremental keyed source to receive
it, and would cost the property that makes this one easy to reason about: the
signal always carries a complete state that existed.

Two cheaper shapes were considered and rejected. Writing straight into
`InputControl::value` publishes with no copy at all, but reaches into another
node's representation and forces every reader onto that node's mutex, losing
concurrent reads. Publishing a `shared_ptr<vector<T> const>` with
copy-on-write moves the copy earlier and then needs a `map` to dereference it
for `forEach`, which copies again.

### No container-assigned item identity

`SharedVector` assigns no ids. `forEach` still requires a key function, and
identity comes from the item's content. A container-scoped id is identity that
cannot survive serialisation, undo, or a round trip through a server —
`Collection`'s ids are heap addresses (`collection.h:140`), which is that defect
in its purest form.

If a caller ever has genuinely keyless items — a list of duplicates the user
reorders, where "which one is this?" has no answer in the data — the answer is a
**second, item-scoped container** whose per-item handle *is* the identity, so
the identity is a thing the caller holds rather than a number the container
made up. That container is not built and nothing in the tree needs it; it is
recorded here so the next reader does not reach for ids on `SharedVector`
instead.

### What survives `DataSource`

`DataSource<T>` is an `evaluate` function returning the current
`vector<pair<size_t, T>>`, a stream of
`variant<Insert, Update, Erase, Swap, Move, Refresh>`, and a `Connection` that
keeps the adapter feeding them alive (`datasource.h:53-57`). Its job is to hand
`dataBind` **deltas carrying container-assigned ids**. Checked against what it
does rather than what it is called, `SharedVector` plus `forEach` covers all of
it but one thing:

- **The six event kinds collapse.** `Insert`, `Erase`, `Swap`, `Move` and
  `Refresh` all say the same thing — the membership or its order changed — which
  a snapshot signal says once. `Update` is a value change, which reaches the
  element through the signal `forEach` already handed its delegate.
- **The `Connection` goes away.** It exists because the adapter's callback
  registrations are owned separately from the data (`datasourcefromcollection.h`),
  and `dataBind` has to smuggle it into a lambda to keep it alive
  (`databind.h:60-62`). A `SharedVector`'s signal holds the control block
  directly, so there is nothing separate to keep alive.
- **A seam closes.** `DataSource` is a pull for the initial state plus a push for
  everything after, and the two are not synchronised: the adapter starts filling
  the event stream as soon as it is built, while `dataBind` calls `evaluate`
  later, at initialize (`databind.h:41-56`). A mutation in between is counted
  twice — once in the initial state and once as a queued event folded on top. A
  level-triggered signal has no such seam, because there is only ever one
  current value.
- **Identity without a key does not survive**, and that is the deliberate part.
  `DataSource`'s ids let `dataBind` track items whose content distinguishes
  nothing. `forEach` requires a key. This is the same gap the item-scoped
  container above would close, and the reason for leaving it open is recorded
  there.

So `DataSource` is subsumed as a protocol, and the one thing it expresses that
`forEach` cannot is the container-assigned identity we are declining to build.

**That last point is not hypothetical, and the audit found it.** `adder`
(`src/testapp1/adder.cpp:84-108`) is a `Collection<std::string>` whose delegate
captures the item's id and uses `range.findId(id)` to update and to reorder that
item — and its "U" button *rewrites the string*. Keyed on content, that item's
key would change on every edit, and `forEach` would retire the identity and
rebuild the widget, which is the state loss the whole design exists to prevent.

The migration is therefore a change of data model, not of plumbing: `adder`'s
element becomes a struct carrying an id the *application* mints, and the key
function reads that field. That is the position stated above — identity comes
from the item's content, and content includes an id the caller owns and can
serialise. It is a real cost of the decision and it lands on the one call site
that used ids for more than lookup. `databindtest.cpp:18` ignores the id
entirely and needs nothing.

## What this supersedes

The existing pipeline, and what `forEach` does instead:

**`Collection<T>`** (`src/bqui/include/bqui/collection.h:196`) is a mutable,
lock-guarded container whose writer API (`pushBack`, `update`, `erase`, `swap`,
`move`, `sort` — `collection.h:307-461`) mutates the vector under a spin lock and
synchronously invokes registered callbacks. Each item's `size_t` id is the
address of its heap-allocated value —
`reinterpret_cast<size_t>(iter_->ptr())` (`collection.h:140`) — which is
stable across reallocation but **not reproducible between runs** — so a
`Collection` source must feed its ids through as keys or be keyed on item
content. `forEach` accepts a `Collection<T>` directly, so it
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

**`dynamicBox`** (`src/bqui/include/bqui/dynamicbox.h:23`, the signal-taking
overload of `hbox`/`vbox` — `src/bqui/src/widget/hbox.cpp:18`,
`src/bqui/src/widget/vbox.cpp:20`) **goes away entirely.** Earlier drafts kept it
and merely gave it an overload; the *One layout engine* finding supersedes that.
`hbox`/`vbox` take an `ArraySignal<AnyWidget>`, and both the static and the
dynamic case run through the single unified `layout()`.

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
(`src/bq/include/bq/signal/datacontext.h:78-100`).

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
`SignalContext` (`src/bqui/src/app.cpp:81-83`, member declared at `:438-439`),
and that context lives for the entire run — it is pumped once per frame at
`app.cpp:269` and torn down only when its `WindowGlue` is destroyed
(`app.cpp:251`, `app.cpp:514`). So every list change
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
(`datacontext.h:88-94`) does not save this: it only catches collisions between
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
(`src/bqui/include/bqui/dynamicbox.h:85-141` — the `prev.first == id` hit at
`:97-102` reuses the old element and `continue`s at `:105-106`).

`forEach` formalises exactly this pattern and moves it below the UI layer, where
it works for any `U` rather than only for widgets.

### `dynamicBox` supports dynamic membership only — and `forEach` fixes it

This is a correctness finding, not just a design argument.

`dynamicBox`'s cache is keyed by id and consulted *before* anything looks at the
widget. For each entry in the incoming builder list it searches the previous
result for the same id; on a hit it reuses the old element and `continue`s
(`src/bqui/include/bqui/dynamicbox.h:94-106`). The builder that stage 1 just
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
(builder → element, `dynamicbox.h:85-141`) then caches by id, so for every
pre-existing item the builder stage 1 just produced is dropped unused at the
`continue` on `dynamicbox.h:105-106`. The cache makes `dynamicBox` *correct*,
not *cheap*: one insert costs an O(n) build-and-discard. `.share()`
(`dynamicbox.h:40`) bounds it to once per changed frame, but not per changed
item. This is the *performance* half of the same cache; the correctness half —
that the discarded builder may carry a genuinely changed widget — is in
*Rationale* above and is not merely a cleanup. Both are moot once `dynamicBox`
is deleted in favour of the unified engine.

**`DataContext` never prunes.** `data_` holds `weak_ptr`s and an expired entry is
reclaimed only when the *same* id is initialised again (`datacontext.h:70-79`).
The array machinery itself mints **no** `DataContext` id per identity — an
`ArrayId` keys only the nodes' own tables, and every `share()` in the
implementation is built when the description is, not per element. What does mint
at churn rate is a **delegate that builds something stateful**: a `forEach`
delegate returning a widget creates that widget's `Input`s and `share()`s, each
of which mints a `UniqueId` when the delegate runs. That is inherent to building
per item and is not something this design can avoid, so a long-lived window whose
list churns still accumulates dead entries. Pre-existing, but this is the first
design that reaches it at churn rate.

## Open questions

Points the design does not settle. Flagged rather than guessed. Sharing within a
single context is *not* one of them — it is a supported mechanism with an
existing reference implementation; see *Sharing: `SharedArraySignal`*.

- **The reactive subtree `AnySignal<ArraySignal<T>>`.** Still unimplemented and
  still unreconciled: the identity algebra says only construction and `forEach`
  mint identity, but this constructor hands out fresh ids on every swap. Either
  it is an acknowledged third minting site or the swap is modelled as a `join`
  over a signal-of-`ArraySignal`. Out of scope for the rewrite.
- **`filter`.** Not in the operator set and not needed by any container. Its
  eviction semantics genuinely conflict with once-per-identity — an item filtered
  out and later back in was never retired upstream, but the node evicted its `U`,
  so the delegate re-runs for an identity that never died and a filtered-out
  widget silently loses its state. Decide *before* adding it, not while.
- **Whether `Collection<T>` and `DataSource<T>` are removed or kept.** `forEach`
  supersedes the plumbing; `Collection` remains a reasonable mutable source. Note
  that `Collection`'s ids are pointer values, so a `Collection` source must feed
  its ids through as keys or be keyed on item content.

## Implementation order

Written for a from-scratch implementation. The spike on PR #100 is superseded and
should not be extended; read it for the `join` fixes and the test cases, and
otherwise start clean.

Steps 0 to 4 are done; the rest is outstanding.

0. **Fix `DataContext::initializeData`'s assert.** *Done.* A **prerequisite**,
   not a nicety. `data_` holds `weak_ptr`s and the assert required the id to be
   absent, but expired entries linger in the map. This design hits that path
   directly — a key disappears, every consumer drops it, the entry expires, the
   key returns — so a legal sequence fired the assert. An expired entry is now
   allowed.

1. **Prove the pick construction.** *Done.* The smallest thing that can fail.
   Build the
   shared keyed source and one `pick(key)` signal by hand, and show: it tracks its
   key across membership changes; it yields no value when its key leaves, and
   asking for one anyway throws; two `SignalContext`s over the same description
   get independent state; and the same description instantiated twice into *one*
   context shares the source. Nothing else proceeds until this is green.

2. **The node: `forEach`, `concat`, `map`, `join`.** *Done.* `detail::ArrayOnce`
   carries the key→id map and the `U` table in its `DataType`; the array holds it
   behind a `.share()`, which is what makes that data per-context and shared
   within one. `forEach` and `map` differ only in their key, build and id
   functions. The exit is a second node, `detail::ArrayJoin`, holding one inner
   data per identity — see *`join` is a node, not a `Join`*. The two `join` fixes
   landed first, as their own commit; they are still needed by `Signal::join`
   generally, but the array exit no longer uses it.

3. **`scatter`.** *Done.* Structurally the same node as `forEach` with a
   different source — `ArrayOnce` keyed by identity, with the slice signal built
   by the same `pick` construction over a keyed zip of the array's elements with
   the aggregate. The size-alignment check is that zip; what it reaches is
   *Alignment is a promise, and only its arity is checked*.

4. **`App`'s window list.** *Done.* The first `bqui` consumer, and the one that
   settled *No identity surfaces at the exit*. `App::run` builds a `WindowGlue`
   per identity with `map`, joins the glues through a constant, and reads the
   live set off the fanned-in vector. `App` then grew a window collection of its
   own — a `SharedVector<Window>` through `forEach` with the identity delegate,
   whose elements are the window signals — with `addWindowArray` keeping the
   general path for a caller whose windows follow a model of its own.
   Independent of steps 5 and 6 and landed before them.

5. **Unify `layout()`** over `map`/`scatter`/`join`, keeping the existing
   `SizeHintMap`/`ObbMap` signature so `box`, `stack` and `uniformGrid` are
   unaffected. Delete the dead heterogeneous tuple path
   (`accumulateSizeHintsTuple`, `MapObbs`) at the same time.

6. **Delete `dynamicBox`.** `hbox`/`vbox` take an `ArraySignal<AnyWidget>` and
   route to the unified `layout()`. This is where the missing `handleGravity()`
   and the element-cache bug disappear. Retire `dataBind` /
   `dataSourceFromCollection` here, porting `src/bqui/test/databindtest.cpp:18`
   and `src/testapp1/adder.cpp:84`.

Steps 0-3 are `bq` only and land before anything in `bqui` changes.

Step 4 also fixed an `ase` defect it was the first thing to reach: the WGL
backend never destroyed a window's `HWND`, so closing a window at runtime left a
dead one on screen that painted nothing and answered no input. Nothing could
close a window while the app ran before this step, which is why it had gone
unnoticed. The GLX backend has always destroyed its window.

### `App::run` can only be tested where the backend is headless

`App::run` opens windows, so on the GLX and WGL backends a test of it needs a
display and a GPU context that a CI runner does not have.
`src/bqui/test/apptest.cpp` is therefore compiled only where `ase` builds its
headless backend, which `ase_is_headless` in `src/ase/meson.build` reports.
Anything else that wants end-to-end coverage of the app loop inherits the
constraint, and the way out is to inject the platform into `App` rather than to
weaken the test — nothing has needed that yet.

### Tests the departed-key invariant requires

Beyond the usual add/remove/reorder coverage, three cases exist specifically to
exercise what the first implementation got wrong. They are cheap and they are the
regression net for the state-location rule:

- **Shrink then grow.** Remove the last element, update, add one back. This is the
  sequence that fires the `DataContext` assert of step 0, and the one in which a
  pick must go empty and be picked up again rather than reporting a stale value.
- **Remove while a sibling still reads.** A surviving element's signals must be
  untouched by a neighbour's removal — the spike's failure mode was a sibling's
  fold resetting on an unrelated insert.
- **Two contexts, diverging membership.** Realise one description in two
  `SignalContext`s and drive them apart. Under the spike this made the two evict
  each other's widget state; under this design they must be wholly independent.
