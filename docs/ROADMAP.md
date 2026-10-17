# Roadmap

Calendar is feature-complete for its current Linux/Cinnamon product scope. This
document therefore defines a maintenance boundary rather than a queue of work
that must be invented before the application can be considered finished.

## Completion baseline

- preserve the 30-calendar and alternative-clock capability with explicit model provenance
- preserve the separation between portable chronology/event logic and Cinnamon presentation
- preserve the installed ABI, settings identity, package identity and upgrade path
- keep unavailable astronomical or platform states explicit rather than guessing values
- keep proprietary font binaries outside the redistributed source/package while honouring the preferred typography contract when those fonts are installed
- require the exact release revision to pass the automated qualification gates

## Maintenance priorities

Correctness defects, supported-platform regressions, security problems, upstream
Cinnamon compatibility changes and evidence gaps take priority over new
features. A fixed defect should gain the narrowest useful permanent regression
test. Tests that are completely subsumed by stronger coverage should be
consolidated rather than retained for historical reasons.

The temporal extension architecture is an intentional maintenance/refactoring
direction: preserve every existing compiled calendar/clock while Common gains a
parallel filesystem-discovery path for additional resources. The first new
Egyptian calendar/time pair is the intended proving case because it can establish
what a real drop-in resource needs without first disturbing compatibility-sensitive
legacy providers.

## Optional expansion

New calendars, clocks, localisation, accessibility work or platform backends are
optional product expansion, not missing completion work. They enter the product
only when ownership, provenance, failure semantics and a credible validation
path are explicit.

Once the resource model has been proved by real implementations, adding a
calendar or clock should normally mean installing a conforming self-contained
resource with its own implementation, tests and documentation rather than
editing a central Calendar capability list. The physical resource representation
remains deliberately undecided until that proving work is complete.

## Completion rule

Calendar is complete when implementation, tests, packaging and maintained
documentation agree for the declared product scope. Completion does not mean
that future defects are impossible; it means there is no known mandatory
feature tranche left to implement.
