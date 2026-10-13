# Validation

## Purpose

Validation distinguishes implemented behaviour from behaviour that has actually been demonstrated. A build proves compilation; it does not by itself prove runtime, hardware or integration correctness.

## Automated evidence

The repository currently uses:

- .github/workflows/ci.yml
- .github/workflows/release.yml
- .github/workflows/upstream-drift.yml

The tests/ tree includes portable-core, exact clock-boundary, property, ABI, JavaScript runtime, settings and release-model tests. Live Cinnamon smoke testing is a separate integration boundary.

`make static-analysis` is the single static-analysis authority for shipped production C translation units. It analyses the native library/core/adapters with their GLib boundary flags and the separately linked GTK About helper with its GTK boundary flags; training and test-only programs remain qualified by their owning build/test gates.

The Cinnamon boundary suite includes behavioural tests for popup lifecycle/input ownership, effective temporal-policy resolution, CalendarServer transport/reconnect semantics and EventsManager orchestration through an injected transport port. CalendarServer tests cover both asynchronous completion failures and synchronous proxy/range-call initiation failures so recovery is qualified at both halves of the Gio async contract. Architecture checks enforce dependency direction and ownership without treating private source layout as a compatibility contract: portable core code cannot acquire Gio/D-Bus/settings objects, and positional native GVariant records are decoded through the named JavaScript native boundary rather than scattered field-number assumptions.

Automated checks should cover ordinary behaviour, important boundaries, malformed/error cases and release/package contracts appropriate to the project.

## Manual and environment-dependent evidence

Real Cinnamon-session behaviour, location-dependent astronomical presentation and platform integration that cannot be faithfully reproduced in a headless runner require explicit live testing.

Manual evidence supplements automation and must be described at the level actually observed. A simulator, fixture or mocked provider must not be described as physical-device proof.

Exact-source Cinnamon-session and location-dependent observations belong in [QUALIFICATION.md](QUALIFICATION.md), which records environment-dependent evidence without turning a skipped or unavailable live environment into a pass. Blocking live CI records the exact source/version and the runner's OS, Cinnamon, session, locale and time-zone context before exercising the exact candidate artifact.

## Release criterion

The exact revision intended for release must pass the required automated gates. Release assets must be derived from that revision, and documentation must not advertise known-failing or merely planned behaviour as supported.

## Regression rule

Every fixed defect should gain the narrowest useful permanent regression check when reproducible. Tests are part of the product contract rather than disposable scaffolding.
