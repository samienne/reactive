# Style guide

*Last verified against `d2e8954` (2026-07-13).*

Prescriptive rules for how code and docs are written here — the rubric a review
(human or agent) checks a change against. It grows as we settle on how things
should be shaped. Where a rule has a *why* or a mechanism, it points at
`conventions.md`; this file stays a checklist.

## Formatting

- Follow `.clang-format` (Allman braces, 80-column limit, one argument per line
  when a call wraps, expanded lambdas). Match the surrounding code.
- To force a fluent chain or a template-argument list to break one-per-line
  where clang-format would otherwise pack it, end each element with a trailing
  empty `//` comment. These comments are load-bearing — keep them. (Mechanism:
  `conventions.md`.)
- Do not use `// clang-format off`. For dense template metaprogramming, extract
  the nested expression into a named `using` alias or `constexpr bool` predicate
  instead; it formats cleanly and reads better.

## Naming and types

- Use the `Any`-prefixed, type-erased forms (`AnySignal<T>`, `AnyWidget`,
  `AnyShape`, …) in public signatures and in prose. Only refer to the concrete
  `Signal<TStorage, Ts...>` form when explaining the type erasure itself.
- Public headers go under `src/<lib>/include/<lib>/` (the include firewall).

## Templates and builders

- Guard builder-style / rvalue-chained template APIs with a smoke test that
  force-instantiates every method (uninstantiated template methods are never
  compiled, so bugs hide until first use — see `conventions.md`).
- In rvalue-qualified (`&&`) methods, call other `&&` overloads through
  `std::move(*this)`.

## Documentation

- **API reference is Doxygen in the public headers.** Never transcribe
  signatures into Markdown.
- **Docs may point at code; code never points at docs.** Do not add code
  comments that reference these documents — they go stale and confuse readers.
- **One home per fact:** user narrative → `readme.md`; API contract → Doxygen;
  cross-cutting model / conventions / decisions → `docs/`.
- Keep the model/conventions/decisions docs signature-free and stamp them with
  the commit they were "verified against."

## Commits and PRs

- Keep commits focused; write messages that explain the *why*, not just the what.
- PRs merge as **merge commits**; squash only when explicitly asked; never
  rebase-merge.
- Every PR needs human review before merge; agents do not self-merge (see
  `AGENTS.md`).
