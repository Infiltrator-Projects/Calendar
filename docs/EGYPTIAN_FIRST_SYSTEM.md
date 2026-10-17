<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# First Egyptian calendar and time system

## Purpose

This document defines the **first Egyptian date-and-time profile** that Calendar should implement when working through native Egyptian systems in historical order.

It records the project's **current best historical model** from the available archaeological and specialist evidence. It is not frozen doctrine. If stronger evidence or specialist consensus changes, the model, tests and presentation must change with it.

The user-facing system names must not contain qualifiers such as "reconstruction" merely because the surviving evidence is incomplete. Instead, this document records what evidence supports the model, what remains uncertain, and what must be revised if the evidence changes.

The parent catalogue is [EGYPTIAN_CHRONOLOGY_AND_TIMEKEEPING.md](EGYPTIAN_CHRONOLOGY_AND_TIMEKEEPING.md). `MODELS.md` remains authoritative for general model admission, failure and validation rules.

## First profile

The first Egyptian profile is:

```text
Calendar: Early Egyptian Lunar Calendar
Time:     Early Egyptian Stellar–Solar Time
```

These are the names a user should conceptually select for the earliest Egyptian profile once both providers are implemented and admitted.

# Calendar — Early Egyptian Lunar Calendar

## Historical position

The earliest Egyptian calendar was most likely lunar. A 2024 peer-reviewed UCLA Encyclopedia of Egyptology survey states that a lunar or lunistellar calendar was in use in ancient Egypt and considers a lunar calendar in Predynastic and Protodynastic Egypt very likely, although not definitively proven. The Egyptian word for month is written with a lunar-crescent sign, and later religious evidence preserves extensive lunar terminology and lunar feast practice.

The fixed 365-day civil calendar belongs later, by the first half of the third millennium BCE. It is therefore not the first calendar in this historical sequence.

## Current project model

Calendar shall model the earliest system as an **observational lunar calendar associated with the Nile seasonal cycle**.

The current model is:

- a month is a lunation;
- a lunation contains 29 or 30 days according to lunar observation;
- the lunar day begins at dawn;
- the month turns when the old waning crescent is no longer visible in the morning sky before sunrise;
- twelve lunar months form the ordinary annual sequence;
- the annual sequence is associated with the arrival of the Nile inundation rather than treated as a free-running modern arithmetic year;
- the calendar does not currently assert a fixed Sirius/Sopdet intercalation rule for this earliest period.

The last point is deliberate. Richard Parker's influential model proposed a sophisticated lunistellar year beginning with the new moon following the heliacal rising of Sirius/Sopdet. More recent specialists, notably Juan Antonio Belmonte and José Lull, reject that rule for Predynastic/Protodynastic Egypt and instead propose that lunar months were counted from the arrival of the inundation without further formal regulation. Rita Gautschy's 2024 survey records that disagreement and states that there is no consensus on how the earliest lunar calendar was aligned with the tropical year, if it was aligned at all.

For the **first Egyptian profile**, the project therefore follows the simpler Nile-associated lunar model unless stronger evidence changes the balance.

## Lunar month boundary

The best attested Egyptian lunar-day rule from later evidence begins a new lunation with the first morning on which the old crescent is no longer visible before sunrise. The resulting lunation is observational and can contain 29 or 30 days.

The project applies that Egyptian observational rule to the first calendar because it is the strongest surviving native rule for Egyptian lunar timekeeping. If future evidence demonstrates that the earliest period used a materially different lunar boundary, the provider must be changed.

This must not be replaced by the more familiar first-evening-visibility rule used by some other lunar calendars.

## Year and seasonal anchor

The earliest annual regulation is the least certain part of the calendar.

The current model treats the **Nile inundation as the annual seasonal anchor**. It does not fabricate a mechanically repeating intercalation cycle where the evidence does not establish one.

For an eventual deterministic software provider, the implementation must define and document how the historical inundation anchor is mapped onto an absolute modern time line. That mapping is an engineering necessity, not permission to rewrite the historical model. Until the mapping has passed historical and numerical validation, absolute conversion for the earliest calendar must remain unavailable rather than silently substitute the later civil calendar or Parker's Sirius rule.

