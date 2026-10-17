<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Ancient Egyptian chronology, calendars and timekeeping

## Status and ownership

This document is Calendar's specialist historical authority for Egyptian date- and time-reckoning. It catalogues the principal systems attested or seriously reconstructed for ancient Egypt, identifies disagreements in modern scholarship, and states what Calendar may and may not claim when admitting a future provider.

It is deliberately broader than a specification for one clock. Ancient Egypt did not possess one timeless, universal "Egyptian calendar" paired with one timeless, universal "Egyptian clock". The evidence spans more than three millennia, multiple political regimes, temple and administrative practices, observational astronomy, mechanical timekeeping, and later Greek and Roman systems used inside Egypt.

`MODELS.md` remains authoritative for Calendar's general evidence hierarchy, failure semantics and admission rules. This document owns the Egyptian historical detail.

Documentation is not implementation. A system described here is not selectable until its arithmetic/astronomy, shared registry identity, formatter, scheduling, tests and presentation contracts have been implemented and released.

## Evidence labels

Every historical claim or eventual implementation rule must be classifiable as one of these categories.

### Attested

Directly supported by surviving Egyptian documents, instruments, inscriptions or other primary evidence, normally interpreted through established specialist scholarship.

### Strong reconstruction

Not preserved as a complete algorithm or notation, but supported well enough by multiple sources that a deterministic computational model can be defended. The UI must still identify it as a reconstruction where that fact matters.

### Contested reconstruction

A serious scholarly proposal for which competing interpretations remain active. Calendar must not silently promote one interpretation to "the Egyptian system".

### Foreign system used in Egypt

A non-Egyptian calendar or clock convention demonstrably used by communities, administrations, rulers, astronomers or institutions in Egypt. Such a system belongs in the historical catalogue but must not be relabelled as an indigenous Egyptian invention.

### Modern precision extension

A modern representation used to preserve or expose the precision of a canonical timestamp when the historical notation is coarser. It must never be assigned a fabricated ancient unit name.

## Catalogue at a glance

### Date and year systems

| System | Approximate historical scope | Evidence state | Calendar treatment |
| --- | --- | --- | --- |
| Early lunar/lunistellar reckoning | Predynastic/Protodynastic is likely; exact regulation disputed | Contested reconstruction built on strong evidence for early lunar reckoning | Document only until a defensible algorithm and intercalation rule can be stated |
| Egyptian civil calendar | At least Old Kingdom; probably earlier; continues through antiquity | Attested | Structure is deterministic: 365-day wandering year, 12 × 30 days + 5 epagomenal days |
| Early event-named regnal years | Dynasty 1 into Old Kingdom | Attested | Historical year-label layer, not a free-running era |
| Census/count year naming | Late Dynasty 2 through Old Kingdom | Attested, count frequency debated | Historical year-label layer; no simple arithmetic conversion without a ruler-specific chronology |
| Numbered regnal years | First Intermediate Period/Middle Kingdom onward, with period-specific year-boundary rules | Attested | Requires ruler/accession/coregency data; not interchangeable with a continuous era |
| Lunar date-keeping and lunar festivals within civil dates | Attested from the third millennium BCE onward | Attested; existence of an independent parallel lunar calendar after the civil calendar is disputed | Lunar phase/day model is admissible only with an explicit era and observational/schematic policy |
| Papyrus Carlsberg 9 lunar scheme | Ptolemaic/Roman evidence, cycles cited from 19–144 CE | Attested schematic cycle | Possible separate late-period computational provider; not to be projected backwards |
| Ptolemaic Macedonian calendar and double dating | Ptolemaic Egypt | Foreign system used in Egypt | Historical foreign calendar; must remain labelled Macedonian/Ptolemaic |
| Canopus leap-year reform | Decreed 238 BCE | Attested decree; extent/duration of actual adoption uncertain | Do not treat as universally implemented Egyptian civil time without a bounded historical rule |
| Nabonassar astronomical era using 365-day Egyptian years | Hellenistic astronomical chronology, especially Ptolemaic scholarship | Attested as astronomical chronological convention | Existing `egyptian-nabonassar` provider; not the normal pharaonic regnal-year system |
| Alexandrian/Augustan reformed Egyptian calendar | Introduced under Augustus, c. 26 BCE | Attested | 365 days with a sixth epagomenal day every fourth year; ancestor of the Coptic calendar |
| Coptic/Anno Martyrum calendar | Late antique and later Christian Egypt | Attested continuation | Already represented separately by Calendar's Coptic provider; year era is not pharaonic |
| Babylonian/Aramaic, Jewish-adapted, Julian/Roman and other imported calendars | Different foreign-rule and community contexts | Foreign systems used in Egypt | Catalogue for provenance; never generic "Egyptian" modes |

