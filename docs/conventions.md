# Conventions & gotchas

*Last verified against `8677be3` (2026-07-21).*

Idioms and traps that are not obvious from reading a single file. Know these
before editing the corresponding area.

## Naming

- **`Any` prefix = type erasure.** `AnySignal<T>`, `AnyWidget`, `AnyShape`,
  `AnyWidgetModifier` are the type-erased forms of their concrete templated
  counterparts (e.g. `Signal<TStorage, Ts...>`). Store and pass the `Any` form.
- **`bq::signal::ArraySignal<T>` is the one exception.** The `Any` prefix marks
  the erased half of a pair whose other half is storage-typed, and that pair
  exists so a chain of combinators can stay free of virtual dispatch. An array is
  heap-backed regardless, so the per-element indirection a storage-typed twin
  would avoid is already paid: it would buy nothing and double the surface.
  `ArraySignal` is therefore type-erased by construction, and the asymmetry with
  `Signal` is the decision rather than an oversight. The rule that follows is in
  `docs/style-guide.md`.
- Project is *Reactive*; the libraries are `bq` (signal core) and `bqui` (UI).
  The repo is still named `reactive` from before the rename.

## The include-directory firewall

Public headers live in `src/<lib>/include/<lib>/`, and only that directory is
added to the library's exported include path. Consequently `#include
<lib/header.h>` resolves **only if `<lib>` is a declared dependency**. This is
intentional: because so much lives in templated headers, a missing dependency
edge should be a hard `#include` error, not a silent compile against another
library's internals. When adding a public header, put it under the nested
`<lib>/` directory.

## Symbol visibility and DLL export

Exported template instantiations use a macro scheme in
`src/btl/include/btl/visibility.h`, re-exported per library (`AVG_EXPORT`,
`AVG_EXPORT_TEMPLATE`, …). **The two sites take different macros:** the `extern
template` *declaration* in the header takes `*_EXPORT_TEMPLATE` (or
`*_IMPORT_TEMPLATE` in a consumer), and the explicit instantiation *definition*
in the `.cpp` takes `*_INSTANTIATE_TEMPLATE`. The `*_INSTANTIATE_TEMPLATE`
re-export exists only where a library has explicit instantiations — today just
`avg`.

They are not interchangeable. On Windows and on GCC the attribute is accepted at
one site and diagnosed at the other (`C4910` /
`-Wdllexport-explicit-instantiation-decl`, and `-Wattributes` respectively), so
the macro is defined empty at the site that would reject it. On clang targeting
ELF/Mach-O both expand to the visibility attribute.

The trap: **MSVC and clang-cl differ.** MSVC will re-emit header-defined inline
members in each consumer even if nothing is exported, so it "works" by accident;
clang-cl honours `extern template` strictly and links against the DLL's exports,
so if the instantiation *definition* is not marked exported you get
unresolved-symbol link errors under clang-cl only. This is why
`BTL_INSTANTIATE_TEMPLATE` is a real `dllexport` on Windows even though
`BTL_EXPORT_TEMPLATE` is empty there.

The other trap: **GCC cannot export bqui's type-erased instantiations at all.**
The libraries build with `gnu_symbol_visibility: 'hidden'`, and GCC fixes a
specialization's visibility when the specialization is first *named*. It takes
the **minimum of the explicit attribute and the computed minimum over the
template arguments** — an attribute is honoured, but it cannot lift an argument
that is itself hidden. The type-erased classes name their own erased
specialization inside their bodies — `operator AnyWidget() &&` and friends — so
the specialization is fixed at that point, through `std::function` of a hidden
type, and no later attribute reaches it. The instantiation is emitted, but
localised when the shared object is linked, so a consumer's reference is
unresolved.

Raising it would mean marking every type in the argument tree default-visible,
including `bq::signal::AnySignal` — a cross-library ABI widening with no
compile-time backstop for the next unmarked type to join the tree. So
**`Widget`, `Element`, `WidgetModifier`, `ElementModifier` and
`BuilderModifier` carry no `extern template` declaration on any toolchain**;
every translation unit instantiates them implicitly. `Element` had been doing
exactly that for years already.

This is specific to classes that name their own erased specialization.
`avg::Animated<T>` keeps the `extern template` scheme and exports normally,
because it has no such self-conversion operator.

