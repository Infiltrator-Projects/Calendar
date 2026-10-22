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

The temporal architecture combines several kinds of model:

- native calendar modules that own conversion, navigation and date presentation;
- resource-owned ICU/CLDR bridges for the delegated lunisolar models and locale data;
- clock modules that own both rendering and exact next-visible-boundary scheduling;
- application-owned event semantics that normalise CalendarServer data into interval contracts.

Common discovers, validates and loads these resources. Calendar, Files, System Settings and the desktop consume the same executable resource ABIs. The independently installed resource pack owns the algorithms, ICU runtime dependency and translations; uninstalling a consumer does not remove it.

These classes have different authorities and should not be described as though they share one source of truth.

## Calendar authorities

| Area | Authority / model |
| --- | --- |
| OS-following panel time | CinnamonDesktop.WallClock |
| Explicit 12/24-hour clocks and Gregorian date presentation | Independently installed clock/calendar resources |
| Gregorian, Hebrew, Persian, Indian, Coptic, Ethiopic, Buddhist, Japanese, Minguo, Islamic civil/tabular and Umm al-Qura arithmetic/navigation | Calendar Resources deterministic native algorithms, with locale-sensitive formatting still delegated to ICU/CLDR |
| Computational Islamic, Chinese and Dangi lunisolar conversion/navigation | ICU/CLDR calendar implementations |
| Julian, ISO week, French Republican, Roman, Mayan, Badíʿ, International Fixed, World, Positivist, Revised Julian, Byzantine Anno Mundi, Egyptian civil (Nabonassar era) and traditional Armenian calendars | Calendar Resources deterministic native algorithms |
| French Republican decimal, Internet, Unix, hexadecimal, binary and Chinese hundred-kè clocks | Independent clock resources owning both rendering and exact display-boundary scheduling |
| Sidereal, solar, Roman temporal, Edo seasonal, Italian, Babylonian-hour, Indian ghaṭī and Nuremberg clocks | Independent clock resources owning rendering and astronomical boundary scheduling using configured coordinates where required |

Sweden's 1700–1753 civil calendar is modelled explicitly, including 30 February 1712.

Chinese and Dangi date/year labels use ICU/CLDR's localized native cyclic year names (`U`), such as `bing-wu`, `丙午` or `병오`. They do not add a related Gregorian year (`r`) or display a bare cycle-year counter (`y`). These traditional names repeat every sixty years; canonical civil coordinates and navigation retain the complete underlying date independently of the displayed name. The UI language controls wording without changing calendar day/month/year rules. Common's bounded executable calendar ABI supplies the same resource presentation to Calendar, Files and System Settings.

Edo clock timestamps use Japanese branch names and traditional bell numerals, with surrounding wording in the UI language. Branch names distinguish the repeated day/night bell counts; English animal explanations belong outside timestamps. The two documented solar-boundary models remain separate. Conventional 12-hour clock markers follow the requested locale, including locale-specific ordering, through the selected clock resource.

## Range and continuation matrix

The installed resource package contains 31 independent calendars and 23 independent explicit clocks. Calendar's own provider registry adapts only the validated resources Common reports as installed. The tables below state the maintained range/continuation contract without pretending that every historical name is a universal historical reconstruction.

### Calendar providers

