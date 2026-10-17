<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Native Ancient Egyptian chronology and timekeeping

## Status and ownership

This document is Calendar's specialist historical authority for **native ancient Egyptian** date- and time-reckoning. Its scope is intentionally narrow: it records the Egyptian systems themselves, not every calendar or clock that later existed in Egypt under Greek, Roman, Christian or other traditions.

`MODELS.md` remains authoritative for Calendar's general evidence hierarchy, failure semantics and model-admission rules. This document owns the Egyptian historical detail.

Documentation is not implementation. A system listed here is not selectable until its arithmetic or astronomy, shared registry identity, formatter/converter, scheduling where required, tests and presentation contracts have been implemented and validated.

## Scope: the ten Egyptian families

For project purposes, the native Egyptian historical catalogue is organised into **five date systems/families** and **five time systems/families**.

### Native Egyptian date systems

1. **Early Egyptian lunar / lunistellar reckoning**
2. **Egyptian civil calendar**
3. **Egyptian regnal dating**
4. **Egyptian lunar date-keeping and festival reckoning**
5. **Late Egyptian schematic 25-year lunar cycle (Papyrus Carlsberg 9 tradition)**

### Native Egyptian time systems

1. **Decanal / stellar night timekeeping**
2. **Seasonal daylight hours (shadow-clock / sundial tradition)**
3. **Seasonal night hours (outflow water-clock tradition)**
4. **Ramesside stellar timekeeping**
5. **Merkhet / meridian-transit stellar timekeeping**

These are historical **families**, not a promise of ten independent selector entries. Some may ultimately share implementation machinery or prove better represented as variants of one provider. In particular, Ramesside stellar reckoning and merkhet/meridian observation are genuinely Egyptian practices but may prove to be methods or variants rather than fully independent clock systems. The historical catalogue keeps them distinct until the evidence and implementation model justify collapsing them.

## Explicitly outside this Egyptian catalogue

The following are deliberately **not** counted here merely because they were used in Egypt, descended from an Egyptian system, or used Egyptian month structure:

- Coptic calendar;
- Macedonian/Ptolemaic calendar;
- Julian or Roman calendars;
- Jewish, Babylonian, Aramaic or other imported calendars;
- Greek/Roman equal or equinoctial-hour systems;
- Alexandrian/Augustan reform as a separate later civil calendar;
- Nabonassar-era astronomical year numbering as though it were normal pharaonic year reckoning.

Those systems may exist elsewhere in Calendar under their own historically correct identities. They must not be duplicated or relabelled as generic "Ancient Egyptian" modes.

The existing `egyptian-nabonassar` provider remains a valid deterministic representation of the wandering Egyptian civil **month/day structure** with Nabonassar-era astronomical year numbering. It is not counted among the five native date systems above because that continuous era is not the ordinary pharaonic Egyptian year-label system.

## Evidence labels

Every rule eventually implemented from this document must be classified as one of the following.

### Attested

Directly supported by surviving Egyptian documents, instruments, inscriptions or astronomical material, interpreted through established specialist scholarship.

### Strong reconstruction

The historical evidence is incomplete, but the surviving material supports a reproducible computational reconstruction strongly enough to implement it if the reconstruction is identified as such.

### Contested reconstruction

A serious scholarly interpretation exists, but important alternatives remain. Calendar must not silently promote one interpretation to "the Egyptian system".

### Modern precision extension

A modern representation used only to preserve or expose the precision of a canonical timestamp where historical notation is coarser. A modern extension must never be given a fabricated Egyptian unit name.

# Part I — Native Egyptian date systems

## 1. Early Egyptian lunar / lunistellar reckoning

### Historical character

Some form of lunar reckoning almost certainly predates or accompanies the development of the fixed civil calendar. The exact early system is not recoverable as one complete algorithm from surviving evidence.

Older reconstructions, especially that associated with Richard A. Parker, proposed an "old" lunistellar calendar whose year was regulated relative to Sirius/Sopdet and the lunar cycle. More recent scholarship has challenged treating that reconstruction as settled history. The existence of early lunar reckoning is much firmer than the exact intercalation and New-Year algorithm.

