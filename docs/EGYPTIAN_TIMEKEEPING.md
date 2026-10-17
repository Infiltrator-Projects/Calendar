<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Ancient Egyptian timekeeping model

## Status and ownership

This document is Calendar's historical-model admission specification for a future Ancient Egyptian clock mode. It is **not** evidence that the clock mode is already implemented or selectable.

`MODELS.md` remains the authority for Calendar's general evidence hierarchy, admission rules and failure semantics. This document records the narrower historical evidence, reconstruction choices and presentation boundaries required before an Egyptian time provider may be admitted.

The intended scope is deliberately narrow: a **New Kingdom-centred reconstruction of Egyptian temporal timekeeping**, informed by surviving stellar tables, shadow/sundial evidence and water clocks. It must not be described as a single universal clock used unchanged throughout all periods and localities of ancient Egyptian history.

## Why the scope must be explicit

Ancient Egyptian timekeeping is not one homogeneous system that can be reduced honestly to a modern `HH:MM:SS` display.

The surviving evidence spans many centuries and several methods:

- stellar and decanal schemes for the night;
- shadow clocks and sundials for daylight;
- outflow water clocks for night-time measurement;
- religious and astronomical texts in which night is divided into twelve named or numbered hours;
- later developments and external influences that must not be projected backwards onto earlier periods.

A model that merely divides every civil day into twenty-four equal modern hours and gives those values Egyptian labels would be historically false.

## Evidence classes

Every behaviour in the implementation must be identifiable as one of the following.

### Attested

Directly supported by surviving Egyptian evidence or strong modern Egyptological/horological scholarship.

### Reconstruction

A deterministic computational choice needed to turn incomplete historical evidence into software. A reconstruction must be documented as such and must not be presented as an attested Egyptian rule.

### Modern precision extension

A representation used only so modern software can preserve and expose the precision of canonical timestamps. A modern precision extension must never be given a fabricated Egyptian unit name.

These classes must remain distinct in source comments, tests, UI help and documentation.

## Attested core

### Twelve day-hours and twelve night-hours

By the New Kingdom, Egyptian evidence clearly supports twelve `wnwt` for the day and twelve `wnwt` for the night. The day-hours and night-hours were conceived separately rather than as one abstract sequence of twenty-four equal modern hours.

The word `wnwt` is conventionally translated as "hour". Earlier stellar material demonstrates a twelvefold division of the night; by the New Kingdom the term is explicitly associated with those hour divisions.

### Seasonal or unequal hours

The Egyptian `wnwt` was not a fixed SI-duration hour. Day-hours lengthened when daylight lengthened and shortened when daylight shortened; night-hours changed in the opposite direction.

Surviving sundial/shadow-clock evidence and water-clock calibration are consistent with seasonal or temporal hours. The interior scales of the Karnak water clock attributed to the reign of Amenhotep III vary by month, demonstrating that night-hour measurement was adjusted for seasonal night length.

### Day and night used different observational methods

Daytime was measured from the Sun and shadows. Night-time could be measured using stars/decanal schemes and, from the New Kingdom, water clocks.

The methods must not be collapsed into one fictional mechanical clock. A software model may unify them behind one presentation interface, but the documentation must preserve their historical distinction.

### Water clocks measured continuously between hour marks

Egyptian outflow water clocks did not physically jump from one hour to the next. Water drained continuously, and the remaining level was compared with calibrated marks. This is important for modern reconstruction: surviving instruments support continuous *observable progress* through an hour even though the surviving evidence does not establish a formal minute/second notation.

### No attested Egyptian minute or second system

There is no evidence that ancient Egyptians tracked minutes or seconds as formal subdivisions equivalent to the modern units. General expressions for intervals shorter than an hour existed, but that is not evidence for a systematic sexagesimal or decimal subdivision of the `wnwt`.

Calendar must therefore never invent labels such as "Egyptian minute" or "Egyptian second".

## Dawn, sunrise, sunset and twilight

This is a model-sensitive area and must remain explicit.

General Egyptological summaries commonly describe the Egyptian day as beginning at dawn, before sunrise. More specialised work on New Kingdom astronomical timekeeping emphasises that day-hours and night-hours were tied to the periods in which solar shadows and stars could actually be observed, leaving transitional periods around sunrise and sunset that were not necessarily assigned an hour in the same way.

Water-clock scholarship introduces a second distinction: decanal star clocks appear to have represented astronomical night, while analysis of Eighteenth-Dynasty water clocks supports a civil-night interpretation from sunset to sunrise.

Therefore there is **no defensible basis for silently claiming one exact dawn/sunrise/sunset boundary as universal Egyptian practice**.

The implementation must state which reconstruction it computes.

## Proposed production reconstruction

