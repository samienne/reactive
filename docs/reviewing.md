# Reviewing

*Last verified against `677f12b` (2026-08-14).*

Knowledge for the **clean-context review** step (see `AGENTS.md` → *Before
merging a PR*). This file holds *how to review* — what to weight, and the
false-positives and judgment calls that accumulate from human reviews. It does
**not** restate the rules a change must follow: those live in `style-guide.md`
and `conventions.md`, which a reviewer also reads. Keep this curated, not a log.

## Inputs a reviewer loads

- `docs/style-guide.md` and `docs/conventions.md` — the rubric.
- The changed area's `src/<lib>/AGENTS.md` — for library-specific context.
- This file — accumulated review lessons.

## What to weight

- **Verify doc claims against the code.** For documentation changes this is the
  real correctness axis — read the headers the doc points at and confirm the
  claim, rather than trusting the prose.
- For code changes: correctness and logic first, then rubric compliance.
- Weight findings by impact: a wrong/misleading claim outranks a cosmetic one.
- **Weight comment/Doxygen narration of mechanism or design rationale heavily
  — flag it** (see `style-guide.md` → Comments and the Doxygen bullet under
  Documentation). `[should-fix]`.
- **Doxygen block layout** — the opening `/**` and the closing `*/` must each
  sit alone on their own line, with content under aligned ` * ` lines (see
  `style-guide.md` → the Doxygen block-layout bullet). Flag `/** @brief ...`
  openers and inline `... */` closers; applies even to one-line briefs. Purely
  mechanical, so cite the exact line. `[should-fix]`.

## Known false-positives — do not flag

- **Trailing empty `//` comments** in fluent chains and template-argument lists
  are load-bearing (they force clang-format to break); they are not mistakes.
- The **`if constexpr (sizeof...(Ts) == 0)` constant path** in
  `shape/shape.h`'s `makeShapeUnchecked` is intentional — `merge()` has no
  zero-argument form.
- **Missing Doxygen** is not blocking; it is backfilled incrementally as headers
  are touched.
- Per-library **`readme.md` files carry no "verified against" stamp** on purpose
  — they are concepts/usage, not a contract.

## Cross-binary type-identity hazard

Type identity compared across a binary boundary is a hazard: a key derived from
a type name or its hash can collide, so a lookup keyed that way can
false-positive and hand back the wrong concrete type. The `getImplOfType`
primitives on `ase::WindowImpl` and `ase::PlatformImpl` (behind `Window::getImpl`
/ `Platform::getImpl`) are a **deliberate, scoped exception** — flag a repeat of
the hazard, not these. They key on `std::type_index`, are same-binary by
construction, and fail safe if that ever breaks; see `decisions.md` for why.

## Precision anchors

The rubric requires technical docs to be precise (`style-guide.md`); this is the
recurring spot to check. The concrete signal type is `Signal<TStorage, Ts...>`
(a value pack), and `AnySignal` *derives from* `Signal<void, Ts...>` rather than
aliasing it — confirm the technical docs say so exactly (a `readme.md` may
simplify).

## Severity

- `[blocking]` — wrong or broken: an inaccurate claim, or a rule violation that
  misleads a reader.
- `[should-fix]` — imprecise or overstated, but not wrong enough to block.
- `[nit]` — cosmetic (wording, path rooting, a soft cross-reference).

## The learning loop

After a human review, distill any reusable lesson into its home: a **rule** →
`style-guide.md` / `conventions.md`; a **heuristic or false-positive** → this
file. This can be done by an agent reading the human's PR comments and proposing
the additions. That is how the reviewer improves without a human repeating
themselves.