### Project classification

**Contested reconstruction.**

Calendar must not expose a generic deterministic "Old Egyptian lunar calendar" until an implementation can name its chosen scholarly reconstruction, period, month-start rule, intercalation rule and validation evidence.

### What is safe to claim

- lunar observation was important very early in Egyptian chronology;
- lunar months naturally contained 29 or 30 days;
- later evidence shows Egyptian lunar days and lunar festivals remained culturally important;
- a precise universal early lunistellar algorithm is not securely attested.

## 2. Egyptian civil calendar

### Historical character

This is the clearest and most computationally deterministic indigenous Egyptian calendar.

The year contains:

```text
3 seasons
× 4 months per season
× 30 days per month
= 360 ordinary days
+ 5 epagomenal days
= 365 days
```

The three seasons are conventionally transliterated:

- `Ꜣḫt` / Akhet — inundation;
- `Prt` / Peret — emergence/growing season;
- `Šmw` / Shemu — harvest/dry season.

Each season contains four 30-day months. The five epagomenal days stand outside the twelve regular months.

The pharaonic civil calendar has no ordinary leap-day rule, so it wanders through the solar seasons relative to the tropical year.

### Ten-day groupings

The 30-day month was structured into three ten-day groups. These can be exposed as historically meaningful calendrical metadata, but they must not be made equivalent to the modern seven-day weekday/weekend cycle.

### Date form

For early and pharaonic presentation, **season + month number + day** is safer than pretending one immutable list of later month names applied throughout Egyptian history.

A historically conservative display may therefore resemble:

```text
III Akhet, day 15
```

with the year supplied by the selected Egyptian year-reckoning model, most authentically regnal dating when the historical context requires it.

### Day boundary

Egyptological summaries commonly associate the Egyptian civil day with dawn. Software must distinguish that historical statement from a precise modern solar-altitude rule. Any exact astronomical threshold chosen to compute "dawn" is a reconstruction unless tied to a specific historical source.

### Project classification

**Attested core, deterministic arithmetic.**

This is the primary native Egyptian calendar structure.

## 3. Egyptian regnal dating

### Historical character

For much of pharaonic history Egyptians did not identify years using one indefinitely increasing national era. Dates were associated with the reigning king.

Regnal dating changed over Egyptian history and therefore represents a family of year-label conventions layered on Egyptian civil month/day coordinates.

### Early event-named years

In the earliest dynastic material, years could be distinguished by important events rather than by a simple continuously numbered regnal count.

### Census/count years

Old Kingdom material uses labels associated with royal counts or censuses, including "year of the Nth count" and "year after the Nth count". The older assumption that every count was rigidly biennial is not safe enough for a general conversion algorithm.

### Numbered regnal years

Later periods increasingly use numbered years of a ruler. The precise year boundary and accession treatment are period-sensitive, and coregencies create further chronological complications.

A meaningful computational regnal formatter therefore requires at least:

- ruler identity;
- a chosen accession chronology;
- accession/coregency policy;
- period-specific year-boundary rules;
- an explicit statement of uncertainty where absolute chronology is disputed.

### Project classification

**Attested historical notation; computational conversion is chronology-dependent.**

A future `egyptian-regnal` mode must never pretend that all pharaonic regnal dates possess one undisputed BCE/Gregorian equivalent.

## 4. Egyptian lunar date-keeping and festival reckoning

### Historical character

Lunar observation remained important after the civil calendar existed, especially in ritual and festival contexts. The strongest modern caution is terminological: surviving evidence does not always require a wholly independent parallel "lunar calendar" with its own autonomous year count. In many cases, lunar days and feasts can be understood as lunar reckoning operating **within or alongside civil dates**.

Accordingly, Calendar uses the neutral term **lunar date-keeping** unless a particular provider implements a specifically defined independent lunar-calendar reconstruction.

### Lunar month boundary

Later Egyptian evidence supports a month beginning when the old waning crescent was no longer visible in the morning sky before sunrise. This differs from traditions that begin a month with first evening visibility of the waxing crescent.

A lunation may therefore contain 29 or 30 days.

### Lunar-day names

