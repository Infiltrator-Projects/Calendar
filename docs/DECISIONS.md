# Decisions

This file records durable architectural decisions for Calendar. It complements `docs/DESIGN.md`: Design explains principles; this file records choices that future work should not casually reverse.

## ADR-001 — Portable chronology stays below Cinnamon

**Decision.** Calendar, clock, astronomy and event semantics remain in the native core. Cinnamon owns presentation, settings and desktop integration.

**Why.** Chronology must be deterministic and testable without a running desktop, while Cinnamon-specific APIs change independently.

**Consequence.** New calendar systems and clock calculations belong in `src/core/`; UI code should not become a second implementation.

## ADR-002 — Existing desktop behaviour is evidence, not the specification

**Decision.** Cinnamon's stock calendar surface is monitored for compatibility, but Calendar does not copy it mechanically.

**Why.** The project intentionally supports behaviour beyond the stock applet and must retain its own correctness model.

**Consequence.** Upstream changes trigger review, not automatic semantic adoption.

## ADR-003 — Historical and computational continuation rules are explicit

**Decision.** Historical evidence, modern standards and project continuation rules are distinguished in source, documentation and tests.

**Why.** A plausible date is not sufficient evidence that a model is historically or geographically universal.

**Consequence.** Every non-trivial calendar/clock model needs defined epoch, range, assumptions and boundary tests.

## ADR-004 — Common owns only genuinely generic mechanics

**Decision.** Calendar consumes Common for shared arithmetic, formatting, timing, loading and other neutral primitives, but Calendar-specific chronology stays local.

**Why.** Sharing should reduce duplication without weakening the strongest domain implementation.

**Consequence.** If Calendar has a superior generic helper, Common is improved first; Calendar does not keep a private fork indefinitely.

## ADR-005 — Runtime compatibility identifiers survive user-facing renames

**Decision.** Stable package/runtime identifiers are retained where changing them would break settings, ABI or upgrades.

**Why.** Product naming and compatibility identity are different concerns.

**Consequence.** Legacy internal identifiers may remain even when the visible product name is Calendar.