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

The temporal extension architecture is now executable for calendars. Every
existing compiled calendar remains available while trusted native calendar
resources may additionally be discovered from `/System/Resources/Calendars`.
Common owns and validates both the generic temporal-resource metadata ABI and
the executable calendar-provider ABI; Calendar adapts validated system resource
modules into its private chronology dispatch instead of defining a second plugin
contract. Built-in identifiers remain authoritative on duplicates, so optional
resources cannot silently replace established chronology.

Clock resource metadata discovery exists in Common, but Calendar does not expose
a filesystem clock as selectable merely because metadata is present. A real
clock implementation ABI must be designed and qualified before that second half
of the migration becomes executable capability.

The first new Egyptian calendar/time pair remains the intended historical proving
case. It should validate the architecture with real scholarship and algorithms,
not with placeholder chronology.

## Optional expansion

New calendars, clocks, localisation, accessibility work or platform backends are
optional product expansion, not missing completion work. They enter the product
only when ownership, provenance, failure semantics and a credible validation
path are explicit.

For calendars, adding capability can now mean installing a conforming trusted
native module with its own implementation, tests and documentation rather than
editing Calendar's built-in provider table. Direct native-module files are the
version-1 executable resource form; directory bundles remain deliberately
reserved until a real resource requires a richer packaging model.

## Completion rule

Calendar is complete when implementation, tests, packaging and maintained
documentation agree for the declared product scope. Completion does not mean
that future defects are impossible; it means there is no known mandatory
feature tranche left to implement.
