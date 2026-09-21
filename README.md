<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calendar

**Project copyright:** © 1993-2026 Shannon Smith

[![Build and test](https://github.com/Infiltrator-Projects/Calendar/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Infiltrator-Projects/Calendar/actions/workflows/ci.yml?query=branch%3Amain)

Calendar is a native Cinnamon panel clock and calendar with alternative clock modes, 30 selectable calendar systems and CalendarServer integration. Its installed Linux desktop identity, Cinnamon Applets manager entry, Linux Mint Software Manager package-name alias and Cinnamon-native About dialog all use the project-owned Calendar icon. The Calendar package owns and installs that artwork itself; no shared icon-helper package is required.

**Stable release:** 1.0.45  
**Runtime:** Cinnamon 6.4, 6.6 and 6.7  
**Build-tested bases:** Debian 13, Linux Mint 22 and Ubuntu 24.04  
**Shared foundation:** pinned Infiltratr Common 1.19.20, including canonical theme, typography and structural-metric contracts

## Engineering ethos

Calendar is a first-principles engineering project. It does not treat the stock desktop clock/calendar as the specification for what chronology and timekeeping should be.

Historical evidence, international standards, maintained reference data, mature implementations and astronomical literature are treated as evidence. Techniques are adopted because they improve correctness, fidelity, performance, resilience, usability, accessibility or maintainability rather than because they are fashionable or new.

Calendar owns the behaviour for which it is responsible. Historical rules, continuation policies, epochs, units, astronomical assumptions, failure states and compatibility boundaries should be explicit, testable and explainable. A plausible-looking date or clock value is not considered sufficient evidence of correctness.

The same principle applies across the wider software family: study what is known now, preserve proven ideas that still deserve to survive, replace assumptions that no longer do, and prove the result in real use.

## System temporal authority

Calendar does not own temporal presentation preferences. When Infiltrator System Settings is installed, its valid shared policy is the richer authority for clock system, calendar system, seconds and geographic location, and Calendar watches that Common 1.19.20 policy for live changes. When System Settings is not installed, Calendar deliberately ignores any stale policy file left in the user's profile and behaves as a native Mint replacement: Cinnamon/locale settings remain authoritative for conventional 12/24-hour time, date visibility and presentation, seconds, first day of week and timezone; the stock Gregorian month view is used and no geographic extension is assumed. Calendar's own settings contain only Calendar-specific behaviour such as appearance, events, week numbers and keyboard shortcuts.

## Capabilities

Clock modes include normal 12/24-hour time, French Republican decimal time, Internet Time, Unix time, hexadecimal and binary time, sidereal time, apparent and mean solar time, Julian/MJD, traditional Chinese double-hours and hundred-kè time, Roman temporal time, Edo Japanese seasonal time, Italian hours from sunset, the historical European gnomonic convention known as Babylonian hours from sunrise, Indian ghaṭī time from sunrise, and Nuremberg equal hours resetting at sunrise and sunset.

Calendar systems available through System Settings include Gregorian, Julian, ISO week, Hebrew, four Islamic variants, Persian, Chinese, Dangi, Indian National, Coptic, Ethiopic Amete Mihret and Amete Alem, Buddhist, Japanese, Minguo, French Republican, Roman, Mayan Long Count with Tzolk’in/Haab Calendar Round, Badíʿ, International Fixed, World, Positivist, Revised Julian, Byzantine Anno Mundi, Egyptian civil (Nabonassar era), traditional Armenian, and Sweden's historical 1700–1753 civil calendar including 30 February 1712.

The applet can coexist with Cinnamon's stock Calendar applet and installs no project-owned daemon, polling service or autostart entry. System Settings is optional at runtime. Common's installed `temporal-v3` provider marker identifies whether the richer authority exists, while one cached effective snapshot keeps all temporal fields coherent. Without both the provider and a valid saved policy Calendar follows Mint/Cinnamon; with both, Calendar gains the extended clock, calendar, seconds and location presentation selected there. Calendar monitors both authority boundaries live and needs no background daemon.

Calendar bundles and installs the MB Corpo font set used by the project: MB Corpo S Regular, MB Corpo S Bold and MB Corpo A Condensed Regular. The package installs them under `/usr/share/fonts/truetype/infiltrator-calendar`. Typography identity, role weights, filenames and immutable asset hashes are taken from the pinned Common 1.19.20 design contract; Cinnamon selector mechanics remain Calendar-owned. Generated Cinnamon CSS is regression-checked against the same Common source. Configuration and About presentation now stay inside Cinnamon itself, so Calendar no longer carries a project-owned Python settings host or a direct GTK runtime dependency. Normal Cinnamon font fallback remains only as an operating-system fallback if font discovery fails.

Location-dependent clocks do not silently assume Greenwich. They show `N/A LOC` until geographic location is configured in **System Settings → Date & Time**. On a Mint-only installation those extended clock modes are not selected, so Calendar remains on Cinnamon's conventional clock path.

Calendar workday styling follows ICU/CLDR weekend data for the active locale, including regions whose weekend is not Saturday/Sunday. If locale weekend data is unavailable, Calendar falls back to Monday-Friday workdays.

## Documentation

The documentation set deliberately separates product overview from engineering authority:

- [Architecture](docs/ARCHITECTURE.md) — layers, ownership, event/state boundaries, failure semantics, Common integration, ABI and release contracts.
- [Design](docs/DESIGN.md) — first-principles goals, non-goals, dependency policy and decision quality.
- [Decisions](docs/DECISIONS.md) — durable architectural decisions.
- [Chronology, time and astronomy models](docs/MODELS.md) — evidence hierarchy, provenance, continuation rules, astronomical assumptions and model failure semantics.
- [Portability](docs/PORTABILITY.md) — C/C++/JavaScript boundaries, representation rules, platform seams, locale and compatibility contracts.
- [Validation](docs/VALIDATION.md) — automated, manual and environment-dependent evidence.
- [Qualification](docs/QUALIFICATION.md) — exact-source live Cinnamon/location evidence and evidence boundaries.
- [Roadmap](docs/ROADMAP.md) — current foundation and direction.
- [Documentation index](docs/README.md) — document authority and maintenance rules.

Code and tests remain authoritative for executable behaviour. Immutable tags and releases identify historical source.

## Architecture summary

```text
Cinnamon presentation / settings / panel lifecycle
                    ↓
         JavaScript/native boundary
                    ↓
GObject / GVariant / CalendarServer / main-loop adapters
                    ↓
portable Calendar domain contracts
                    ↓
chronology / clocks / astronomy / event semantics

Calendar native arithmetic    Infiltratr Common 1.19.20
     ↓                                  ↓
fixed-rule chronology         generic checked arithmetic /
                               formatting / timing / loading /

ICU / CLDR → remaining lunisolar/astronomical conversion and locale formatting
                               UTF-8 / allocation primitives
```

The source tree is grouped by responsibility:

```text
src/
├── core/       Portable chronology, clock, astronomy and event logic
├── adapters/   GObject, GVariant, main-loop and native integration
├── app/        Project identity, version API and About helper
├── cinnamon/   Cinnamon JavaScript runtime and settings
├── i18n/       Gettext sources and translations
├── abi/        Exported-library version map
└── vendor/     Pinned Infiltratr Common submodule
```

Calendar-specific chronology, continuation and astronomy policy remains local. Common owns only genuinely generic mechanisms. Cinnamon owns desktop integration. Fixed-rule calendar arithmetic is now Calendar-owned; ICU/CLDR remains authoritative only for the remaining lunisolar/astronomical providers and locale-formatting data Calendar still explicitly delegates.

The installed `libcalendar-plus.so.0` ABI and established `CalendarPlus` compatibility identifiers remain stable runtime contracts even though the visible product is **Calendar** and the Debian package is `infiltrator-calendar`.

Detailed representation, event ownership, failure, security and release contracts are maintained in [Architecture](docs/ARCHITECTURE.md) and [Portability](docs/PORTABILITY.md).

## Correctness and model scope

Historical calendars and clocks are deterministic computational models rather than claims about every historical locality or observational practice. Islamic results are not local crescent observations; solar and seasonal clocks use defined astronomical approximations and may be unavailable at polar latitudes; Japanese era output follows installed ICU data.

The evidence hierarchy, exact model authorities, continuation rules, astronomical references and admission requirements for new calendars/clocks are maintained in [MODELS.md](docs/MODELS.md).

Unavailable, unsupported or physically undefined states are represented explicitly. Calendar does not invent Greenwich, extrapolate nonexistent solar crossings or substitute plausible values for failed chronology.

## Build and test

Install the development dependencies on Debian 13, Linux Mint 22 or Ubuntu 24.04:

```bash
sudo apt install build-essential clang debhelper gettext gobject-introspection gjs \
    gir1.2-glib-2.0-dev libglib2.0-dev libicu-dev nodejs pkg-config \
    python3 shellcheck git
```

Clone recursively because Calendar pins Infiltratr Common:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Calendar.git
cd Calendar
make check
```

A recursive Git clone carries the pinned Common submodule. If that vendor checkout is absent, normal `make` automatically retrieves the exact Common commit recorded by Calendar before building; it does not follow an unpinned moving branch.


Additional gates include `make sanitize`, `make coverage`, `make static-analysis`, `make reproducible-build` and `make release-check`.

CI qualifies the portable/native code, JavaScript boundary, ABI, packaging and reproducibility contracts. The exact revision intended for release must pass the required gates. Live Cinnamon behaviour remains a distinct integration boundary and is verified with the installed-session smoke tooling where a suitable runner/session is available.

A scheduled upstream-drift workflow watches the reviewed Cinnamon calendar/CalendarServer integration surface. It triggers review when upstream changes; it does not import upstream behaviour automatically.

## Install and releases

Numbered releases publish two project-owned artifacts:

| File | Purpose |
| --- | --- |
| `calendar_<version>_amd64.deb` | Generic amd64 Debian package; package identity is `infiltrator-calendar` |
| `calendar-<version>-local-folder.run` | Verified local hardware-native PGO builder (`-O3 -march=native -mtune=native`, LTO, representative profile training, profile-use rebuild) |

Install the generic package with:

```bash
sudo apt install ./calendar_<version>_amd64.deb
```

Or use the native PGO builder:

```bash
chmod +x calendar-<version>-local-folder.run
./calendar-<version>-local-folder.run
```

The About dialog reports the compiled build profile using Infiltratr Common's canonical label, matching System Monitor: repository builds show **Generic / APT package**, while local `.run` builds show **Native / local machine compile**.\n\nThe `.run` performs a two-pass local build. Pass 1 instruments Calendar and the pinned Common library with GCC profile generation, runs Calendar's automated tests plus a representative clock/calendar/event workload on the actual machine, then pass 2 rebuilds with the measured profile using `-fprofile-use -fprofile-correction -fprofile-partial-training`. The resulting Debian-managed package is therefore tuned to the local CPU rather than being a generic repository build. The correctness-preserving profile deliberately does not use `-Ofast` or `-ffast-math`.

After installation, add **Calendar** from **System Settings → Applets**.

The previous `calendar-plus` and `cinnamon-calendar` Debian names are migration identities only. The central Infiltrator repository supplies transitional packages so installed systems move to `infiltrator-calendar` without losing the application.

GitHub supplies the standard source ZIP and tarball for each immutable release tag, so the project does not upload a redundant source archive.

## Troubleshooting

- **No events:** confirm Cinnamon CalendarServer/Evolution Data Server is available; Calendar reconnects automatically after service restarts.
- **Alternative clock shows `N/A LOC`:** configure geographic location in **System Settings → Date & Time**.
- **Applet refuses to load after an upgrade:** a stale native library is rejected deliberately when its version differs from the installed applet; reinstall the matching package.
- **Installed-state verification:** run `tools/cinnamon-smoke.sh` from a matching source checkout inside the Cinnamon session.

## Translations

Project-owned catalogues are shipped for German, Spanish, French, Italian, Brazilian Portuguese, Russian, Japanese, Australian English, British English and United States English.

Every maintained catalogue is merged deterministically against the generated POT during validation, so a new UI string cannot silently leave non-English catalogues structurally stale. ICU and Cinnamon continue to localise the calendar/date material they own.

## Project policies

Contribution guidance is in [CONTRIBUTING.md](CONTRIBUTING.md), security reporting is in [SECURITY.md](SECURITY.md), and participation standards are in [.github/CODE_OF_CONDUCT.md](.github/CODE_OF_CONDUCT.md).

Development is performed on `main`. Published tags and release assets are immutable identities.

## Licence

Copyright © 1993-2026 Shannon Smith.

Calendar is GPL-3.0-or-later. The pinned Infiltratr Common dependency uses the same licence. The complete project licence is in `LICENSE`; Debian packaging provenance is recorded in `debian/copyright`.


The packaged Calendar icon follows the non-automotive Infiltrator desktop family: graphite tile with canonical `#00ADEF` linework. One verified source asset feeds Cinnamon, the desktop icon theme and Mint app-install metadata.
