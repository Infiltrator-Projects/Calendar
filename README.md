<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calendar

[![Build and test](https://github.com/Infiltrator-Projects/Calendar/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Infiltrator-Projects/Calendar/actions/workflows/ci.yml?query=branch%3Amain)

Calendar is a native Cinnamon panel clock and calendar with alternative clock modes, 30 selectable calendar systems and CalendarServer integration. Its installed Linux desktop identity, Cinnamon Applets manager entry and native About dialog use the project-owned neon Calendar icon.

**Stable release:** 1.0.15.

**Runtime:** Cinnamon 6.4, 6.6 and 6.7.

**Build-tested bases:** Debian 13, Linux Mint 22 and Ubuntu 24.04.

## Engineering ethos

Calendar is a first-principles engineering project. It does not treat the
existing desktop clock/calendar as the specification for what a calendar can
be. The question is what a calendar and timekeeping system should look like
when chronology, astronomy, historical evidence, international standards,
human-interface research and current software-engineering practice are all
considered together.

"Modern" is not treated as a synonym for "better". Current research, standards
and mature implementations are studied deliberately, but techniques are adopted
because they improve correctness, fidelity, performance, resilience, usability,
accessibility or maintainability, not merely because they are new. Older
algorithms and conventions are retained when they remain the strongest
available model; historical claims are separated from modern computational
continuation rules when the evidence does not justify pretending they are the
same thing.

External authorities such as ICU/CLDR, Cinnamon and established astronomical
models are evidence and infrastructure to use where they are authoritative.
They are not excuses to duplicate assumptions blindly. Calendar owns the
behaviour for which it is responsible, documents epochs, continuation rules and
approximations explicitly, and tests those choices against known references and
edge cases.

The implementation therefore aims for the best justified approach rather than
the newest fashionable one or the easiest conventional one. Architecture,
chronology and astronomical behaviour should be measurable, testable and
explainable. A feature is not complete merely because it produces a plausible
date or clock reading: locality, historical scope, polar and boundary cases,
failure modes, reproducibility and regression protection are part of the
feature.

The same principle applies across the software family: start from the
problem, study what is known now, preserve proven ideas that still deserve to
survive, replace assumptions that no longer do, and prove the result in real
use.

## Capabilities

Clock modes include normal 12/24-hour time, French Republican decimal time, Internet Time, Unix time, hexadecimal and binary time, sidereal time, apparent and mean solar time, Julian/MJD, traditional Chinese double-hours and hundred-kè time, Roman temporal time, Edo Japanese seasonal time, Italian hours from sunset, the historical European gnomonic convention known as Babylonian hours from sunrise, Indian ghaṭī time from sunrise, and Nuremberg equal hours resetting at sunrise and sunset.

Primary and optional secondary dates include Gregorian, Julian, ISO week, Hebrew, four Islamic variants, Persian, Chinese, Dangi, Indian National, Coptic, Ethiopic Amete Mihret and Amete Alem, Buddhist, Japanese, Minguo, French Republican, Roman, Mayan Long Count with Tzolk’in/Haab Calendar Round, Badíʿ, International Fixed, World, Positivist, Revised Julian, Byzantine Anno Mundi, Egyptian civil (Nabonassar era), traditional Armenian, and Sweden's historical 1700–1753 civil calendar including 30 February 1712.

The applet has its own seconds preference, can coexist with Cinnamon's stock Calendar applet and installs no project-owned daemon, polling service or autostart entry.

Calendar prefers the MB Corpo family throughout its owned interface when those fonts are already installed on the host: MB Corpo S Regular for normal UI text and the slightly enlarged panel clock, MB Corpo S Bold for emphasis and actions, and MB Corpo A Condensed Regular for condensed title display. Calendar does not redistribute proprietary MB Corpo font binaries; when they are unavailable Cinnamon/GTK font fallback is used automatically.

Location-dependent clocks do not silently assume Greenwich. They show `N/A LOC` until **Geographic location** is enabled and coordinates are supplied. Existing non-zero coordinates from older releases are migrated automatically; a genuine 0°, 0° location can now be selected explicitly.

Calendar workday styling follows ICU/CLDR weekend data for the active locale, including regions whose weekend is not Saturday/Sunday. If locale weekend data is unavailable, Calendar falls back to Monday-Friday workdays.

## Architecture

The source tree is grouped by responsibility:

```text
src/
├── core/       Portable calendar, clock and event logic
├── adapters/   GLib, GVariant and native integration
├── app/        Project identity, version API and About helper
├── cinnamon/   Cinnamon JavaScript runtime and settings
├── i18n/       Gettext sources and translations
├── abi/        Exported-library version map
└── vendor/     Pinned Infiltratr Common submodule
```

The portable core is kept separate from presentation and platform adapters. Cinnamon owns desktop actors, settings and CalendarServer transport; the native C library owns chronology, astronomy, alternative clocks and event semantics. Generic strings, UTF-8 validation, checked/saturating arithmetic, timing and dynamic-library mechanics are supplied by the exact Common 1.19.3 release. Calendar uses checked Common arithmetic where overflow makes a chronology/astronomy result invalid, and saturating Common arithmetic only where clamping is the deliberate Calendar policy; ICU-derived allocation sizes are validated with Common's checked size primitives before allocation. The build verifies both the Common version and immutable release commit before linking.

### Native ABI policy

The installed `libcalendar-plus.so.0` and its versioned symbol map are a **runtime stability contract for Calendar itself**, not a separately supported C SDK. The neutral core headers remain source-internal and are not installed as a development package. The exported ABI is nevertheless regression-tested so the Cinnamon typelib, About helper and packaged runtime cannot drift accidentally between releases. If a supported third-party SDK is ever introduced, it will get installed headers, pkg-config metadata and its own compatibility policy rather than silently treating these internal headers as public.

### Compatibility naming

The user-facing application is **Calendar**. The compatibility identifiers `calendar-plus`, `CalendarPlus`, `CALENDAR_PLUS_*`, `libcalendar-plus.so.0` and `calendar-plus@the-infiltratr` are intentionally retained so package upgrades, settings, GObject Introspection consumers and the published runtime ABI do not break. Release asset filenames use the user-facing `Calendar` name; the Debian package contained inside remains `calendar-plus` for upgrade compatibility. Prose should therefore say **Calendar** unless it is naming one of those literal compatibility interfaces. Renaming a compatibility identifier is a separate migration with ABI, packaging and upgrade consequences; it is not a cosmetic documentation change.

### Engineering contracts and invariants

These contracts define the meaning of values crossing module boundaries. They are part of the design, not incidental implementation details.

| Contract | Invariant |
| --- | --- |
| Civil-date boundary | `CalendarPlusDate` is a proleptic-Gregorian civil coordinate. Alternative calendars format and navigate that absolute coordinate rather than replacing event storage with calendar-specific dates. |
| Internal date axis | Date-only native algorithms use an integral, midnight-based Julian Day Number. This is deliberately distinct from the fractional astronomical Julian Date whose day boundary is noon. |
| Year numbering | Internal proleptic arithmetic uses astronomical numbering where the provider requires it, including year 0. Presentation code is responsible for historical era labels. |
| Absolute time | Native clock instants are signed Unix microseconds from 1970-01-01T00:00:00Z. Event transport fields documented as Unix seconds remain seconds until their single normalisation boundary. |
| Geographic coordinates | Latitude is north-positive and longitude east-positive, in degrees. Location-dependent modes report unavailable when a location or required solar crossing is unavailable; they do not invent Greenwich or extrapolated polar events. |
| Arithmetic | Checked Common arithmetic is used when overflow means a result is unrepresentable. Saturating arithmetic is used only where clamping is the explicit policy. Signed-overflow undefined behaviour is not an accepted failure mode. |
| Event intervals | Timed event endpoints are inclusive. CalendarServer all-day end points enter as exclusive following-midnight values and are converted to an inclusive final instant exactly once in the native event boundary. |
| Event ownership | The native event index is single-owner-thread state. Snapshots are detached deep copies; their revision value is an opaque equality token rather than a timestamp for callers to interpret. |
| Localisation | ICU/CLDR and Cinnamon own locale-sensitive calendar names, weekend policy and conventional clock presentation where their data is authoritative. Project-owned strings use gettext. |
| Failure semantics | Invalid, unsupported or physically undefined states return the documented empty/false/null/unavailable result. A plausible-looking fabricated date or clock value is considered a correctness failure. |

## Correctness model

| Area | Authority / model |
| --- | --- |
| Conventional panel time and Gregorian locale presentation | CinnamonDesktop.WallClock |
| Hebrew, Islamic, Persian, Chinese, Dangi, Indian, Coptic, Ethiopic, Buddhist, Japanese and Minguo calendars | ICU/CLDR calendar data |
| Julian, ISO week, French Republican, Roman, Mayan, Badíʿ, International Fixed, World, Positivist, Revised Julian, Byzantine Anno Mundi, Egyptian civil (Nabonassar era) and traditional Armenian calendars | Calendar deterministic native algorithms |
| French Republican decimal, Internet, Unix, hexadecimal, binary and Chinese hundred-kè clocks | Exact integer/rational partitioning |
| Sidereal, solar, Roman temporal, Edo seasonal, Italian, Babylonian-hour, Indian ghaṭī and Nuremberg clocks | Native astronomical models using configured coordinates |

Historical calendars and clocks are deterministic computational models rather than claims about every historical locality or observational practice. Modern Badíʿ years use a Tehran-referenced astronomical March equinox and sunset boundary; years before 172 B.E. retain the historical Western 21-March civil convention. Italian hours use equal hours measured strictly from computed sunset. “Babylonian hours” uses the later European gnomonic convention of equal hours from sunrise; Calendar does not present that label as a reconstruction of ancient Mesopotamian civil timekeeping. The source comments document the exact continuation rules, epochs and astronomical assumptions used where more than one convention exists.

### Model provenance and references

Calendar treats external literature and standards as evidence for a particular model, not as a blanket claim that every implementation detail is inherited unchanged. The relevant source module records the subset used, deliberate approximations and project-specific continuation policy.

| Model / decision | Primary reference used by the implementation |
| --- | --- |
| General calendrical algorithms, epochs and cross-calendar reference practice | Edward M. Reingold and Nachum Dershowitz, *Calendrical Calculations: The Ultimate Edition*, 4th ed., Cambridge University Press, 2018, DOI [10.1017/9781107415058](https://doi.org/10.1017/9781107415058). |
| ICU-backed calendar variants and locale data | Unicode Locale Data Markup Language / CLDR calendar data and ICU calendar implementations: [Unicode TR35 / LDML](https://unicode.org/reports/tr35/) and [ICU](https://icu.unicode.org/). |
| Solar equation-of-time/declination and sunrise/sunset conventions | NOAA Global Monitoring Laboratory, [Solar Calculation Details](https://gml.noaa.gov/grad/solcalc/calcdetails.html), which documents a Meeus-derived solar-calculation model and the conventional 0.833° sunrise/sunset assumption. Calendar intentionally uses a compact fractional-year subset rather than claiming equivalence to NOAA's complete calculator. |
| Equinox calculation | Jean Meeus, *Astronomical Algorithms*, 2nd ed., Willmann-Bell, 1998, especially the equinox/solstice polynomial and periodic correction. |
| ΔT conversion used by the modern Badíʿ calculation | Fred Espenak and Jean Meeus, NASA GSFC, [Polynomial Expressions for Delta T](https://eclipse.gsfc.nasa.gov/LEcat5/deltatpoly.html). Only the pieces relevant to Calendar's supported modern-year range are implemented. |
| Modern Badíʿ Naw-Rúz reference location and equinox rule | Universal House of Justice, 10 July 2014, [message on implementation of the Badíʿ calendar](https://www.bahai.org/library/authoritative-texts/the-universal-house-of-justice/messages/20140710_001/1). Calendar's numerical astronomy is an implementation of that rule, not an official calendrical authority. |
| Cinnamon integration behaviour | The reviewed Linux Mint Cinnamon calendar surface recorded in `tools/upstream-calendar-baseline.json`; scheduled drift detection forces explicit review when that upstream integration surface changes. |

Reference provenance is deliberately kept close to the algorithms it justifies. A constant, epoch or historical rule that materially affects output should have either an explanatory source comment or an entry above; tests then protect the chosen interpretation from accidental drift.

## Build and test

Install the development dependencies on Debian 13, Linux Mint 22 or Ubuntu 24.04:

```bash
sudo apt install build-essential clang debhelper gettext gobject-introspection gjs \
    gir1.2-glib-2.0-dev libglib2.0-dev libicu-dev nodejs pkg-config \
    python3 ripgrep shellcheck git
```

Clone recursively because Calendar pins Infiltratr Common as a submodule:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Calendar.git
cd Calendar
make check
```

Git clones carry the pinned Common submodule. If a GitHub automatic source archive does not contain the vendor checkout, normal `make` automatically retrieves the exact pinned Common commit.

Additional quality gates are available through `make sanitize`, `make coverage`, `make static-analysis`, `make reproducible-build` and `make release-check`. GCC, Clang, sanitizer, coverage and static-analysis primary Linux qualification run on the trusted self-hosted Linux runners, while clean Debian packaging/install qualification remains independently hosted. CI requires at least 80% C line coverage and 60% branch coverage in addition to sanitizer, static-analysis, ABI, packaging and reproducibility gates. A clean Debian 13 container also builds, installs, executes and purges the generic package before a release is eligible for publication; the central package repository separately performs a full Linux Mint 22.3 package lifecycle test after publication.

For an installed Cinnamon session, `tools/cinnamon-smoke.sh` verifies installed runtime hashes and typelib identity, then exercises the live applet through Cinnamon's D-Bus evaluation interface without changing persistent settings. CI first probes whether a trusted runner can qualify the exact source revision. A matching installed version is tested directly; a Cinnamon runner with passwordless package-install permission builds, installs and reloads the exact revision before testing. When neither is possible the actual live-smoke step is explicitly shown as skipped rather than reporting an older applet as a pass.

A scheduled `upstream-calendar-drift` workflow watches Cinnamon's stock calendar applet, CalendarServer and resume integration. It does not import upstream code: it fails only when the reviewed upstream surface changes so Calendar can assess relevant compatibility fixes deliberately.

Release publication verifies the central package repository APT catalogue and waits for its five-minute safety refresh when required. The central repository detects when that refresh advances Calendar and runs its Linux Mint 22.3 lifecycle qualification for the newly published version. This avoids depending on a separately scoped cross-repository personal access token.

## Install and releases

Numbered releases publish two project-owned artifacts:

| File | Purpose |
| --- | --- |
| `calendar_<version>_amd64.deb` | Generic amd64 Debian package |
| `calendar-<version>-local-folder.run` | Verified local hardware-native builder |

Install the generic package with:

```bash
sudo apt install ./calendar_<version>_amd64.deb
```

Or use the native builder:

```bash
chmod +x calendar-<version>-local-folder.run
./calendar-<version>-local-folder.run
```

After installation, add **Calendar** from **System Settings → Applets**.

GitHub supplies the standard source ZIP and tarball for each immutable release tag, so the project does not upload a redundant source archive. Development is performed on `main`; every push is tested, and only a successful commit whose subject begins with `Release <version>` is eligible for automated tagging and publication.

## Project policies

Contribution guidance is in [`.github/CONTRIBUTING.md`](.github/CONTRIBUTING.md), security reporting is in [`.github/SECURITY.md`](.github/SECURITY.md), and participation standards are in [`.github/CODE_OF_CONDUCT.md`](.github/CODE_OF_CONDUCT.md). These files contain policy only; user and developer guidance remains consolidated here.

## Troubleshooting

- **No events:** confirm Cinnamon CalendarServer/Evolution Data Server is available; Calendar reconnects automatically after service restarts.
- **Alternative clock shows `N/A LOC`:** enable **Geographic location** in the applet settings and enter latitude/longitude.
- **Applet refuses to load after an upgrade:** a stale native library is rejected deliberately when its version differs from the installed applet; reinstall the matching package.
- **Custom format is invalid:** Calendar falls back safely instead of passing a null format result into the native library.
- **Installed-state verification:** run `tools/cinnamon-smoke.sh` from a matching source checkout inside the Cinnamon session.

## Model limits

Historical calendars and clocks are computational models. Islamic results are not local crescent observations; solar and seasonal clocks use defined astronomical approximations and may return N/A at polar latitudes. Japanese era output follows the installed ICU data.

## Translations

Project-owned catalogues are shipped for German, Spanish, French, Italian, Brazilian Portuguese, Russian, Japanese, Australian English, British English and United States English. Every maintained catalogue is deterministically merged against the generated POT during validation, so a new UI string cannot silently leave non-English catalogues structurally stale. Historical proper names and calendar terms whose conventional spelling is intentionally unchanged remain untranslated where appropriate. Other locales fall back to the source English strings; ICU and Cinnamon continue to localise the calendar/date material they own.

## Licence

Calendar is GPL-3.0-or-later. The pinned Infiltratr Common dependency uses the same licence. The complete project licence is in `LICENSE`; Debian packaging provenance is recorded in `debian/copyright`.
