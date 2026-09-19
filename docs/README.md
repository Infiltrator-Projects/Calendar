# Documentation

This directory is the canonical documentation entry point for Calendar. The Infiltrator project family uses the same baseline document roles in every repository so readers can move between projects without relearning the structure.

## Canonical baseline

- [Architecture](ARCHITECTURE.md) — ownership, layers, dependencies and system boundaries.
- [Design](DESIGN.md) — first-principles goals, non-goals, trade-offs and failure philosophy.
- [Decisions](DECISIONS.md) — durable architectural decisions, alternatives and consequences.
- [Roadmap](ROADMAP.md) — current foundation, near-term priorities and longer-term direction.
- [Validation](VALIDATION.md) — automated, manual and environment-specific evidence boundaries.
- [Project README](../README.md) — product overview, capabilities, build/use entry point and engineering ethos.
- [Changelog](../CHANGELOG.md) — user-visible and contract-relevant change history.
- [Contributing](../CONTRIBUTING.md) — development, ownership and verification rules.
- [Security](../SECURITY.md) — vulnerability scope, reporting and response policy.

## Documentation authority

The baseline files have distinct responsibilities and should not compete as alternate sources of truth. Architecture describes where behaviour belongs; Design explains why; Roadmap describes direction; Validation records what evidence is required. Code and tests remain authoritative for executable behaviour, while immutable tags/releases identify historical source.

Specialist documents may go deeper into one subsystem, protocol, platform, research area or historical investigation. They should link back to the canonical baseline when a reader needs the wider project context. Calendar keeps model evidence in `MODELS.md` and cross-platform/representation contracts in `PORTABILITY.md` so the project README can remain a product and build/use entry point.

## Specialist documentation

- [Chronology, time and astronomy models](MODELS.md) — evidence hierarchy, provenance, continuation rules, astronomical assumptions and model failure semantics.
- [Portability](PORTABILITY.md) — language, platform, representation, locale, ABI and compatibility boundaries.

## Maintenance rule

When a change moves an ownership boundary, support boundary, validation claim or major design decision, update the corresponding canonical document in the same change. Avoid copying the same status statement into several files; link to the authoritative document instead.