**A Release build hides the whole problem** — the members are small enough to
inline at every call site, so no external reference is emitted. That is why the
gcc-12 and clang-11 **Debug** matrix legs exist.

GCC **9 through 16** were probed and behave identically, so this is not
something to gate on a version range. clang is unaffected.

## Templates: latent bugs and smoke tests

Member functions of class templates are only compiled when **instantiated**
(i.e. actually called). So a broken method — infinite recursion, a non-existent
operator, a wrong ref-qualifier — compiles fine and passes CI until something
calls it. Guard builder-style APIs with a **smoke test that force-instantiates
every method** (see `src/bqui/test/shapetest.cpp`); that turns latent breakage
into a compile error.

## Rvalue-ref-qualified builders

The fluent builders (e.g. `bqui::shape::Shape`) rvalue-qualify their chaining
methods (`&&`). Inside such a method, `*this` is an **lvalue**, so calling
another `&&`-qualified overload on it requires `std::move(*this)`. Forgetting
this is a latent bug (see above): `return foo(...)` may need to be
`return std::move(*this).foo(...)`.

## `merge()` has no zero-argument form

`bq::signal::merge(...)` is variadic but does **not** support zero signals
(`ConcatSignalResults<>` has no result type), and with zero arguments it is not
even found by ADL. A signal graph with no inputs — e.g. a shape with no animated
parameters — must be built as a *constant* instead of via an empty `merge`
(see the `if constexpr (sizeof...(Ts) == 0)` branch in
`src/bqui/include/bqui/shape/shape.h`). The principled fix would be a
`signal<void>` identity element for `merge`; see `docs/decisions.md`.

## Compiler warnings

The project builds at `warning_level=3`; **third-party code does not**. Every
subproject is configured with `warning_level=0` and its headers enter through
`include_type: 'system'` / `.as_system()`, and the vendored
`src/avg/src/clipper/` builds as its own target with warnings off. Keeping the
noise out at that boundary is what makes a `-Wno-*` in our own targets
unnecessary — do not add one; fix the code instead.

Two things this cannot reach, so do not be surprised by them: Meson's
`warning_level=0` emits no flag at all rather than `-w`, so a compiler's
*default-on* diagnostics still fire inside a subproject; and Meson emits plain
`/I` for a system include under MSVC (never `/external:I`), so third-party
headers warn on the `msvc-17` leg but not on `clang-cl`.

## clang-format

- Config is `.clang-format` (based on WebKit, heavily customised). Key settings:
  `NamespaceIndentation: All`, `AlwaysBreakTemplateDeclarations: Yes`,
  `ColumnLimit: 80`, `BinPack*: false`, `AllowShortLambdasOnASingleLine: None`.
  The CI clang-format job is currently disabled.
- clang-format cannot force a fluent chain or a template argument list onto
  one-item-per-line when it would otherwise fit within the column limit. The
  in-code workaround is a **trailing empty `//` comment** on each element, which
  forces the break. It is load-bearing — do not remove those comments.

The prescriptive rules that follow from this (the `//` trick, extracting dense
template metaprogramming into named aliases, Doxygen-only API reference) live in
`docs/style-guide.md`.

## CI

The GitHub Actions matrix builds Linux (gcc-10/11/12, clang-11 ± tracy, one
clang-15 ASan+UBSan leg, plus gcc-12 and clang-11 in Debug), macOS (Release and
Debug), and Windows (**MSVC and clang-cl**). A single aggregating
job, **`ci-success`**, `needs` all the build jobs and is the *only* required
status check — new matrix legs are covered automatically, and a new top-level
job just needs adding to its `needs` list. Mark only `ci-success` as required in
branch protection.

Builds run through **`lw`** (loomworks), not direct meson calls: a leg is a
configuration set from `loomworks.json` plus a coarse toolchain pin, and the two
composite actions under `.github/actions/` do the rest. One trap:

- **Create profiles with `--local`.** Profiles are per-machine but default to
  local+shared, so an unqualified `lw profile create` followed by `lw publish`
  would push a runner's toolchain into the committed `loomworks.json`.

Known flaky: `async.whenAllCancelOnFail` (in `src/btl/test/asynctest.cpp`)
intermittently fails on macOS runners; re-run the failed job to clear it.
