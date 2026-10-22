# Decisions

This file records durable architectural decisions for Calendar. It complements `docs/DESIGN.md`: Design explains principles; this file records choices that future work should not casually reverse.

## ADR-001 — Portable chronology stays below Cinnamon

**Decision.** Calendar, clock, astronomy and event semantics remain below the presentation layer. Cinnamon owns presentation, settings and desktop integration.

**Why.** Chronology must be deterministic and testable without a running desktop, while Cinnamon-specific APIs change independently.

**Consequence.** New calendar systems and clock calculations belong in portable temporal resources or Calendar core infrastructure below the JavaScript/native presentation boundary; UI code must not become a second implementation.

## ADR-002 — Existing desktop behaviour is evidence, not the specification

**Decision.** Cinnamon's stock calendar surface is monitored for compatibility, but Calendar does not copy it mechanically.

**Why.** The project intentionally supports behaviour beyond the stock applet and must retain its own correctness model.

**Consequence.** Upstream changes trigger review, not automatic semantic adoption.

## ADR-003 — Historical and computational continuation rules are explicit

**Decision.** Historical evidence, modern standards and project continuation rules are distinguished in source, documentation and tests.

**Why.** A plausible date is not sufficient evidence that a model is historically or geographically universal.

**Consequence.** Every non-trivial calendar/clock model needs defined epoch, range, assumptions and boundary tests.

## ADR-004 — Common owns only genuinely generic mechanics

**Decision.** Calendar consumes Common for shared arithmetic, formatting, timing, loading and other neutral primitives. Resource-specific chronology remains owned by the resource that implements it; Calendar-specific application behaviour remains local.

**Why.** Sharing should reduce duplication without weakening the strongest domain implementation.

**Consequence.** If Calendar has a superior generic helper, Common is improved first; Calendar does not keep a private fork indefinitely. Common owns the product-neutral temporal resource contracts and loaders, while the resource module owns the chronology that satisfies those contracts.

## ADR-005 — Runtime compatibility identifiers survive user-facing renames

**Decision.** Stable package/runtime identifiers are retained where changing them would break settings, ABI or upgrades.

**Why.** Product naming and compatibility identity are different concerns.

**Consequence.** Legacy internal identifiers may remain even when the visible product name is Calendar.

## ADR-006 — Stateful desktop protocols get explicit state owners

**Decision.** Multi-step desktop protocols with coupled transitions use a dedicated state owner rather than a constellation of controller flags. CalendarServer visible-range generation, admission, retry and queued-force state is owned by `EventRangeState`; the transport controller performs D-Bus and event-store work against that contract.

**Why.** Independent booleans make valid states implicit and allow one callback to update only part of a transition. An explicit state owner makes lifecycle invariants testable without binding tests to transport-controller internals.

**Consequence.** New CalendarServer lifecycle state belongs in the narrow state object when it participates in the range-request protocol. Presentation and transport code must not recreate parallel range-state flags. Completion is an exactly-once state transition: a generation match without a pending request is not sufficient, and duplicate/stale callbacks never replace the settled outcome.

## ADR-007 — Civil chronology has one native implementation

**Decision.** Civil-day navigation and weekday calculation are native Calendar responsibilities. Cinnamon JavaScript consumes the native APIs and must not maintain a second chronology implementation for the same operations.

**Why.** Parallel date arithmetic creates silent divergence at calendar boundaries, skipped local dates and large year ranges, while the native implementation is already the tested chronology authority.

**Consequence.** New civil-date arithmetic belongs below the JavaScript/native boundary. Cinnamon may coordinate navigation and presentation, but it delegates chronology semantics to Calendar/resource code.

## ADR-008 — Calendar and clock capabilities move toward filesystem-discovered resources

**Decision.** Installed temporal capabilities use the filesystem-discovered resource concept defined by `Libraries/docs/TEMPORAL_PROVIDERS.md`. Calendar resources are direct native modules under `/System/Resources/Calendars` and expose Common's generic metadata ABI plus Common's versioned executable calendar-provider ABI. Clock resources use the same namespace concept and Common's executable clock-provider ABI for both rendering and scheduling.

**Why.** Adding a temporal system should be an installation operation, not coordinated edits to Calendar, Common and System Settings merely to make the system visible. The executable behaviour contract therefore belongs at the system/Common boundary rather than inside the Calendar application.

**Consequence.** All thirty-one calendars and twenty-three explicit clocks are resource-only. Calendar's internal provider table is a resource adapter: filesystem modules never implement or depend on it. Missing installations never invalidate persisted policy IDs or reset the selection to another calendar or clock. A missing or invalid resource remains unavailable and must never silently become a different calendar. Direct native-module files are resource ABI version 1; richer bundle forms remain reserved until a real need justifies them.