### Time-of-day systems and instruments

| System / method | Approximate historical scope | Evidence state | Calendar treatment |
| --- | --- | --- | --- |
| Twelve night divisions in early religious/stellar material | Old/Middle Kingdom roots | Attested in broad structure; detailed computational interpretation varies | Historical background, not by itself a complete clock algorithm |
| Middle Kingdom diagonal star tables | Principally early Middle Kingdom | Attested tables; their exact use as "clocks" is reconstructed and terminology is debated | Do not implement a generic decanal clock merely from the table layout |
| Solar shadow clocks | New Kingdom evidence and later examples | Attested | Candidate evidence for seasonal daylight-hour reconstruction |
| Sundials, including later sloping/concave forms | New Kingdom onward, especially later periods | Attested | Instrument family; the hour convention must be identified per source/period |
| Outflow water clocks | New Kingdom onward | Attested | Strong basis for a New Kingdom seasonal night-hour reconstruction |
| Stellar/decanal night reckoning | Middle/New Kingdom evidence | Attested in broad form; detailed boundary and observation rules vary | Separate future model from water-clock civil night |
| Ramesside star tables | Ramesside period | Attested tables; interpretation as hour clocks remains contested | Research-only until an admission-grade model exists |
| Merkhet / meridian-transit observations | Later pharaonic evidence; use continues into later periods | Attested instrument and astronomical practice | Instrument/method, not automatically a distinct public clock mode |
| Seasonal day/night hours in Ptolemaic and Roman Egypt | Hellenistic/Roman period | Attested | Preserve as seasonal-hour practice; distinguish Egyptian ancestry from Greek formalisation |
| Equinoctial/equal hours in astronomical/scientific use | Hellenistic/Roman Egypt | Attested in Greek scientific tradition | Foreign/Greco-Egyptian scientific convention, not an indigenous pharaonic minute/second system |
| Hellenistic mechanical/inflow water clocks | Ptolemaic/Roman Egypt | Attested Greco-Egyptian technology | Historical late-period technology; do not project into New Kingdom reconstruction |

## 1. The Egyptian civil calendar

### 1.1 Structure

The pharaonic civil calendar is the clearest deterministic Egyptian calendar system.

A normal civil year contains exactly 365 days:

```text
3 seasons
× 4 months per season
× 30 days per month
= 360 ordinary days
+ 5 epagomenal days
= 365 days
```

The seasons are conventionally transliterated:

- `Ꜣḫt` / Akhet — inundation;
- `Prt` / Peret — emergence/growing season;
- `Šmw` / Shemu — harvest/dry season.

Each season contains four numbered 30-day months. Official dates can therefore be expressed as, for example, "day 15, month 3 of Akhet". The five epagomenal days stand outside the twelve regular months.

The civil year has no ordinary leap-day rule. Relative to the tropical/Julian year it therefore drifts by roughly one day every four years. The familiar approximately 1,460-Julian-year Sothic recurrence follows from this wandering structure; it must not be mistaken for proof that Egyptians maintained a separate universally operative "Sothic calendar".

### 1.2 Ten-day groups

The 30-day civil month was organised into three groups of ten days. Modern scholarship often calls these "decads" or ten-day weeks. Calendar may expose that structure as calendrical metadata, but should not automatically equate it with a modern seven-day week contract such as weekday/weekend semantics.

### 1.3 Day boundary

Most modern Egyptological summaries place the beginning of the Egyptian civil day at dawn, before sunrise. That is also compatible with the lunar rule in which a new lunar day could begin at dawn after the moon's morning invisibility had been established.

Calendar must distinguish "day begins at dawn" from the separate question of exactly which solar altitude or observational criterion should be used in software. An astronomical altitude chosen for determinism is a reconstruction unless a specific historical source defines it.

### 1.4 Month numbering and names

The historically safest canonical civil coordinate is **season + month number + day**, because named months vary by period and their interpretation is itself part of an Egyptological debate.

Later Egyptian, Greek and Coptic traditions preserve names associated with the twelve month positions. Representative correspondences include the following; spellings vary across transliteration systems and periods.