Egyptian lunar days possessed names, but the surviving vocabulary varies by period and source. Complete late lists must not be projected unchanged into much earlier periods.

### Project classification

**Attested lunar practice; exact provider semantics must be period-specific.**

An observational provider must state its visibility model and locality assumptions. A schematic provider must identify the historical scheme it follows.

## 5. Late Egyptian schematic 25-year lunar cycle

### Historical character

Papyrus Carlsberg 9 preserves a late Egyptian schematic relation between lunar months and the 365-day Egyptian civil calendar.

The arithmetic basis is:

```text
25 Egyptian civil years = 9,125 days
309 mean lunations       ≈ 9,124.95 days
```

The preserved scheme includes nine "great" years containing thirteen lunar months within a 25-year cycle. Its purpose is computational predictability, and it knowingly tolerates divergence from actual lunar visibility over time.

This is historically important precisely because it is **schematic**, not because it proves that earlier Egyptian lunar practice used the same cycle.

### Project classification

**Attested late Egyptian schematic lunar system.**

If implemented, it must be labelled by period/source and must not be projected backward as the universal Egyptian lunar calendar.

# Part II — Native Egyptian time systems

## Shared temporal principle: twelve day-hours and twelve night-hours

Egyptian timekeeping developed around a twelvefold division of the day and a twelvefold division of the night. These were not simply twenty-four fixed modern SI hours.

In seasonal/temporal practice, daylight was divided into twelve parts and night into twelve parts. Their physical duration therefore changed with season and locality.

The Egyptian term `wnwt` is conventionally translated as "hour". It should not be silently equated with exactly 3,600 SI seconds.

There is no evidence for a native Egyptian formal system equivalent to modern minutes and seconds. Calendar must never invent "Egyptian minutes" or "Egyptian seconds".

## 1. Decanal / stellar night timekeeping

### Historical character

Egyptian night-time reckoning used stars and star groups, especially the objects modern Egyptology calls **decans**. Diagonal star tables preserve ordered stellar material associated with the Egyptian calendar and twelve night divisions.

The term **diagonal star table** is preferred over "diagonal star clock" because calling every surviving table a clock assumes a specific function that remains under scholarly reconstruction.

### What is attested

- ordered decanal/star material;
- twelve-row structures in the diagonal tables;
- association with ten-day divisions of the Egyptian civil year;
- a long Egyptian tradition of using stellar phenomena to reckon the night.

### What is reconstructed

The exact mapping from every surviving table to a continuously running modern-style timestamp is not directly preserved. A provider must therefore reconstruct observation rules explicitly rather than simply reading table cells as modern hours.

### Project classification

**Attested stellar tradition; exact clock algorithm is reconstruction-dependent.**

A future stellar/decanal provider must be separate from the water-clock civil-night provider.

## 2. Seasonal daylight hours — shadow-clock and sundial tradition

### Historical character

Egyptians used the Sun's shadow to reckon daytime hours. Surviving shadow clocks and sundials demonstrate a native daylight timekeeping tradition in which the twelve daylight hours were seasonal rather than permanently equal SI durations.

Shadow clocks and sundials are **instruments**, not automatically separate time systems. For Calendar they belong to one major historical family unless a particular instrument is shown to encode materially different hour semantics.

### Computational reconstruction

For a deterministic modern implementation, Calendar may divide an explicitly chosen solar daylight interval into twelve equal temporal hours. The exact boundaries — for example computed sunrise and sunset — must be named as a **reconstruction**, not asserted as the universal definition encoded by every Egyptian instrument.

### Project classification

**Attested seasonal daylight-hour system; exact astronomical boundary convention reconstructed.**

## 3. Seasonal night hours — outflow water-clock tradition

### Historical character

Surviving Egyptian outflow water clocks provide exceptionally strong physical evidence for seasonal night-hour measurement.

The Karnak water clock associated with Amenhotep III has twelve interior columns of hour marks associated with the twelve civil months. The changing calibration reflects changing night length through the year.

Water drained continuously between hour marks. This matters for modern presentation: the instrument gives physical evidence of continuous progress through an hour even though the Egyptians did not have an attested minute/second notation.

