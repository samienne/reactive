# Conventions & gotchas

*Last verified against `d2e8954` (2026-07-13).*

Idioms and traps that are not obvious from reading a single file. Know these
before editing the corresponding area.

## Naming

- **`Any` prefix = type erasure.** `AnySignal<T>`, `AnyWidget`, `AnyShape`,
  `AnyWidgetModifier` are the type-erased forms of their concrete templated
  counterparts (e.g. `Signal<Storage, T>`). Store and pass the `Any` form.
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

## Symbol visibility and DLL export (Windows)

Exported template instantiations use a macro scheme in
`src/btl/include/btl/visibility.h` (`BTL_EXPORT_TEMPLATE` etc., re-exported per
library as `AVG_EXPORT_TEMPLATE`, `BQUI_EXPORT_TEMPLATE`, …). The pattern for a
DLL-exported explicit instantiation is: an `extern template` declaration in the
header marked import/export, **and** the explicit instantiation *definition* in
the `.cpp` marked with the export attribute.

The trap: **MSVC and clang-cl differ.** MSVC will re-emit header-defined inline
members in each consumer even if nothing is exported, so it "works" by accident;
clang-cl honours `extern template` strictly and links against the DLL's exports,
so if the instantiation *definition* is not marked exported you get
unresolved-symbol link errors under clang-cl only. Always mark the definition,
not just the declaration. (This is why the Windows branch of `visibility.h`
gives `*_EXPORT_TEMPLATE` a real `dllexport`/`dllimport`.)

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

The GitHub Actions matrix builds Linux (gcc-10/11/12, clang-11 ± tracy), macOS,
and Windows (**MSVC and clang-cl**). A single aggregating job, **`ci-success`**,
`needs` all the build jobs and is the *only* required status check — new matrix
legs are covered automatically, and a new top-level job just needs adding to its
`needs` list. Mark only `ci-success` as required in branch protection.

Known flaky: `async.whenAllCancelOnFail` (in `src/btl/test/asynctest.cpp`)
intermittently fails on macOS runners; re-run the failed job to clear it.