| Civil position | Representative Egyptian name/form | Later Greek/Coptic form |
| --- | --- | --- |
| I Akhet | Djehuty / related forms | Thoth / Thout |
| II Akhet | Pa-en-Ipet / related forms | Phaophi / Paope |
| III Akhet | Hathor | Athyr / Hathor |
| IV Akhet | Ka-her-ka / related forms | Choiak / Koiak |
| I Peret | Ta-abet / related forms | Tybi / Tobi |
| II Peret | Pa-en-Mekher / related forms | Mechir / Meshir |
| III Peret | Pa-en-Amenhotep / related forms | Phamenoth / Paremhotep |
| IV Peret | Pa-en-Renenutet / related forms | Pharmouthi / Parmouti |
| I Shemu | Pa-en-Khonsu / related forms | Pachon / Pashons |
| II Shemu | Pa-en-Int / related forms | Payni / Paoni |
| III Shemu | Ipip / related forms | Epiphi / Epip |
| IV Shemu | Mesut-Ra / related forms | Mesore / Mesori |

These names must not be presented as a single immutable pharaonic list. The Ebers calendar, Senenmut material, Deir el-Medina sources, Ramesseum, Edfu and later Greek forms preserve related but non-identical name sets. Scholars disagree about whether some earlier name sets are specifically lunar-month names or changing civil-month names.

### 1.5 What the current `egyptian-nabonassar` provider actually means

Calendar's existing `egyptian-nabonassar` provider uses the **Egyptian civil month/day structure** but supplies a continuous year era anchored to the Nabonassar epoch:

```text
1 Thoth, year 1 = JDN 1448638
                    ≈ 26 February 747 BCE (proleptic Julian convention)
```

That makes conversion deterministic, but the year number is an **astronomical/chronological era**, not the normal way a pharaonic Egyptian official dated a document in, for example, the reign of Hatshepsut or Ramesses II.

The provider label and documentation must continue to expose the Nabonassar-era qualification. A generic label such as "Ancient Egyptian calendar" would hide a material historical choice.

## 2. Regnal dating and year labels

Egyptian civil month/day coordinates do not by themselves define the historical year label. For much of pharaonic history, years were tied to the reigning king.

### 2.1 Dynasty 1 and early event-named years

Early records can identify years by important events, such as cultic, building or ceremonial occurrences. This is closer to a named-year chronicle than to a continuously numbered era.

### 2.2 Count years in the Old Kingdom

By late Dynasty 2 and into the Old Kingdom, years could be identified by a national count/census, with expressions equivalent to "year of the Nth count" and "year after the Nth count".

The old assumption that the count was mechanically biennial in every reign is no longer safe. Proposed patterns include changing frequency, annualisation and other administrative triggers. Calendar must therefore not derive absolute regnal years simply by doubling a count number.

### 2.3 Numbered regnal years

From the First Intermediate Period/Middle Kingdom onward, numbered regnal years become normal, but even here the year boundary is period-sensitive.

Modern summaries indicate:

- in the Middle Kingdom, regnal years were associated with the civil New Year boundary, with accession-year handling not completely certain in all cases;
- in the New Kingdom, the regnal count could run from accession/coronation anniversary to the next anniversary, still using civil-calendar months and days within the regnal year.

A historically meaningful regnal formatter therefore needs ruler identity, accession chronology, coregency policy and period-specific year-boundary rules. It is not a trivial alternative year number on the existing Nabonassar provider.

### 2.4 Historical notation

A date such as the Akhenaten boundary-stela example can be represented structurally as:

```text
Regnal year 6, IV Peret, day 13
```

The ruler is part of the date's historical context. Converting that to an absolute proleptic Gregorian/Julian date requires a chosen modern chronology, and early Egyptian absolute chronology remains uncertain at many points.

For that reason, a future `egyptian-regnal` presentation should expose chronology provenance rather than pretending every pharaonic regnal date maps to one undisputed BCE day.

## 3. Lunar and lunistellar date-keeping

### 3.1 Early lunar reckoning

Recent specialist surveys agree that some form of lunar or lunistellar reckoning probably predates the civil calendar, but the exact early algorithm is not known.

A major historical proposal, associated especially with Richard Parker, reconstructed an "old" lunistellar calendar in which the new year was related to the new moon following the heliacal rising of Sirius/Sopdet. More recent scholarship has challenged whether such a sophisticated Sirius-regulated system existed that early, proposing simpler lunar counting associated with the inundation instead.

