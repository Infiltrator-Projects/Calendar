<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Chronology, time and astronomy models

Calendar exposes computational models, not unexplained date strings. Every supported calendar or alternative clock must have an identifiable authority, epoch or reference rule, continuation policy, validity boundary and failure behaviour that can be tested.

This document is the specialist source of truth for model evidence and interpretation. `docs/ARCHITECTURE.md` owns module boundaries; `docs/DESIGN.md` owns project-wide engineering principles; implementation and tests remain authoritative for executable behaviour.

## Evidence hierarchy

A chronology or time model should prefer evidence in this order:

1. an authoritative standard, primary historical rule or maintained reference implementation whose semantics are explicit;
2. established scholarly calendrical or astronomical literature with a reproducible algorithm;
3. a documented project continuation rule when historical evidence or standards do not define the requested range;
4. unavailable or unsupported when the project cannot justify a deterministic interpretation.

A familiar name, convenient formula, plausible output or another desktop application's behaviour is not enough to establish correctness.

## Model classes

Calendar currently combines several kinds of model:

- ICU/CLDR-backed calendars whose locale data and supported variants are delegated to ICU;
- deterministic native calendar algorithms owned by Calendar;
- exact partitioning clocks defined by integer or rational division;
- astronomical clocks whose output depends on a documented solar, sidereal or equinox model and configured coordinates;
- event semantics that normalise CalendarServer data into Calendar-owned interval contracts.

These classes have different authorities and should not be described as though they share one source of truth.

## Calendar authorities

| Area | Authority / model |
| --- | --- |
| Conventional panel time and Gregorian locale presentation | CinnamonDesktop.WallClock |
| Hebrew, Islamic, Persian, Chinese, Dangi, Indian, Coptic, Ethiopic, Buddhist, Japanese and Minguo calendars | ICU/CLDR calendar data |
| Julian, ISO week, French Republican, Roman, Mayan, Badíʿ, International Fixed, World, Positivist, Revised Julian, Byzantine Anno Mundi, Egyptian civil (Nabonassar era) and traditional Armenian calendars | Calendar deterministic native algorithms |
| French Republican decimal, Internet, Unix, hexadecimal, binary and Chinese hundred-kè clocks | Exact integer/rational partitioning |
| Sidereal, solar, Roman temporal, Edo seasonal, Italian, Babylonian-hour, Indian ghaṭī and Nuremberg clocks | Calendar astronomical models using configured coordinates |

Sweden's 1700–1753 civil calendar is modelled explicitly, including 30 February 1712.

## Historical scope and continuation rules

Historical names do not imply universal historical reconstruction. Where a system varied by locality, observation or era, Calendar documents the convention it computes and distinguishes that convention from historical universality.

Examples:

- modern Badíʿ years use a Tehran-referenced astronomical March equinox and sunset boundary;
- years before 172 B.E. retain the historical Western 21-March civil convention;
- Italian hours are equal hours measured from computed sunset;
- the label “Babylonian hours” refers to the later European gnomonic convention of equal hours from sunrise, not a claim to reproduce ancient Mesopotamian civil timekeeping;
- Islamic calendar output is computational and is not a local crescent observation.

A continuation rule is a project contract and must be identified as such rather than presented as discovered history.

## Astronomical assumptions

Astronomical modes use explicit computational approximations. A result is valid only when the required input and physical event exist under that model.

Solar and seasonal clocks may be unavailable at polar latitudes. Calendar must return an unavailable state rather than extrapolate a sunrise, sunset or temporal-hour boundary that the model cannot establish.

Coordinates use latitude north-positive and longitude east-positive in degrees. No location-dependent model silently substitutes 0°,0°.

## Model provenance

Calendar treats references as evidence for specific rules, not as a blanket claim that all implementation details are copied from them.

| Model / decision | Primary reference used by the implementation |
| --- | --- |
| General calendrical algorithms, epochs and cross-calendar reference practice | Edward M. Reingold and Nachum Dershowitz, *Calendrical Calculations: The Ultimate Edition*, 4th ed., Cambridge University Press, 2018, DOI 10.1017/9781107415058 |
| ICU-backed calendar variants and locale data | Unicode TR35 / LDML and ICU calendar implementations |
| Solar equation-of-time/declination and sunrise/sunset conventions | NOAA Global Monitoring Laboratory Solar Calculation Details, using a Meeus-derived model and the conventional 0.833° sunrise/sunset assumption |
| Equinox calculation | Jean Meeus, *Astronomical Algorithms*, 2nd ed., Willmann-Bell, 1998 |
| ΔT conversion used by the modern Badíʿ calculation | Fred Espenak and Jean Meeus, NASA GSFC, Polynomial Expressions for Delta T |
| Modern Badíʿ Naw-Rúz reference location and equinox rule | Universal House of Justice, message dated 10 July 2014 on implementation of the Badíʿ calendar |
| Cinnamon integration behaviour | Reviewed Linux Mint Cinnamon calendar surface recorded in `tools/upstream-calendar-baseline.json` |

The source module should keep a reference or explanatory comment close to constants, epochs and historical rules that materially affect output.

## Exactness versus approximation

An exact integer/rational partitioning model should remain exact across its supported range. An astronomical model may be approximate by design, but the approximation must be identified and tested at meaningful reference points.

Tests should not convert an approximate model into a false claim of physical or historical exactness. Conversely, approximation is not permission for arbitrary tolerance: the implementation should have a documented numerical expectation appropriate to the model.

## Boundary behaviour

New and existing models should explicitly cover relevant boundaries such as:

- epoch transitions;
- leap rules and exceptional civil dates;
- negative/proleptic years where supported;
- era-label boundaries;
- day-boundary transitions;
- timezone and daylight-saving transitions where platform civil time participates;
- sunrise/sunset absence;
- equinox boundary decisions;
- integer/range limits;
- locale data unavailable or unsupported.

A boundary that changes semantics should have a deterministic regression test where practical.

## Failure semantics

Unavailable, unsupported, malformed or physically undefined states are first-class outcomes. Returning an empty, false, null or unavailable result according to the owning API is correct when the model cannot establish a value.

A plausible-looking fabricated date or clock value is a correctness failure.

## Adding a calendar or clock

A new model should not be accepted merely because an algorithm can be written. Before inclusion it should establish:

- the owning authority or literature;
- epoch/reference rule;
- supported range;
- year-numbering and day-boundary convention;
- locality or observational assumptions;
- continuation policy outside historically defined ranges;
- arithmetic and unit representation;
- unavailable/error conditions;
- cross-reference examples and edge cases;
- a validation strategy strong enough to detect a convincing but wrong implementation.

When those cannot be stated precisely, the model is not ready for production support.
