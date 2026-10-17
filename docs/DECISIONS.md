# Decisions

This file records durable architectural decisions for Calendar. It complements `docs/DESIGN.md`: Design explains principles; this file records choices that future work should not casually reverse.

## ADR-001 — Portable chronology stays below Cinnamon

**Decision.** Calendar, clock, astronomy and event semantics remain below the presentation layer. Cinnamon owns presentation, settings and desktop integration.

**Why.** Chronology must be deterministic and testable without a running desktop, while Cinnamon-specific APIs change independently.

**Consequence.** New calendar systems and clock calculations belong in portable temporal providers or Calendar core infrastructure below the JavaScript/native presentation boundary; UI code must not become a second implementation.

## ADR-002 — Existing desktop behaviour is evidence, not the specification

**Decision.** Cinnamon's stock calendar surface is monitored for compatibility, but Calendar does not copy it mechanically.

**Why.** The project intentionally supports behaviour beyond the stock applet and must retain its own correctness model.

**Consequence.** Upstream changes trigger review, not automatic semantic adoption.

## ADR-003 — Historical and computational continuation rules are explicit

**Decision.** Historical evidence, modern standards and project continuation rules are distinguished in source, documentation and tests.

**Why.** A plausible date is not sufficient evidence that a model is historically or geographically universal.

**Consequence.** Every non-trivial calendar/clock model needs defined epoch, range, assumptions and boundary tests.

## ADR-004 — Common owns only genuinely generic mechanics

**Decision.** Calendar consumes Common for shared arithmetic, formatting, timing, loading and other neutral primitives. Provider-specific chronology remains owned by the provider that implements it; Calendar-specific application behaviour remains local.

**Why.** Sharing should reduce duplication without weakening the strongest domain implementation.

**Consequence.** If Calendar has a superior generic helper, Common is improved first; Calendar does not keep a private fork indefinitely. Temporal-provider discovery and ABI mechanics may live in Common without transferring historical chronology policy into Common.

## ADR-005 — Runtime compatibility identifiers survive user-facing renames

**Decision.** Stable package/runtime identifiers are retained where changing them would break settings, ABI or upgrades.

**Why.** Product naming and compatibility identity are different concerns.

**Consequence.** Legacy internal identifiers may remain even when the visible product name is Calendar.

## ADR-006 — Stateful desktop protocols get explicit state owners

**Decision.** Multi-step desktop protocols with coupled transitions use a dedicated state owner rather than a constellation of controller flags. CalendarServer visible-range generation, admission, retry and queued-force state is owned by `EventRangeState`; the transport controller performs D-Bus and event-store work against that contract.

**Why.** Independent booleans make valid states implicit and allow one callback to update only part of a transition. An explicit state owner makes lifecycle invariants testable without binding tests to transport-controller internals.

**Consequence.** New CalendarServer lifecycle state belongs in the narrow state object when it participates in the range-request protocol. Presentation and transport code must not recreate parallel range-state flags.

## ADR-007 — Civil chronology has one native implementation

**Decision.** Civil-day navigation and weekday calculation are native Calendar responsibilities. Cinnamon JavaScript consumes the native APIs and must not maintain a second chronology implementation for the same operations.

**Why.** Parallel date arithmetic creates silent divergence at calendar boundaries, skipped local dates and large year ranges, while the native implementation is already the tested chronology authority.

**Consequence.** New civil-date arithmetic belongs below the JavaScript/native boundary. Cinnamon may coordinate navigation and presentation, but it delegates chronology semantics to Calendar/provider code.

## ADR-008 — Calendar and clock capabilities are runtime-discovered providers

**Decision.** The long-term extension model for calendars and clocks is the Common temporal-provider architecture defined by `Libraries/docs/TEMPORAL_PROVIDERS.md`. Available systems are discovered at runtime through stable provider IDs and a versioned C ABI rather than requiring additions to a central compiled capability catalogue.

**Why.** A temporal system should be a cohesive unit containing its own implementation, tests, metadata and historical/technical specification. Adding one should not require coordinated edits to Calendar's registry, Common's catalogue and System Settings merely to make the capability visible.

**Consequence.** Existing grouped translation units and compiled registries remain valid transitional code but are migration debt, not the target extension boundary. New work should prove the provider pattern, beginning with the first Egyptian calendar/time pair where practical. Calendar consumes the runtime registry and keeps application-specific navigation/event integration separate from provider-specific chronology. Provider failure or absence must remain local and explicit rather than silently substituting a different calendar or clock.