Calendar must therefore record the existence of the debate rather than encode Parker's model as settled fact.

### 3.2 Lunar date-keeping after the civil calendar

Lunar practice unquestionably remained important in cult and festival life after the civil calendar existed. What is disputed is whether this should always be called a **separate parallel lunar calendar**.

Recent scholarship emphasises that many lunar feasts were recorded as lunar days falling within civil-calendar dates. This can be understood as lunar date-keeping operating alongside the civil calendar without requiring a wholly independent year/month numbering system.

The project should therefore use the neutral phrase **lunar date-keeping** unless a particular provider explicitly implements a scholarly model of an independent lunar calendar.

### 3.3 Lunar month and day boundary

The Egyptian lunar month was observational. A lunation could contain 29 or 30 days.

The best-supported later rule is that lunar day 1 begins when the old waning crescent is no longer visible in the morning sky before sunrise. If the crescent is still visible on lunar day 29, the current month continues to a 30th day; if it is no longer visible, the new lunation begins with the new day at dawn.

This is not the same rule as a calendar that begins the month with the first evening visibility of the waxing crescent.

### 3.4 Named lunar days

Individual lunar days had names, some tied to lunar phases, some ordinal/numerical and some cultic. Complete surviving lists of all 30 lunar-day names belong to the Ptolemaic/Roman evidence, notably Edfu; earlier periods attest only subsets and variants.

A future lunar provider should therefore version its vocabulary by evidence period. It must not back-project a late Edfu list unchanged into the Old or Middle Kingdom.

### 3.5 The 25-year Papyrus Carlsberg 9 scheme

Papyrus Carlsberg 9 preserves a late schematic relation between the Egyptian 365-day civil calendar and lunar months. Twenty-five Egyptian civil years contain 9,125 days, very close to 309 mean lunations. The preserved scheme includes nine "great" years with thirteen lunar months within a 25-year cycle.

The scheme is valuable because it is deterministic and historically attested, but its predictive convenience deliberately tolerates drift from actual lunar visibility. It belongs to Ptolemaic/Roman calendrical practice and must not be presented as the universal mechanism behind earlier Egyptian lunar observance.

## 4. Reform attempts and late Egyptian calendars

### 4.1 Canopus Decree, 238 BCE

Ptolemy III's Canopus Decree ordered an additional epagomenal day every fourth year, which would have arrested the civil calendar's seasonal drift.

The decree is attested; universal implementation is not. Scholarship has proposed limited or short-lived use, including possible adoption in Alexandria, but the traditional 365-day civil calendar continued.

Calendar should therefore model the Canopus reform only if a bounded provider can state where and when it applies. It must not rewrite the ordinary pharaonic civil provider as though the reform had universally succeeded.

### 4.2 Alexandrian/Augustan calendar

Under Augustus, Egypt received a successful four-year intercalation rule: three 365-day years followed by a year with a sixth epagomenal day. This produced the Alexandrian calendar, structurally close to the later Coptic calendar.

This is a distinct historical calendar and is suitable for a future provider if the project needs Roman-Egyptian civil dating separate from Coptic Anno Martyrum year numbering.

### 4.3 Coptic continuity

The Coptic calendar preserves the Alexandrian 12 × 30-day structure plus 5/6 epagomenal days, but its Christian era and later liturgical identity make it a separate calendar, not merely a UI alias for pharaonic Egypt.

Calendar already has a Coptic provider and should keep these identities distinct.

## 5. Foreign calendars actually used in Egypt

Ancient Egypt was repeatedly part of larger political and cultural systems. A complete historical catalogue must therefore record calendars used **in Egypt** without pretending they were all indigenous Egyptian calendars.

### 5.1 Babylonian/Aramaic calendar at Elephantine

Aramaic papyri from Persian-period Elephantine use a Babylonian calendar alongside Egyptian civil dating. This is important evidence for double calendation in Egypt, but it is not a native Egyptian calendar provider.

### 5.2 Macedonian calendar in Ptolemaic Egypt

Ptolemaic administration used Macedonian calendrical forms, and double-dated documents correlate Macedonian and Egyptian dates. The Macedonian system changed and interacted with Egyptian practice over time.

If Calendar eventually supports a historical Ptolemaic Macedonian provider, it should be named as such and not hidden under "Egyptian".