## Display character

A native presentation should identify:

- lunar month within the annual sequence;
- lunar day;
- where supported, an historically justified lunar-day name;
- the annual Nile-season association when it can be established.

It must not back-project the complete Ptolemaic/Roman list of thirty lunar-day names into the earliest period. Complete surviving lists are late; earlier evidence contains only subsets and variants.

# Time — Early Egyptian Stellar–Solar Time

## Historical position

The earliest systematic Egyptian hour-like evidence is stellar.

The Pyramid Texts, before about 2400 BCE, contain the word `wnwt` with a star determinative, associating the concept with night. By about 2000 BCE, Middle Kingdom diagonal star tables contain twelve stellar rows. Cockcroft and Symons identify these as the earliest systematic division of the night into twelve time areas, each associated with a star.

The explicit surviving evidence for **twelve day `wnwt` and twelve night `wnwt` together** is later, in the New Kingdom. At that stage daylight was measured from solar shadows, night primarily from stars, and the hour lengths varied through the year.

The first full-day time provider therefore uses the earliest stellar evidence for the night and the Egyptian solar/shadow tradition for the day as one continuous historical model of Egyptian temporal time.

## Current project model

The first time system is:

```text
Early Egyptian Stellar–Solar Time
```

Its rules are:

- **night:** twelve stellar temporal divisions;
- **day:** twelve solar temporal divisions;
- day and night are numbered separately, never as one ancient 1–24 sequence;
- the duration of a temporal division changes with season;
- daytime position is governed by the Sun/shadow cycle;
- nighttime position is governed by the stellar/decanal cycle;
- sunrise and sunset/twilight transitions remain distinct from numbered hours where the historical evidence requires that distinction;
- no Egyptian minutes or seconds are invented.

This model deliberately follows the Egyptian evidence that day and night were conceptually separate time systems. New Kingdom sources explicitly show twelve day-hours and twelve night-hours, while specialist analysis notes transition periods around sunrise and sunset in which neither the solar nor stellar observation was usable.

## Night divisions

The night side is the older and more directly evidenced half of the model.

Middle Kingdom diagonal star tables from around 2000 BCE normally use twelve rows and track changing stellar configurations across ten-day periods. The modern Ancient Egyptian Astronomy Database prefers the neutral term **diagonal star tables**, because calling every surviving table a "clock" would assume more than the object itself proves.

Calendar may nevertheless use the twelvefold stellar structure as the basis of the time provider because the later Egyptian evidence explicitly connects twelve rows with `wnwt`, and the developmental continuity from stellar night divisions to later Egyptian night-hours is the strongest available model.

The provider must not pretend that every diagonal table is an executable clock algorithm. Its astronomical mapping must be based on the best-supported stellar/decan model available when implementation occurs.

## Day divisions

The surviving physical evidence for shadow clocks and sundials is later than the earliest stellar tables, but the mature native Egyptian system explicitly contains twelve daylight `wnwt` measured from the Sun and shadows.

For the first full-day profile, Calendar therefore uses twelve **seasonal solar day divisions** as the daytime counterpart of the twelve stellar night divisions.

The day divisions are unequal in SI duration across the year: summer daylight divisions are longer, winter daylight divisions shorter. The reverse applies to the night divisions.

If future evidence shows that the earliest period used a different number or structure of daytime divisions, this model must be updated rather than preserving the present choice for compatibility.

## Dawn, dusk and transition periods

The project must not force the Egyptian system into a modern seamless 24-hour clock if the evidence says otherwise.

Current specialist interpretation states that the day and night systems depended respectively on visible solar shadows and visible stars, leaving periods around sunrise and sunset that contained no numbered `wnwt` in the later explicit system.

Accordingly, the provider may expose states such as:

```text
Dawn transition
1st hour of the day
...
12th hour of the day
Dusk transition
1st hour of the night
...
12th hour of the night
```