The initial provider should model **New Kingdom civil temporal time**, with a water-clock-centred night model and a solar/shadow-centred day model. This provides a reproducible clock while staying close to surviving instruments.

A future decanal stellar provider may be added separately if Calendar acquires an era-specific, evidence-backed stellar model. It must not be approximated by relabelling the civil temporal provider.

### Geographic requirement

The clock is location-dependent. It requires finite latitude and longitude. No default coordinate such as Greenwich or `0,0` may be substituted when location is absent.

### Day-hours

For the initial reconstruction, the interval between the selected solar day boundaries is divided into twelve equal **temporal day-hours**. Their SI duration therefore changes with season and latitude.

The exact solar boundary convention must be named in code and tests. If the implementation uses computed sunrise and sunset as the practical boundaries, that choice is a **computational reconstruction**, not a claim that every Egyptian source defined the day in precisely those astronomical terms.

### Night-hours

For the initial reconstruction, the civil night between computed sunset and the following sunrise is divided into twelve equal **temporal night-hours**. This is the model most directly compatible with the interpretation of New Kingdom water clocks as civil-night instruments.

This choice must be documented as the water-clock/civil-night reconstruction. It must not be described as a faithful simulation of decanal star clocks, which may represent astronomical darkness differently.

### Transitional periods

Because scholarly reconstruction of stellar and shadow-clock practice allows periods around sunrise and sunset that were not themselves assigned conventional `wnwt`, Calendar must not erase that evidence.

The initial civil temporal provider may remain continuous by using the water-clock/sunrise-sunset reconstruction described above, but its user-facing description must say that this is a **civil temporal reconstruction** and that stellar/decanal practice treated twilight differently.

If a later historically stricter stellar mode is implemented, twilight may legitimately produce an explicit transition/unavailable state instead of a fabricated hour.

## Precision without invented units

### Canonical timestamp remains authoritative

A canonical timestamp is never converted destructively into Egyptian hour notation. Sorting, comparison, storage, filesystem metadata, event ordering and machine interchange continue to use the owning canonical timestamp.

### Exact within-hour phase

For an instant `T` inside an Egyptian temporal hour with exact computational boundaries `A` and `B`, Calendar may compute the dimensionless phase

```text
phase = (T - A) / (B - A)
```

with `0 <= phase < 1`.

The implementation should retain this as an exact integer/rational relationship for as long as practical and round only for presentation. This preserves all timestamp precision supplied by the caller.

### Historical meaning of the phase

The phase is **not an attested Egyptian named unit**. It is a modern numerical representation of something their instruments physically exposed: progress of a shadow or water level between hour boundaries.

That distinction is the core historical safeguard.

### Ordinary clock presentation

A normal human-facing clock should prefer the attested granularity, for example:

```text
5th hour of the night
```

or a localized equivalent.

A graphical progress indicator through the current `wnwt` is acceptable because it represents continuous instrument progress without inventing a historical subdivision name.

### Precision-sensitive presentation

Contexts such as file properties, forensic viewers or event details may need finer discrimination. They may append an explicitly modern interpolation, for example:

```text
5th hour of the night · 0.637 through the hour
```

or:

```text
5th hour of the night · 63.7% elapsed
```

The exact wording may be refined, but the UI must make clear that the fractional component is computational interpolation, not surviving Egyptian notation.

A conventional/canonical timestamp should remain available in detailed or forensic contexts whenever exact correlation matters.

### Egyptian fractions

Egyptian mathematics made extensive use of unit fractions, but the project currently has no evidence that formal fractional notation was routinely attached to clock hours as timestamp syntax.

Calendar must therefore **not** present an Egyptian-fraction rendering such as `1/2 + 1/8 of an hour` as historically attested time notation. Such a display could only be admitted later as an explicitly labelled modern stylistic reconstruction with its own evidence review.

## Numbering and wording

The provider should use ordinal hour numbering from 1 through 12 separately for day and night.

English examples should say "1st hour of the day", "5th hour of the night", and so on. `wnwt` may appear in scholarly detail/help text but should not be used to imply that a reconstructed English display reproduces original Egyptian written syntax.

The project must not fabricate hieroglyphic clock strings merely for visual effect.

## Relationship to the Egyptian civil calendar

The existing `egyptian-nabonassar` calendar provider and this proposed time provider are independent presentation dimensions.

The calendar provider is a deterministic Ptolemaic/Nabonassar-era arithmetic model of the wandering 365-day Egyptian civil calendar. The proposed time provider is a New Kingdom-centred reconstruction of temporal hours.

Selecting both may be a useful "Egyptian date + Egyptian time" presentation, but the combination spans historically different evidence and must not be described as one single attested timestamp standard used throughout ancient Egypt.