### 5.3 Julian/Roman and later provincial practice

Roman rule introduced or strengthened Julian calendrical use alongside the Alexandrian reformed Egyptian calendar. Late antique documents may therefore require explicit calendar provenance rather than one generic "Roman Egypt date".

## 6. Time of day: the attested Egyptian core

### 6.1 Twelve hours of day and twelve hours of night

Egyptian timekeeping developed separate twelvefold divisions of daylight and night. By the New Kingdom, the term `wnwt` is conventionally translated as "hour".

These are **seasonal/temporal hours**, not automatically equal SI hours. Daytime hours become longer when the daylight interval becomes longer; night hours change in the opposite direction.

The historical day and night systems should not be flattened into a modern 00–23 sequence unless a particular late scientific source explicitly uses such a convention.

### 6.2 No attested Egyptian minutes or seconds

There is no evidence for a formal pharaonic system equivalent to modern minutes and seconds. General words for shorter intervals or moments do not establish a numbered sexagesimal or decimal subdivision of the `wnwt`.

Calendar must never invent "Egyptian minutes" or "Egyptian seconds".

### 6.3 Continuous measurement is not the same as named subdivisions

A shadow moves continuously and water drains continuously. Egyptian instruments could therefore expose progress between hour boundaries without requiring a named minute unit.

This distinction is the basis for Calendar's allowed modern precision extension: an exact phase through a historical hour may be computed for file/event timestamps, but that phase must be visibly modern interpolation.

## 7. Stellar and decanal timekeeping

### 7.1 Decans

Decans are stars or asterisms used in Egyptian astronomical lists and tables. The familiar 36-decan organisation is related to ten-day divisions of the civil year and to stellar phenomena across the year.

The connection between decans and night-hour reckoning is historically important, but the surviving evidence does not give one universal star-clock algorithm valid across all periods.

### 7.2 Middle Kingdom diagonal star tables

Painted diagonal star tables survive primarily on Middle Kingdom coffins. Older literature commonly calls them "diagonal star clocks" or "diagonal star calendars". The Ancient Egyptian Astronomy Database deliberately prefers **diagonal star tables** because "clock" and "calendar" pre-judge their function.

Any Calendar implementation must respect that scholarly caution. The presence of a tabular 36-decan structure and twelve deep-night divisions is not, by itself, sufficient to reconstruct a single exact timestamp algorithm.

### 7.3 Astronomical night versus civil night

Decanal/star-based schemes may concern astronomical darkness, while surviving Eighteenth-Dynasty water-clock evidence has been interpreted as timing civil night from sunset to sunrise.

Those are materially different boundary models. Calendar must never silently use one as a substitute for the other.

### 7.4 Ramesside star tables

Ramesside star tables constitute a separate body of evidence. Their exact observational method and even their interpretation as hour clocks have been disputed. Serious proposals include stellar transit/timekeeping interpretations and non-clock cosmological or lunar-house interpretations.

Therefore a public `egyptian-ramesside-star` clock is **not admission-ready**. It would require an explicit scholarly model, source corpus, epoch handling and validation against the tables themselves.

## 8. Solar timekeeping

### 8.1 Shadow clocks

New Kingdom shadow clocks use the Sun's shadow to indicate parts of the daylight period. Surviving designs and reconstructions differ; some depend on shadow length, others on orientation and instrument geometry.

They support the historical reality of seasonal daytime reckoning but do not prove that every Egyptian day-hour was generated by one universal sunrise/sunset formula.

### 8.2 Sundials

Later Egyptian sundials include horizontal, sloping, concave and portable forms. Some surviving examples include seasonal adjustments or day curves.

A sundial is an instrument family, not automatically a separate clock system. A provider must identify the exact hour convention encoded by the chosen class of dial.

### 8.3 Obelisks

Large monuments can cast useful shadows, but Calendar should avoid the popular oversimplification that every obelisk was primarily a public clock. Monument function must be established source by source.

## 9. Water clocks

### 9.1 New Kingdom evidence

The earliest preserved Egyptian water clocks belong to the New Kingdom. An inscription of the official Amenemhet under the early Eighteenth Dynasty describes a water-clock instrument; the best-known surviving example comes from the reign of Amenhotep III at Karnak.

### 9.2 Seasonal calibration

The Karnak outflow clock contains month-dependent scales. Water drains continuously and the falling level crosses hour marks. The differing scales are evidence that night-hour duration was adjusted seasonally.