The astronomical definitions used to make those transition boundaries deterministic must be documented and tested. They may be refined as specialist evidence improves.

## Precision

Historical display uses the Egyptian temporal division, for example:

```text
5th hour of the night
```

The canonical timestamp remains exact underneath.

Where a file timestamp, forensic view or other precision-sensitive context must distinguish instants within one Egyptian hour, Common may expose a dimensionless progress value through the current division:

```text
phase = (instant - division_start) / (division_end - division_start)
```

For example:

```text
5th hour of the night · 63.7% elapsed
```

That fractional phase is modern presentation of continuous position. It is not an "Egyptian minute" or "Egyptian second" and must never be labelled as one.

# First-profile selector contract

When both providers are implemented, the first Egyptian historical profile in System Settings should resolve to exactly these two independent choices:

```text
Calendar
    Early Egyptian Lunar Calendar

Clock
    Early Egyptian Stellar–Solar Time
```

Selecting the profile does not rewrite canonical timestamps. Calendar and clock remain presentation dimensions supplied through the shared Temporal policy.

# Evidence-change policy

This model is intentionally evidence-led.

A later publication, newly interpreted artefact, improved astronomical analysis or stronger specialist consensus may alter:

- the annual anchor of the earliest lunar calendar;
- whether and how a thirteenth month was inserted;
- the exact first-day lunar visibility rule in the earliest period;
- the stellar mapping of the twelve night divisions;
- the existence or placement of dawn/dusk transition intervals;
- the structure of the earliest daylight divisions.

When that happens, the project changes the model. Historical compatibility is not a reason to preserve a rule known to be wrong.

Changes that alter visible historical semantics require corresponding documentation, validation and release notes.

# Evidence base

The current model is based primarily on:

- Rita Gautschy, **"Lunar Calendar"**, *UCLA Encyclopedia of Egyptology* (2024), DOI 10.5070/G9.3898. This is the principal current survey for the early lunar-calendar question. It regards an early lunar calendar as very likely, documents the 29/30-day observational month and morning-invisibility rule, and records the modern disagreement between Parker's Sirius-regulated lunistellar model and the simpler inundation-counting model of Belmonte and Lull. <https://escholarship.org/uc/item/4zq1z7q1>
- Juan Antonio Belmonte and José Lull, **Astronomy of Ancient Egypt: A Cultural Perspective** (Springer, 2023), especially the calendar and timekeeping chapters. Their current synthesis favours simpler local Nile-based lunar calendars before the civil calendar rather than projecting Parker's sophisticated Sirius-regulated scheme into the Predynastic/Protodynastic period. DOI 10.1007/978-3-031-11829-6.
- Robert Cockcroft and Sarah Symons, **"Ancient Egyptians measured the first hour, and changed how we related to time"** (2023), summarising the `wnwt` evidence, twelve stellar night divisions by about 2000 BCE, the later explicit twelve day/twelve night system, seasonal hour lengths and sunrise/sunset transition periods. <https://news.mcmaster.ca/analysis-ancient-egyptians-measured-the-first-hour-and-changed-how-we-related-to-time/>
- Ancient Egyptian Astronomy Database, McMaster University, **Diagonal Star Tables**, for the surviving Middle Kingdom table corpus and the methodological caution against assuming that every diagonal table's purpose was literally a clock. <https://aea.mcmaster.ca/index.php/en/database/diagonal-star-tables>
- Janice Kamrin, **"Telling Time in Ancient Egypt"**, The Metropolitan Museum of Art (2017), for the general lunar-origin assessment, dawn day boundary, twelve day/twelve night structure and later physical evidence for sundials, shadow clocks and water clocks. <https://www.metmuseum.org/essays/telling-time-in-ancient-egypt>

# Admission rule

The first profile becomes selectable only when both providers can produce deterministic output with tests strong enough to distinguish plausible-looking output from the documented historical model.

Where the historical input required for an absolute conversion cannot yet be established responsibly, the correct state is unavailable. The project should improve the model as evidence improves rather than conceal uncertainty behind a convenient but unrelated calendar or clock.