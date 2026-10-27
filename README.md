<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Calendar

**Project copyright:** © 1993-2026 Shannon Smith

[![Build and test](https://github.com/Infiltrator-Projects/Calendar/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Infiltrator-Projects/Calendar/actions/workflows/ci.yml?query=branch%3Amain)

Calendar is a native Cinnamon panel clock and calendar with alternative clock modes, 31 selectable calendar systems and CalendarServer integration. Its installed Linux desktop identity, Cinnamon Applets manager entry, Linux Mint Software Manager package-name alias and suite-standard native GTK About dialog all use the project-owned Calendar icon. The Calendar package owns and installs that artwork itself; no shared icon-helper package is required.

**Runtime:** Cinnamon 6.4, 6.6 and 6.7\
**Build-tested bases:** Debian 13, Linux Mint 22 and Ubuntu 24.04\
**Shared foundation:** an exact pinned Libraries release, including canonical theme, typography and structural-metric contracts

The released feature set is the completed Calendar 1.0 baseline. [Roadmap](docs/ROADMAP.md) defines its maintenance boundary and distinguishes optional research from release work.

Date and clock wording follows the desktop UI language while the selected calendar and clock retain their own rules. Traditional Chinese time renders as `Shēn hour` or `Shēn, first half` in English, and native Chinese forms in Chinese. Roman seasonal daylight clocks retain Roman numerals and native twelfths with translated wording. The panel schedules updates at those native boundaries. Calendar also exposes `calendar_format_date_v1`, a bounded C date-rendering entry point available to Common's optional POSIX bridge; it delegates conversion and formatting to the validated system resource.

Chinese and Korean Dangi dates/year headings use native cyclic year names, such as `bing-wu`, `丙午` or `병오`, without adding a Gregorian reference year or a bare cycle counter. Edo clocks retain Japanese bell numerals with localized branch names and wording; explicit 12-hour clocks use locale-specific day-period markers and ordering. The same date output is supplied to Files and System Settings.

## Engineering ethos

Calendar follows the [Infiltrator Engineering Philosophy](docs/PHILOSOPHY.md): correctness, restrained design, explicit ownership, recoverable failure and proven behaviour take priority over novelty or code volume. Historical evidence, standards and mature platform libraries inform the design but do not silently determine its semantics.

Cinnamon owns desktop integration; Calendar owns application and event behaviour; Common owns reusable mechanisms and temporal resource ABIs; independently installed resources own chronology and clock algorithms. The authoritative design and evidence boundaries are maintained in [Architecture](docs/ARCHITECTURE.md), [Decisions](docs/DECISIONS.md) and [Models](docs/MODELS.md).

## System temporal authority

All thirty-one calendar implementations and twenty-three explicit clock modes
are independently installed system resources. Each `Calendar-*` or `Clock-*`
repository owns its module, tests, translations, package and native installer.
Their packages own the files under `/System/Resources/Calendars/` and
`/System/Resources/Clocks/`; removing an application leaves those packages
installed. Common owns neutral APIs, validated discovery/loading and the policy
bridge, and preserves its original implementation source without building it.
Calendar, Files, Settings and the desktop consume the same installed providers.
Calendar's production core contains no selectable chronology or clock-face
fallback implementation. Each clock resource owns its notation and exact next
display boundary. Missing or malformed resources are visibly unavailable.
The canonical Unix timestamp and raw timestamp sorting never change with the
selected calendar or clock. OS-following `standard` remains a platform policy
choice; explicit 12/24-hour modes require their respective resources.

Calendar does not own temporal presentation preferences. System Settings is the writer/UI for the richer temporal-v3 policy covering clock system, calendar system, seconds and geographic location. Once a valid shared policy has been written, that persisted system policy remains authoritative even if the System Settings package is later removed; removing the settings UI must not silently discard the user's selected system behaviour. If no valid temporal-v3 policy exists, Calendar behaves as a native Mint replacement: Cinnamon/locale settings remain authoritative for conventional 12/24-hour time, date visibility and presentation, seconds, first day of week and timezone; the stock Gregorian month view is used and no geographic extension is assumed. Calendar's own settings contain only Calendar-specific behaviour such as appearance, events, week numbers and keyboard shortcuts.

## Capabilities

Clock modes include normal 12/24-hour time, French Republican decimal time, Internet Time, Unix time, hexadecimal and binary time, sidereal time, apparent and mean solar time, Julian/MJD, traditional Chinese double-hours and hundred-kè time, Roman temporal time, late-Edo 1797 twilight time and early-Edo sunrise/sunset unequal time, Italian hours from sunset, the Renaissance European gnomonic convention known as Babylonian hours from sunrise, ancient Babylonian bēru/UŠ time, Indian ghaṭī time from sunrise, and the historical Nürnberg Great Clock with fixed Wendetage and a separate location-aware Nuremberg-style solar reconstruction.

Calendar systems available through System Settings include Gregorian, Julian, ISO week, Hebrew, four Islamic variants, Persian, Chinese, Dangi, Indian National, Coptic, Ethiopic Amete Mihret and Amete Alem, Buddhist, historical Thai Buddhist solar, Japanese, Minguo, French Republican, Roman, Mayan Long Count with Tzolk’in/Haab Calendar Round, Badíʿ, International Fixed, World, Positivist, Revised Julian, Byzantine Anno Mundi, Egyptian civil (Nabonassar era), traditional Armenian, and Sweden's historical 1700–1753 civil calendar including 30 February 1712.

The applet can coexist with Cinnamon's stock Calendar applet and installs no project-owned daemon, polling service or autostart entry. System Settings is optional at runtime. Common's installed `temporal-v3` provider marker advertises that the writer/UI is currently installed, while the validated persisted policy is the durable system authority and survives writer removal. With neither a valid saved policy nor an installed richer authority, Calendar follows Mint/Cinnamon. Calendar monitors both the policy document and writer-marker boundary live and needs no background daemon.

Calendar prefers the MB Corpo family names defined by the pinned Common typography contract when those typefaces are already available on the system, but it does not redistribute proprietary MB Corpo font binaries. Cinnamon's normal font fallback is therefore a required runtime path, not an error condition. Generated Cinnamon CSS remains regression-checked against Common's typography roles. Configuration stays in Cinnamon's xlet-settings renderer; About is a package-owned native GTK 3 helper launched out-of-process from Cinnamon. GTK is therefore confined to that presentation edge, while Calendar carries no project-owned Python settings host.

Location-dependent clocks do not silently assume Greenwich. They show `N/A LOC` until geographic location is configured in **System Settings → Date & Time**. On a Mint-only installation with no persisted Infiltrator temporal policy those extended clock modes are not selected, so Calendar remains on Cinnamon's conventional clock path.

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

| Owner | Responsibility |
| --- | --- |
| Cinnamon frontend | Panel/popup presentation, settings and desktop lifecycle |
| Calendar core and adapters | Civil coordinates, grid construction, event semantics, native presentation boundary and timer orchestration |
| Pinned Common library | Shared primitives, temporal policy, resource discovery, ABI validation and loading |
| Independently installed Common resources | Calendar conversion, clock rendering, astronomical models and next-visible-tick boundaries; ICU/CLDR where delegated |

The source tree is grouped by responsibility:

```text
src/
├── core/       Canonical coordinates, resource adapters and event logic
├── adapters/   GObject, GVariant, main-loop and native integration
├── app/        Project identity, version API and About helper
├── cinnamon/   Cinnamon JavaScript runtime and settings
├── i18n/       Gettext sources and translations
├── abi/        Exported-library version map
└── vendor/     Pinned Libraries submodule
```

Chronology, continuation and astronomy source, tests, builds and packages live in the independent provider repositories. Common owns the resource ABI, discovery, validation, loading and policy integration, while retaining its original source as a reference. Cinnamon owns desktop integration. Provider-specific model and authority choices, including ICU/CLDR delegation and owned lunar tables, are documented in each provider repository.

The installed `libcalendar.so.0` ABI and established `Calendar` compatibility identifiers remain stable runtime contracts even though the visible product is **Calendar** and the Debian package is `calendar`.

Detailed representation, event ownership, failure, security and release contracts are maintained in [Architecture](docs/ARCHITECTURE.md) and [Portability](docs/PORTABILITY.md).

## Correctness and model scope

Historical calendars and clocks are deterministic computational models rather than claims about every historical locality or observational practice. Islamic results are not local crescent observations; solar and seasonal clocks use defined astronomical approximations and may be unavailable at polar latitudes. Japanese modern-era boundaries are resource-owned for stable cross-ICU behaviour, while locale-sensitive era wording and other explicitly delegated presentation data continue to use ICU/CLDR.

The evidence hierarchy, exact model authorities, continuation rules, astronomical references and admission requirements for new calendars/clocks are maintained in [MODELS.md](docs/MODELS.md).

Unavailable, unsupported or physically undefined states are represented explicitly. Calendar does not invent Greenwich, extrapolate nonexistent solar crossings or substitute plausible values for failed chronology.

## Build and test

Install the development dependencies on Debian 13, Linux Mint 22 or Ubuntu 24.04:

```bash
sudo apt install build-essential clang debhelper gettext gobject-introspection gjs \
    gir1.2-glib-2.0-dev libglib2.0-dev libgtk-3-dev libicu-dev nodejs pkg-config \
    python3 shellcheck git
```

Clone recursively because Calendar pins Libraries:

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

Each Calendar release publishes **two** application artifacts:

| Artifact | Purpose |
| --- | --- |
| `calendar_<version>_amd64.deb` | Generic amd64 Debian application package |
| `calendar-<version>-local-folder.run` | Verified local hardware-native PGO source builder |

Each optional clock or calendar has its own `.deb`, native source `.run` and source ZIP. These are provider release assets and remain installed when Calendar is removed. InfiltratorOS installs all 54 providers through their aggressive native source installers.

Install the generic packages with:

```bash
sudo apt install ./calendar_<version>_amd64.deb
# Optional examples from the configured Infiltrator repository:
sudo apt install infiltrator-calendar-gregorian infiltrator-clock-standard-24
```

Or use the native PGO builder:

```bash
chmod +x calendar-<version>-local-folder.run
./calendar-<version>-local-folder.run
```

The About dialog reports the compiled build profile using Libraries's canonical label, matching System Monitor: repository builds show **Generic / APT package**, while local `.run` builds show **Native / local machine compile**.

The `.run` performs a two-pass local build. Pass 1 instruments Calendar and the pinned Common library with GCC profile generation, runs Calendar's automated tests plus a representative clock/calendar/event workload on the actual machine, then pass 2 rebuilds with the measured profile using `-fprofile-use -fprofile-correction -fprofile-partial-training`. The resulting Debian-managed package is therefore tuned to the local CPU rather than being a generic repository build. The correctness-preserving profile deliberately does not use `-Ofast` or `-ffast-math`.

After installation, add **Calendar** from **System Settings → Applets**.

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

Calendar is GPL-3.0-or-later. The pinned Libraries dependency uses the same licence. The complete project licence is in `LICENSE`; Debian packaging provenance is recorded in `debian/copyright`.

The packaged Calendar icon follows the non-automotive Infiltrator desktop family: graphite tile with canonical `#00ADEF` linework. One verified source asset feeds Cinnamon, the desktop icon theme and Mint app-install metadata.

Calendar owns no calendar/clock implementation source or module release jobs.
Normal host Gregorian dates and standard system time work without optional
modules. Additional system capabilities are discovered from the resource folders
through Common. Missing custom selections stay visibly unavailable.

## Independent temporal packages

All 31 calendar and 23 clock implementations are released by their own repositories. Calendar suggests their individual packages and tests verified snapshots from those releases, including provider-owned translation catalogues. Common supplies the neutral public ABI and preserves its original source; it no longer builds or publishes the combined resource package. Calendar removal leaves the provider packages installed.