### 9.3 Civil-night reconstruction

Cotterell, Dickson and Kamminga's reanalysis argues that New Kingdom outflow water clocks may have timed **civil night from sunset to sunrise**, in contrast to decanal astronomical-night schemes.

This makes a water-clock-centred New Kingdom civil temporal model one of the strongest candidates for a deterministic Egyptian clock provider, provided the UI says explicitly that it is a reconstruction.

### 9.4 Accuracy

A reconstructed clock may mathematically calculate exact boundaries to microsecond precision, but that is not historical instrument accuracy. Estimates that a surviving design could potentially achieve accuracy on the order of modern minutes must not be converted into claims of second-level pharaonic observation.

Canonical computer timestamps and reconstructed historical measurement accuracy are separate concepts.

## 10. Merkhets, transits and observing practice

The merkhet is an attested Egyptian astronomical/surveying instrument involving a plumb line and sighting/alignment. In later evidence it could be used with selected stars and a north-south meridian to establish night-time positions/hours.

Calendar should classify a merkhet as an **observational method/instrument**, not automatically as another time scale. If future scholarship justifies a specific meridian-transit clock convention, that convention may become a provider; the instrument name alone is not enough.

## 11. Ptolemaic and Roman Egypt: seasonal and equal hours

By the Hellenistic and Roman periods, Egypt contains both inherited Egyptian practice and Greek/Roman scientific conventions.

### 11.1 Seasonal hours

Greek practice in Ptolemaic Egypt clearly uses numbered seasonal hours of day and night. The conceptual ancestry is Egyptian, but Greek mathematical astronomy formalised seasonal hours geometrically.

This should be described as Greco-Egyptian/Hellenistic practice where appropriate, not projected unchanged into the Old Kingdom.

### 11.2 Equinoctial/equal hours

Greek astronomical and scientific work also used equal/equinoctial hours: 24 equal divisions of the full day. Such hours are important for scientific observations and later exact astronomy, but they do not establish Egyptian pharaonic minutes and seconds.

A future provider should identify whether it is presenting seasonal civil hours or equinoctial astronomical hours.

### 11.3 Mechanical clocks

Hellenistic Alexandria contributed substantially to more sophisticated water-clock technology, including inflow/mechanical designs associated with Greek engineering. These belong to the history of timekeeping **in Egypt**, but must remain historically distinct from New Kingdom pharaonic outflow clocks.

## 12. Dawn, sunrise, sunset and twilight

Boundary choice is one of the most sensitive modelling questions.

General Egyptological summaries often say the Egyptian day began at dawn. Specialist work on stellar and shadow-clock practice, however, allows transitional intervals around sunrise and sunset in which the relevant observation method changes or no numbered hour is straightforwardly assigned. Water-clock scholarship can instead support a sunset-to-sunrise civil night.

There is therefore no defensible universal formula such as:

```text
all Egyptian day-hours = sunrise-to-sunset / 12
all Egyptian night-hours = sunset-to-sunrise / 12
```

for every period and instrument.

A deterministic provider must state its boundary convention as part of its identity.

## 13. Proposed provider taxonomy

### 13.1 Currently implemented

#### `egyptian-nabonassar`

Status: **implemented**.

Meaning: wandering 365-day Egyptian civil structure with continuous Nabonassar-era year numbering. Useful and deterministic, but not generic pharaonic regnal dating.

### 13.2 Strong candidates for future implementation

#### New Kingdom civil temporal time

Possible stable identity after implementation review:

```text
egyptian-temporal-new-kingdom
```

Model: twelve seasonal day-hours plus twelve seasonal night-hours, with explicitly documented solar/day and water-clock/civil-night reconstruction boundaries.

This is the most defensible first Egyptian clock provider because the physical instrument evidence is comparatively strong.

#### Alexandrian reformed Egyptian calendar

Possible identity:

```text
egyptian-alexandrian
```

Model: Egyptian month structure with a sixth epagomenal day every fourth year under the Augustan reform. Year-numbering policy must be specified separately from Coptic Anno Martyrum.

### 13.3 Research-blocked candidates

#### Early lunar/lunistellar calendar

Blocked by disagreement over the existence and regulation of an independent parallel lunar calendar after the civil calendar, and by uncertainty over early intercalation/Sirius rules.

#### Late lunar schematic calendar