| Provider id | Range / continuation contract |
| --- | --- |
| `gregorian` | Resource-owned proleptic Gregorian arithmetic across the representable civil-date domain; locale-sensitive formatting remains ICU/CLDR-backed. |
| `julian` | Proleptic Julian arithmetic across Calendar's representable civil-date domain. |
| `iso-week` | ISO week arithmetic derived from the proleptic Gregorian civil-date domain. |
| `hebrew` | Resource-owned deterministic Hebrew arithmetic with the 19-year cycle and postponement rules; locale-sensitive formatting remains ICU/CLDR-backed. |
| `islamic` | ICU computational Islamic model; not local crescent observation. Effective supported range is ICU-defined. |
| `islamic-civil` | Resource-owned civil/tabular Islamic arithmetic using the Friday epoch. |
| `islamic-umalqura` | Resource-owned Umm al-Qura month table for 1300–1600 AH; outside that maintained table the resource reports unavailable rather than inventing a continuation. |
| `persian` | Resource-owned Solar Hijri arithmetic compatible with the maintained ICU rule/correction set; locale-sensitive formatting remains ICU/CLDR-backed. |
| `chinese` | ICU traditional Chinese calendar; effective supported range and cyclical fields are ICU-defined. |
| `indian` | Resource-owned Indian National/Saka arithmetic derived from the proleptic Gregorian year. |
| `coptic` | Resource-owned Coptic arithmetic with its 13-month leap cycle. |
| `ethiopian` | Resource-owned Ethiopic Amete Mihret arithmetic with its 13-month leap cycle. |
| `buddhist` | Resource-owned Gregorian-derived Buddhist year arithmetic; locale-sensitive formatting remains ICU/CLDR-backed. |
| `thai-buddhist-solar` | Thai solar civil calendar from 1 April 1912: April New Year through 1940, the shortened 2483 B.E. year, and January New Year from 1941. Earlier dates and nonexistent January–March 2483 B.E. are unavailable. |
| `japanese` | Resource-owned modern Japanese era boundaries (Meiji through Reiwa) and Gregorian-derived arithmetic; locale-sensitive formatting remains ICU/CLDR-backed. |
| `minguo` | Resource-owned Republic of China/Minguo year arithmetic over the proleptic Gregorian calendar. |
| `french-republican` | Historical epoch at JDN 2375840 (22 September 1792 = Year I), with the resource's explicit Romme-style arithmetic continuation outside the historical republican era. |
| `roman` | Roman date naming over the proleptic Julian calendar; not a separate absolute chronology. |
| `mayan` | Arithmetic Long Count over kin using the GMT 584283 correlation. GMT is a family of nearby proposed correlations, so absolute Gregorian correspondence is explicitly model-dependent; pre-epoch arithmetic remains reversible. |
| `bahai` | Years 1–171 B.E. use the historical Western 21-March convention. From 172 B.E., years whose corresponding Gregorian year is 1000–3000 use the Tehran/equinox model; outside that astronomical range the resource deliberately returns to the 21-March continuation. |
| `international-fixed` | Deterministic reform-calendar mapping derived from the proleptic Gregorian civil year, including explicit intercalary days. |
| `world` | Deterministic World Calendar mapping derived from the proleptic Gregorian civil year, including explicit intercalary days. |
| `positivist` | Comte's Positivist calendar: Gregorian 1789 is Year 1 of the Great Crisis; 13×28 days plus one or two festival days, with Gregorian leap-year alignment. |
| `revised-julian` | Milanković 1923 leap rule continued arithmetically in both directions, anchored where Revised Julian and Gregorian coincide at 2000-01-01. |
| `byzantine` | Julian month/day with a 1 September year boundary and Constantinopolitan Anno Mundi era; continued arithmetically using astronomical year numbering internally. |
| `egyptian-nabonassar` | Wandering 365-day Egyptian civil year anchored at 1 Thoth year 1 = JDN 1448638; arithmetic continuation in both directions. |
| `dangi` | ICU Dangi calendar; effective supported range and cyclical data are ICU-defined. |
| `ethiopic-amete-alem` | Resource-owned Ethiopic Amete Alem arithmetic using the maintained 5500-year era offset. |
| `islamic-tbla` | Resource-owned tabular Islamic arithmetic using the astronomical Thursday epoch. |
| `armenian-traditional` | Traditional 365-day wandering year anchored to 11 July 552 Julian, with twelve 30-day months plus five epagomenal days and no leap day; arithmetic continuation in both directions. |
| `swedish-historical` | Actual Swedish civil transitions are modelled for 1700–1753, including 30 February 1712 and the 1753 omission. Earlier dates use Julian labels; dates from March 1753 onward use Gregorian labels rather than inventing a proleptic Swedish system. |

### Clock resources

