# Roadmap

This is a direction document, not a dated promise. The released source and tests define what is actually supported.

## Current foundation

- maintain the 30-calendar and alternative-clock capability with explicit provenance
- keep the native core and Cinnamon shell separated
- protect ABI, settings, packaging and runtime integration with automated tests

## Near-term priorities

- strengthen edge-case and historical-boundary evidence as models evolve
- continue consolidating genuinely generic mechanics into Common without exporting calendar policy
- keep live Cinnamon integration and upstream-drift checks aligned with supported Mint/Cinnamon versions

## Longer-term direction

- extend chronology or clock coverage only when rules and provenance can be documented precisely
- improve accessibility and localisation without weakening deterministic model behaviour

## Admission rule

A proposed capability enters the roadmap only when its ownership is clear and there is a credible way to validate it. Features that require pretending uncertain behaviour is known do not qualify.

## Completion rule

An item is complete when implementation, tests, user-visible behaviour and maintained documentation agree. A checkbox or release number cannot substitute for missing evidence.