## Failure semantics

The provider must return unavailable rather than fabricate a plausible value when:

- required geographic coordinates are absent or invalid;
- the selected astronomical boundary cannot be established under the model;
- a future stellar/decanal variant lacks the required era-specific data;
- numerical range or timestamp conversion fails.

If the production civil model is limited to latitudes where sunrise and sunset exist normally, polar-day/night cases are unavailable.

## Naming

A user-facing name should expose the reconstruction rather than imply universality. A suitable form is:

```text
Ancient Egyptian temporal time — New Kingdom civil reconstruction
```

A shorter UI label may be used where necessary, but help text must retain the historical scope.

If a stable provider identifier is required during implementation, `egyptian-temporal` is a reasonable provisional identifier. It must not enter the public registry until formatter, boundary scheduling, tests, documentation and System Settings presentation are complete.

## Validation requirements

Before admission, tests must cover at least:

- twelve day-hours and twelve night-hours under the chosen civil reconstruction;
- seasonal change in the SI duration of day and night `wnwt`;
- exact noon/midnight symmetry expectations where the model implies them;
- sunrise/sunset boundary transitions;
- timezone/DST interaction without changing the underlying astronomical instant;
- missing/invalid location;
- polar or otherwise undefined solar crossings;
- exact within-hour phase monotonicity;
- canonical timestamp preservation in precision contexts;
- absence of invented minute/second terminology;
- user-visible wording that labels the model as a reconstruction;
- separation from `roman-temporal`, `babylonian-ancient` and the existing Egyptian civil calendar provider.

Validation should include cross-checks against independently computed sunrise/sunset data and hand-worked seasonal examples for an Egyptian reference latitude, without turning one reference locality into a universal historical location.

## Evidence and references

The following sources support different parts of the model; none is treated as blanket authority for every implementation detail.

- Janice Kamrin, **"Telling Time in Ancient Egypt"**, The Metropolitan Museum of Art, 2017. General Egyptological overview: twelve day-hours and twelve night-hours; day beginning around dawn; decanal night reckoning; New Kingdom sundials, shadow clocks and water clocks; explicit statement that there is no evidence for tracked minutes or seconds. <https://www.metmuseum.org/essays/telling-time-in-ancient-egypt>
- Robert Cockcroft and Sarah Symons, **"Ancient Egyptians measured the first hour, and changed how we related to time"**, McMaster University / The Conversation, 2023. Specialist summary of `wnwt`, twelve day-hours and twelve night-hours, separate day/night systems, seasonal hour length, stellar/shadow methods and transitional periods around sunrise and sunset. <https://news.mcmaster.ca/analysis-ancient-egyptians-measured-the-first-hour-and-changed-how-we-related-to-time/>
- Sarah Symons, **"Ancient Egyptian astronomy: timekeeping and cosmography in the New Kingdom"**, doctoral thesis, University of Leicester. Detailed study of diagonal star clocks, Ramesside stellar timekeeping, shadow clocks and sundials. <https://figshare.le.ac.uk/articles/thesis/Ancient_Egyptian_astronomy_timekeeping_and_cosmography_in_the_new_kingdom/10097984>
- UCL Digital Egypt, **"Measuring time in Egypt"**. Surviving water clocks, continuous water-level measurement and sundial/shadow-clock forms. <https://www.ucl.ac.uk/museums-static/digitalegypt/astro/clocks.html>
- B. Cotterell, F. P. Dickson and J. Kamminga, **"Ancient Egyptian water-clocks: A reappraisal"**, *Journal of Archaeological Science* 13(1), 1986, pp. 31–50, DOI 10.1016/0305-4403(86)90025-7. Seasonal night-hours; distinction between decanal astronomical night and the civil-night interpretation of Eighteenth-Dynasty water clocks.
- Richard A. Parker, **The Calendars of Ancient Egypt**, Studies in Ancient Oriental Civilization 26, University of Chicago Press / Oriental Institute. Discussion and diagram of the Karnak water-clock calibration, including month-specific interior scales and hour transitions. <https://isac.uchicago.edu/sites/default/files/uploads/shared/docs/saoc26.pdf>
- Institute for the Study of Ancient Cultures, University of Chicago, **"Water Clocks in Ancient Egypt"**. Museum overview of Egyptian outflow/inflow clocks and their use in measuring elapsed time. <https://isac.uchicago.edu/sites/default/files/uploads/shared/docs/oimp36.pdf>

## Admission rule

Historical plausibility is not enough. The provider is ready only when the code can identify, for every visible element, whether it is attested, reconstructed or a modern precision extension.

If that distinction cannot be maintained, the correct result is to leave the clock unsupported rather than manufacture a more convenient history.