A bounded Ptolemaic/Roman provider based on Papyrus Carlsberg 9 may be possible, but it must be labelled as that late schematic cycle, not "the Egyptian lunar calendar".

#### Regnal dating

Requires a ruler chronology database, accession dates, coregencies, period-specific regnal-year boundary rules and explicit modern chronology provenance.

#### Decanal/diagonal-star time

Requires a source-period-specific stellar model and a defensible interpretation of table function. Not ready as a generic clock.

#### Ramesside stellar time

Blocked by competing interpretations of the surviving tables.

### 13.4 Not separate providers by themselves

The following are instruments or techniques, not automatically independent time scales:

- shadow clock;
- sundial;
- water clock;
- merkhet;
- obelisk shadow;
- a named astronomical table without an admitted interpretation.

Calendar modes represent **time coordinate systems**, not museum object categories.

## 14. Precision without invented historical units

### 14.1 Canonical timestamp remains authoritative

A filesystem timestamp, event time or stored instant remains in its canonical representation. Historical rendering never rewrites the underlying value and never controls sorting or comparison.

### 14.2 Exact phase through a reconstructed hour

For a canonical instant `T` inside a reconstructed temporal hour bounded by exact computational instants `A` and `B`, Calendar may calculate:

```text
phase = (T - A) / (B - A)
```

with `0 <= phase < 1`.

The fraction is a **modern precision extension**. It corresponds conceptually to continuous progress of a shadow or water level, but it is not an Egyptian minute or second.

### 14.3 Presentation contexts

Ordinary historical display should prefer native granularity:

```text
5th hour of the night
```

Precision-sensitive display may append a clearly modern interpolation:

```text
5th hour of the night · 63.7% elapsed
```

A detailed or forensic surface should retain access to the exact canonical/conventional timestamp.

### 14.4 Egyptian fractions

Egyptian mathematics used unit fractions extensively. That does **not** establish an attested timestamp notation such as "1/2 + 1/8 of the fifth hour". Such a display may only be added as an explicitly modern stylistic reconstruction after a separate evidence review.

## 15. Naming and UI rules

Selector labels must expose the historical model rather than use broad names that imply universal coverage.

Good examples:

```text
Egyptian civil — Nabonassar astronomical era
Ancient Egyptian temporal time — New Kingdom civil reconstruction
Egyptian Alexandrian calendar — Augustan reform
Egyptian lunar scheme — Papyrus Carlsberg 9 (late-period reconstruction)
```

Bad examples when more precision is known:

```text
Ancient Egyptian calendar
Egyptian time
Pharaoh clock
```

The selected date system and selected clock system remain independent. A user may deliberately combine models from different periods, but System Settings must not describe the resulting pair as one historically attested timestamp standard.

## 16. Validation requirements

Any Egyptian provider must prove at least:

- its exact historical scope and evidence class;
- epoch, year-label and day-boundary rules where relevant;
- locality/astronomical dependencies;
- explicit distinction between attested rule and project reconstruction;
- no silent projection of late names/rules into earlier periods;
- no invented minutes, seconds, leap rules or year eras;
- exact canonical timestamp preservation;
- unavailable/ambiguous states rather than plausible fabrication;
- cross-checks against independent scholarly calculations or surviving source examples;
- help/provenance that lets a specialist identify which historical model is being computed.

A provider is not admitted merely because it produces aesthetically convincing output.

## 17. Open research questions that must remain open in the UI

The project currently treats the following as unresolved rather than choosing silently:

- how the earliest Predynastic/Protodynastic lunar year was regulated;
- whether Parker's Sirius-regulated "old lunar calendar" existed in the proposed form;
- whether a fully independent lunar calendar continued in parallel with the civil calendar throughout pharaonic history, versus lunar date-keeping embedded in civil dates;
- the historical status and interpretation of the Ebers calendar;
- the exact original operational interpretation of Middle Kingdom diagonal star tables;
- the detailed observational algorithm behind Ramesside star tables;
- whether particular twilight intervals should be numbered, transitional or unavailable in a strict stellar/shadow reconstruction;
- the frequency of Old Kingdom census/count years in individual reigns;
- precise regnal-year allocation around accessions and coregencies for many rulers;
- the extent and duration of practical adoption of the Canopus reform.

A future discovery or stronger scholarly consensus may change these classifications. The documentation should be updated before the implementation is broadened.

## 18. Evidence and references

The project uses references for specific claims rather than treating any single work as universal authority.