### Civil-night reconstruction

Modern scholarship distinguishes stellar/decanal astronomical night from the civil-night interpretation of New Kingdom water clocks. A practical provider may model the civil night from computed sunset to following sunrise and divide it into twelve temporal hours, but that must be labelled a **New Kingdom civil/water-clock reconstruction**, not a simulation of every stellar system.

### Project classification

**Attested instrument and seasonal-hour principle; exact software boundaries reconstructed.**

This remains the strongest candidate for the first production Egyptian night clock.

## 4. Ramesside stellar timekeeping

### Historical character

Ramesside astronomical material represents a later stellar timekeeping tradition distinct in form from the earlier Middle Kingdom diagonal star tables. Surviving star tables describe observations of stars relative to the human body or a sighting framework and are important evidence for night-time reckoning.

### Caution

It is unsafe to treat a Ramesside star table as a ready-made digital clock algorithm. The surviving texts/tables require interpretation, and not every scholarly reconstruction agrees on their practical use or precision.

### Project classification

**Attested source family; computational clock semantics remain research-dependent.**

Calendar should preserve this as a distinct historical family until research shows whether it deserves a separate provider or should become a variant of a broader Egyptian stellar clock.

## 5. Merkhet / meridian-transit stellar timekeeping

### Historical character

Egyptian astronomers used sighting/alignment methods associated with the **merkhet** and stellar meridian observations. Such methods could establish north-south alignment and support night-time reckoning from stellar transits.

The instrument and observational practice are genuinely Egyptian. That does not automatically mean "merkhet time" was a separately named public time scale analogous to a modern clock mode.

### Project classification

**Attested instrument/observational practice; provider status undecided.**

Calendar should keep this family documented separately while determining whether its computational semantics are sufficiently distinct from broader stellar/decanal timekeeping to justify a separate selector entry.

# Precision and modern software

## Preserve the canonical instant

Historical presentation must never destroy timestamp precision.

A filesystem timestamp, event instant or database value remains canonical. Egyptian presentation is derived from it.

For an instant `T` inside a reconstructed Egyptian temporal hour bounded by `A` and `B`, Calendar may compute:

```text
phase = (T - A) / (B - A)
```

with `0 <= phase < 1`.

The phase can retain the precision supplied by the source timestamp.

## Do not invent historical subdivisions

The phase is a **modern precision extension**, not an Egyptian minute or second.

A historically conservative ordinary display may say:

```text
5th hour of the night
```

A precision-sensitive human display may say:

```text
5th hour of the night · 63.7% elapsed
```

provided the UI clearly identifies the fractional component as modern interpolation.

A graphical indicator is also acceptable where it represents continuous shadow or water-level progress without inventing an ancient unit name.

Egyptian unit fractions are historically authentic mathematics, but there is currently insufficient evidence that Egyptians routinely wrote clock timestamps as fractional `wnwt`. Calendar must therefore not present an Egyptian-fraction timestamp as attested notation unless specific evidence is established later.

# Relationship among the ten families

The five date families and five time families are **independent historical models**. They did not all coexist unchanged, and the project must not fabricate one master "Egyptian date/time standard" spanning three millennia.

Examples:

- civil dates can be paired historically with regnal year labels;
- lunar festival reckoning can coexist with civil dates without implying a completely separate civil administration calendar;
- Middle Kingdom diagonal star material must not automatically be paired with a New Kingdom water-clock model and described as one contemporary system;
- a modern user may intentionally combine independently selectable Egyptian date and time modes, but the UI must not claim that every combination is historically attested.

# Implementation admission order

Historical importance alone does not make a provider ready. The recommended admission order is based on evidential and computational strength.

### Date systems

1. **Egyptian civil calendar** — strongest deterministic arithmetic foundation.
2. **Egyptian regnal dating** — after a defensible ruler/accession chronology data model exists.
3. **Late Egyptian 25-year schematic lunar cycle** — deterministic once the source cycle is fully encoded and validated.
4. **Egyptian lunar date-keeping** — after observational/schematic variants and period vocabulary are defined.
5. **Early lunar/lunistellar reckoning** — remain research-blocked while the governing reconstruction is contested.

