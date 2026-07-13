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

Two audiences, kept separate:

- **Human docs — `readme.md`.** Concepts and usage, at the level that conveys
  the ideas. Root `readme.md` for the project; `src/<lib>/readme.md` per library.
- **Agent docs — `AGENTS.md` plus the small topic files it indexes.** Internals,
  entry points, gotchas, discovery. Root `AGENTS.md` for the project;
  `src/<lib>/AGENTS.md` per library.

Agent docs are a **knowledge base, not a manual**:

- Structure them as an **index plus small, single-topic files**, so a reader
  loads a cheap index and then only the one file it needs — never a whole
  document for one fact. A folder's `AGENTS.md` is that index (a one-line hook
  per file); promote to an `.agent/` folder of topic files only once the single
  `AGENTS.md` outgrows itself.
- Keep each file **small and single-topic** (soft cap: a few hundred lines).
  Split it when it covers more than one topic, or gets long enough that loading
  it for one fact is wasteful. Do not over-fragment — a topic per file, not a
  fact per file.
- **Co-locate a library's docs under `src/<lib>/`** so they travel with the code
  if it becomes its own repository. Cross-cutting rules stay at the top level and
  are **referenced, not duplicated**.

General rules:

- **API reference is Doxygen in the public headers.** Never transcribe
  signatures into Markdown.
- **Docs may point at code; code never points at docs.**
- **One home per fact:** concepts/usage → `readme.md`; API contract → Doxygen;
  cross-cutting model/conventions/decisions → top-level `docs/`; library-specific
  knowledge → that library's `AGENTS.md`.
- **Technical docs must be precise.** `docs/` and `AGENTS.md` state things
  exactly; illustrative simplifications belong only in a human-facing `readme.md`.
- Stamp the model/conventions/decisions docs with the commit they were "verified
  against."

## Comments

- A comment describes the code **as it is now**, for a reader with no outside
  context; it should still make sense a year later to someone who knows nothing
  about how the code got there.
- Keep **history and context out of comments** — why a change was made, what the
  previous version did, a reference to some discussion. That knowledge rots as a
  comment and confuses code readers; put it in an agent topic file (or
  `decisions.md` if it is a decision).
- A short comment explaining a **non-obvious invariant or a genuinely surprising
  line** — timeless rationale the code reader needs — is good and stays inline.
  The rule targets context and history, not all rationale.
- Prefer a small KB topic file over a long, context-heavy comment. Comments
  *describe*; the knowledge base *explains and remembers*.

## Commits and PRs

- Keep commits focused; write messages that explain the *why*, not just the what.
- PRs merge as **merge commits**; squash only when explicitly asked; never
  rebase-merge.
- Every PR needs human review before merge; agents do not self-merge (see
  `AGENTS.md`).