### General Egyptian chronology and timekeeping

- Janice Kamrin, **"Telling Time in Ancient Egypt"**, The Metropolitan Museum of Art, 2017. Civil calendar structure, ten-day groups, dawn day boundary, lunar practice, regnal dating overview, 12 day/night hours and lack of attested minutes/seconds. <https://www.metmuseum.org/essays/telling-time-in-ancient-egypt>
- UCL Digital Egypt, **"Calendar"**. Egyptian/Alexandrian/Coptic month structure and later month-name correspondences. <https://www.ucl.ac.uk/museums-static/digitalegypt/chronology/calendar.html>
- UCL Digital Egypt, **"Measuring time in Egypt"**. Water clocks, sundials and shadow clocks. <https://www.ucl.ac.uk/museums-static/digitalegypt/astro/clocks.html>
- Sacha Stern, **Calendars in Antiquity: Empires, States, and Societies**, Oxford University Press, 2012, chapter "The Egyptian Calendar". Civil, lunar, Sothic/reform and Ptolemaic calendar problems.
- Marshall Clagett, **Ancient Egyptian Science, Volume II: Calendars, Clocks, and Astronomy**, American Philosophical Society, 1995. Major source collection and historical discussion.
- Juan Antonio Belmonte and José Lull, **Astronomy of Ancient Egypt: A Cultural Perspective**, Springer, 2023. Recent comprehensive treatment; important for disputed lunar/calendar interpretations.

### Lunar date-keeping

- Rita Gautschy, **"Lunar Calendar"**, UCLA Encyclopedia of Egyptology, 2024, DOI 10.5070/G9.3898. Current specialist survey of early lunar reckoning, the debate over later independent lunar calendars, lunar-day observation, month names and Papyrus Carlsberg 9. <https://escholarship.org/uc/item/4zq1z7q1>
- Leo Depuydt, **Civil Calendar and Lunar Calendar in Ancient Egypt**, Peeters, 1997. Major argument for a continuing lunar calendar; used with later critiques.
- Richard A. Parker, **The Calendars of Ancient Egypt**, SAOC 26, Oriental Institute, 1950. Foundational reconstruction and source collection; historically indispensable but several conclusions are now disputed or superseded. <https://isac.uchicago.edu/sites/default/files/uploads/shared/docs/saoc26.pdf>

### Stellar astronomy and clock evidence

- Sarah Symons, **"Ancient Egyptian astronomy: timekeeping and cosmography in the New Kingdom"**, doctoral thesis, University of Leicester. Detailed treatment of diagonal star tables/clocks, Ramesside tables, shadow clocks and sundials. <https://figshare.le.ac.uk/articles/thesis/Ancient_Egyptian_astronomy_timekeeping_and_cosmography_in_the_new_kingdom/10097984>
- Ancient Egyptian Astronomy Database, McMaster University. Current scholarly catalogue of decans, diagonal star tables, astronomical representations, sundials and water clocks; notably prefers the neutral term "diagonal star tables". <https://aea.mcmaster.ca/>
- B. Cotterell, F. P. Dickson and J. Kamminga, **"Ancient Egyptian water-clocks: A reappraisal"**, *Journal of Archaeological Science* 13(1), 1986, 31–50, DOI 10.1016/0305-4403(86)90025-7. Seasonal night-hours and civil-night versus astronomical-night distinction.

### Hellenistic/Roman Egypt

- Kassandra J. Miller, **Time and Ancient Medicine: How Sundials and Water Clocks Changed Medical Science**, Oxford University Press, 2023. Seasonal and equinoctial hours in Greco-Roman practice.
- Institute for the Study of the Ancient World, **Time and Cosmos in Greco-Roman Antiquity** materials. Greek adoption/formalisation of seasonal hours and later instrument culture. <https://isaw.nyu.edu/exhibitions/time-cosmos/>
- Oxford Handbook material on calendrical time and Egyptian calendars, including the civil calendar, lunar/cultic practice, Canopus reform and Augustan Alexandrian reform.

## Admission rule

The correct goal is not to make Egypt look neat. The correct goal is to make every displayed date or time traceable to a specific historical convention, source class and reconstruction policy.

When the evidence supports several systems, Calendar should expose several clearly named systems. When the evidence is contested, the disagreement belongs in the contract. When no deterministic interpretation can yet be defended, unsupported is more accurate than plausible-looking output.