### Time systems

1. **Seasonal night hours / outflow water-clock reconstruction** — strongest surviving instrument basis.
2. **Seasonal daylight hours / shadow-clock-sundial reconstruction** — strong instrument basis with explicit solar-boundary reconstruction.
3. **Decanal / stellar night timekeeping** — after an admission-grade stellar observation model exists.
4. **Ramesside stellar timekeeping** — after source-specific observation semantics are validated.
5. **Merkhet / meridian-transit timekeeping** — expose separately only if research establishes a distinct reproducible clock contract.

This ordering is not a claim of historical chronology. It is an engineering admission order based on how strongly a deterministic model can presently be defended.

# Validation requirements

Any Egyptian provider must test the claims specific to its historical model. As applicable, validation must include:

- attested calendar structure and exceptional days;
- 29/30-day lunar behaviour where relevant;
- exact schematic-cycle arithmetic where relevant;
- ruler/accession/coregency data provenance for regnal dating;
- twelve day-hours and/or twelve night-hours where required;
- seasonal change in temporal-hour SI duration;
- astronomical boundary transitions;
- location requirements and invalid/missing coordinates;
- separation of stellar and water-clock night models;
- exact monotonic within-hour phase for modern precision contexts;
- canonical timestamp preservation;
- absence of invented minute/second terminology;
- period-appropriate labels and vocabulary;
- explicit unavailable state where the evidence or physical event cannot establish a value.

A plausible-looking value is not a substitute for an evidence-backed model.

# Principal references

These sources support different parts of the catalogue; none is treated as blanket authority for every reconstruction.

- Janice Kamrin, **"Telling Time in Ancient Egypt"**, The Metropolitan Museum of Art, 2017. Overview of the civil calendar, regnal dating, lunar practice, twelve day/night hours, decans, shadow clocks, sundials and water clocks; explicitly notes the absence of tracked minutes/seconds. <https://www.metmuseum.org/essays/telling-time-in-ancient-egypt>
- Rita Gautschy, **"Lunar Calendar"**, UCLA Encyclopedia of Egyptology, 2024. Current survey of Egyptian lunar reckoning, the debate over independent lunar calendars, lunar-day rules and the Papyrus Carlsberg 9 25-year scheme. <https://escholarship.org/content/qt4zq1z7q1/qt4zq1z7q1.pdf>
- Ancient Egyptian Astronomy Database, McMaster University. Scholarly catalogue of decans, diagonal star tables, sundials, water clocks and astronomical representations. The project explicitly prefers **diagonal star table** over terms that assume a clock/calendar function. <https://aea.mcmaster.ca/>
- Sarah Symons, **"Ancient Egyptian astronomy: timekeeping and cosmography in the New Kingdom"**, doctoral thesis, University of Leicester. Detailed study of stellar timekeeping, shadow clocks, sundials and New Kingdom astronomical practice. <https://figshare.le.ac.uk/articles/thesis/Ancient_Egyptian_astronomy_timekeeping_and_cosmography_in_the_new_kingdom/10097984>
- B. Cotterell, F. P. Dickson and J. Kamminga, **"Ancient Egyptian water-clocks: A reappraisal"**, *Journal of Archaeological Science* 13(1), 1986, 31–50, DOI 10.1016/0305-4403(86)90025-7. Analysis of seasonal night-hour calibration and the distinction between astronomical and civil night.
- Richard A. Parker, **The Calendars of Ancient Egypt**, Studies in Ancient Oriental Civilization 26, University of Chicago Press / Oriental Institute, 1950. Foundational study; still important but superseded or disputed on several lunar-calendar questions by later scholarship. <https://isac.uchicago.edu/sites/default/files/uploads/shared/docs/saoc26.pdf>

# Governing rule

The project documents and implements **the Egyptian systems themselves**, not every later or imported system that happened to be used in Egypt.

Where the evidence gives a deterministic system, implement it faithfully. Where it gives several plausible reconstructions, name the reconstruction. Where it gives an instrument but not a distinct time scale, do not manufacture one. Where the evidence is insufficient, unsupported is more historically accurate than a convenient invention.