| Provider id | Range / continuation contract |
| --- | --- |
| `standard-12` | Explicit locale-sensitive 12-hour civil clock over the representable instant domain. |
| `standard-24` | Explicit locale-sensitive 24-hour civil clock over the representable instant domain. |
| `decimal` | Exact ten-hour partition of the local civil day for any representable input instant. |
| `internet` | Exact 1000-beat partition using the defined Internet Time offset for any representable input instant. |
| `unix` | Signed Unix epoch seconds over the representable native instant domain. |
| `hexadecimal` | Exact hexadecimal partition of the civil day for any representable input instant. |
| `binary` | Exact binary presentation of the civil-day partition for any representable input instant. |
| `sidereal` | Continuous local sidereal model for finite longitude; unavailable for invalid coordinates. |
| `solar` | NOAA/Meeus-derived apparent solar time for finite longitude; unavailable for invalid coordinates. |
| `julian` | Astronomical Julian Date derived directly from the representable Unix-microsecond instant. |
| `mean-solar` | Local mean solar time for finite longitude; unavailable for invalid coordinates. |
| `modified-julian` | Modified Julian Date derived directly from the representable Unix-microsecond instant. |
| `chinese-time` | Exact traditional double-hour partition of local civil time; no geographic solar event required. |
| `roman-temporal` | Unequal temporal hours between computed sunrise/sunset boundaries. Requires finite latitude/longitude and a real crossing; polar absence is unavailable. |
| `japanese-temporal` | Edo late seasonal unequal-hour model using the documented 7°21′40″ dawn/dusk solar depression. Requires finite latitude/longitude and a real crossing; polar absence is unavailable. |
| `japanese-temporal-early` | Edo early seasonal unequal-hour model using the 0.833° sunrise/sunset boundary. Requires finite latitude/longitude and a real crossing; polar absence is unavailable. |
| `italian-hours` | Equal elapsed hours from computed sunset. Requires finite latitude/longitude and a valid sunset; otherwise unavailable. |
| `babylonian-hours` | Renaissance European gnomonic convention called “Babylonian hours”: 24 equal hours counted from computed sunrise. This is not ancient Mesopotamian timekeeping. |
| `babylonian-ancient` | Ancient Mesopotamian astronomical time: 12 fixed bēru per sunset-to-sunset day, 30 UŠ per bēru (4 SI minutes per UŠ). Requires a valid computed sunset as the daily origin. |
| `indian-ghati` | Sixty ghaṭī per mean day counted from computed sunrise; the resource displays ghaṭī/pala units explicitly. Requires finite latitude/longitude and a valid sunrise. |
| `chinese-ke` | Exact hundred-kè partition of the civil day; no geographic solar event required. The generic mode does not pretend one dynasty's finer subdivision applied universally. |
| `nuremberg-solar` | Location-aware Nuremberg-style sunrise/sunset reconstruction retained separately from Nürnberg's documented civic Wendetag system. |
| `nuremberg-hours` | Historical Nürnberg Great Clock reconstruction: equal hours with separate day/night counts and fixed civic Wendetage at Nürnberg; user coordinates are not used. |

## Historical scope and continuation rules

Historical names do not imply universal historical reconstruction. Where a system varied by locality, observation or era, Calendar documents the convention it computes and distinguishes that convention from historical universality.

Examples:

- modern Badíʿ years use a Tehran-referenced astronomical March equinox and sunset boundary;
- years before 172 B.E. retain the historical Western 21-March civil convention;
- Italian hours are the explicitly historical equal-hour system measured from computed sunset;
- “Babylonian hours” refers to the later European sunrise-origin convention; ancient Babylonian bēru/UŠ is a separate mode;
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
| General calendrical algorithms, epochs and cross-calendar reference practice | Edward M. Reingold and Nachum Dershowitz, *Calendrical Calculations: The Ultimate Edition*, 4th ed., Cambridge University Press, 2018, DOI [10.1017/9781107415058](https://doi.org/10.1017/9781107415058) |
| ICU-backed calendar variants and locale data | [Unicode TR35 / LDML](https://unicode.org/reports/tr35/) and [ICU](https://icu.unicode.org/) calendar implementations |
| Solar equation-of-time/declination and sunrise/sunset conventions | NOAA Global Monitoring Laboratory, [Solar Calculation Details](https://gml.noaa.gov/grad/solcalc/calcdetails.html), using a Meeus-derived model and the conventional 0.833° sunrise/sunset assumption |
| Equinox calculation | Jean Meeus, *Astronomical Algorithms*, 2nd ed., Willmann-Bell, 1998 |
| ΔT conversion used by the modern Badíʿ calculation | Fred Espenak and Jean Meeus, NASA GSFC, [Polynomial Expressions for Delta T](https://eclipse.gsfc.nasa.gov/LEcat5/deltatpoly.html) |
| Modern Badíʿ Naw-Rúz reference location and equinox rule | Universal House of Justice, [message dated 10 July 2014](https://www.bahai.org/library/authoritative-texts/the-universal-house-of-justice/messages/20140710_001/1) on implementation of the Badíʿ calendar |
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
