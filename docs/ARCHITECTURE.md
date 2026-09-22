<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

Calendar separates platform-neutral chronology, clock, astronomy and event semantics from Cinnamon presentation, GObject/GVariant/main-loop adapters and reusable Common mechanisms. That separation is a correctness boundary: desktop code should present completed Calendar-owned state rather than becoming a second implementation of chronology or timekeeping rules.

## First-principles design

Calendar begins with the behaviour that chronology, astronomy, historical evidence and time standards actually justify rather than treating an existing desktop calendar as the specification.

First principles does not mean reimplementing every dependency. Cinnamon, GLib, ICU/CLDR where still required, and Infiltratr Common are appropriate where their documented contracts are the strongest engineering choice. The project owns the semantics that are specific to Calendar and delegates only the mechanisms or data for which another component is authoritative.

A dependency is therefore chosen deliberately. Convenience, convention or similarity to another calendar application is not enough to transfer ownership of product behaviour.

## Structure

```text
Cinnamon presentation / settings / panel lifecycle
                    ↓
         JavaScript/native boundary
                    ↓
GObject / GVariant / CalendarServer / main-loop adapters
                    ↓
platform-neutral Calendar domain contracts
                    ↓
chronology / clocks / astronomy / event semantics

ICU / CLDR                    Infiltratr Common 1.19.23
     ↓                                  ↓
locale/calendar authority     reusable checked arithmetic /
where explicitly delegated    formatting / timing / loading /
                               UTF-8 / allocation primitives
```

The source tree reflects those responsibilities:

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

## Contracts and ownership

The native core owns Calendar-specific chronology models, continuation rules, alternative clocks, astronomical calculations and event semantics. It deliberately uses GLib foundational facilities such as fixed-width types, strings, containers and civil-time helpers, but remains independent of GObject presentation facades, GVariant transport schemas, Cinnamon actors and desktop lifecycle APIs.

Cinnamon owns panel integration, settings presentation, lifecycle and CalendarServer transport mechanics. Calendar now owns deterministic arithmetic/navigation for the fixed-rule calendar families it can validate directly. ICU/CLDR remains authoritative only for the remaining astronomical/lunisolar providers and locale-sensitive formatting that Calendar still explicitly delegates. Common owns genuinely generic primitives whose contract is reusable across the software family. Common is also the single authority for system clock-mode identifiers, presentation names and capability metadata; Calendar binds those Common catalogue entries to Calendar-owned formatter and next-boundary implementations rather than duplicating the metadata. Calendar's Cinnamon surface projects the pinned Common design JSON into toolkit-specific CSS under generated, regression-checked markers rather than carrying independent typography values. Configuration uses Cinnamon's own xlet-settings renderer and About uses Cinnamon/St, eliminating Calendar-owned Python/GTK presentation shims.

Ownership is visible at API boundaries. Caller-owned values, returned heap data, snapshots, compatibility ABI objects and adapter-owned platform state should not be inferred from accidental implementation detail.

Cross-layer shortcuts require a documented reason. GObject/GVariant, toolkit or desktop-platform objects should not leak into platform-neutral chronology merely because doing so is convenient.

## Date and time representation

Calendar uses more than one time representation because the domains are different:

- `CalendarPlusDate` is a proleptic-Gregorian civil coordinate used as the stable date boundary;
- date-only native algorithms use an integral midnight-based Julian Day Number;
- astronomical Julian Date is a separate fractional quantity whose conventional boundary is noon;
- native absolute instants use signed Unix microseconds from 1970-01-01T00:00:00Z;
- CalendarServer fields documented as Unix seconds remain seconds until their single normalisation boundary.

Internal proleptic arithmetic may use astronomical year numbering, including year 0, when a provider requires it. Presentation is responsible for historical era labels.

Representation conversion belongs at explicit boundaries. A helper that silently changes epoch, unit or day-boundary convention violates the architecture because it makes a plausible-looking chronology error difficult to detect.

## Event model and snapshot consistency

CalendarServer all-day end points enter as exclusive following-midnight values and are normalised exactly once into Calendar's inclusive final-instant contract. Timed event end points are inclusive under the native event model.

The native event index is single-owner-thread state. Consumers obtain detached deep-copy snapshots rather than sharing mutable index internals. Snapshot revisions are opaque equality tokens; callers compare them for change detection rather than treating them as timestamps or ordering metadata.

This design keeps CalendarServer lifecycle and transport behaviour out of portable event semantics while allowing the Cinnamon presentation layer to refresh safely from coherent Calendar-owned state.

## Location and astronomical state

Geographic coordinates are degrees, latitude north-positive and longitude east-positive. Location-dependent clocks require an explicitly configured location.

Astronomical modes may depend on solar events that do not exist for a particular place/date under the selected model. The domain contract therefore includes unavailable states. Presentation must preserve that outcome rather than converting it to Greenwich, a cached crossing or an extrapolated polar result.

Detailed model authority and continuation policy are defined in [MODELS.md](MODELS.md).

## Arithmetic and allocation safety

Checked Common arithmetic is used when overflow means a chronology, astronomy or allocation result cannot be represented safely. Saturating arithmetic is used only where clamping is the explicit Calendar policy.

Externally derived allocation sizes are validated before allocation. Signed-overflow undefined behaviour is not an accepted failure mode. Arithmetic safety is part of chronology correctness because overflow can otherwise create a valid-looking but wrong date.

## Failure model

Unsupported, malformed, unavailable or physically undefined states remain explicit across layers. A numeric or textual result that merely looks plausible is not an acceptable substitute.

A failure in one optional model should not corrupt independent Calendar behaviour. Locale fallback, event transport loss, astronomical unavailability and unsupported calendar ranges are represented at the narrowest owning boundary so presentation can report them honestly.

Malformed external data is rejected or normalised once at the adapter/domain boundary. The portable core should not need to understand transport-specific error recovery.

## Localisation and text ownership

Project-owned strings use gettext. ICU/CLDR and Cinnamon remain authoritative for locale-sensitive calendar names, weekend policy and conventional date/time presentation where Calendar explicitly delegates those responsibilities.

UTF-8, locale and translation handling must not become hidden chronology policy. A locale difference may affect presentation or ICU-owned data; it must not silently change a Calendar-owned mathematical rule.

## Common

`src/vendor/infiltratr-common` is pinned to one exact Common release commit. Common is the authoritative home for reusable mechanisms; Calendar owns chronology, historical continuation and astronomy policy that is genuinely specific to this product.

If Calendar contains a stronger implementation of a capability that is fundamentally generic, the correct direction is to improve Common so its generic contract preserves that correctness, performance and resilience. Once Common is at least as strong, Calendar should consume it and remove the duplicate implementation.

Do not weaken specialised chronology merely to increase reuse. Equally, do not preserve a private generic helper indefinitely when its advantages can be incorporated into Common. Common 1.19.23 is therefore consumed wherever its public contract is genuinely stronger or more general: checked/saturating arithmetic, timing, strings, dynamic loading, project metadata and the shared design/typography contract. Calendar does not manufacture artificial callers for unrelated Common APIs such as POSIX hardware readers, byte order helpers or graphics surfaces.

## Native ABI and compatibility

The installed `libcalendar-plus.so.0` and versioned symbol map form a runtime stability contract for Calendar itself. They are not currently advertised as a separately supported third-party C SDK.

The typelib, About helper, applet UUID, gettext domain and established `CalendarPlus` / `CALENDAR_PLUS_*` identifiers are compatibility identities. The user-facing application is Calendar and the Debian package is `infiltrator-calendar`; compatibility naming is intentionally allowed to differ from product naming.

A supported third-party SDK would require installed headers, package metadata and its own explicit compatibility policy rather than silently promoting internal headers into a public contract.

## Security and trust model

Calendar consumes event data, D-Bus/GVariant payloads, locale data, settings and platform-service responses. Those inputs are treated as external even when they originate from the local desktop.

Parsing and conversion boundaries validate structure, size and range before Calendar-owned state is updated. Untrusted or malformed event data must not be able to turn an adapter failure into memory corruption or fabricated chronology.

The applet installs no project-owned daemon, polling service or autostart entry. CalendarServer reconnect behaviour and Cinnamon lifecycle integration remain within the desktop session rather than creating a parallel background service.

## Verification and assurance

Correctness is enforced at several levels rather than by one end-to-end test. Portable chronology/property tests exercise deterministic models and boundaries; clock tests cover exact transition behaviour; ABI tests protect the native compatibility surface; JavaScript/settings tests protect the Cinnamon boundary; package and reproducibility tests protect release construction.

Live Cinnamon behaviour is a separate integration boundary. The repository uses a Cinnamon smoke tool for installed-session verification and an upstream-drift workflow to detect changes in the stock Cinnamon integration surface that require deliberate review.

The exact release revision must satisfy the required build, sanitizer, static-analysis, coverage, ABI, packaging, reproducibility and release gates. A skipped live-environment check is reported as skipped rather than treated as proof.

## Build and release contract

Calendar pins the exact Common version and commit used by the build. Release assets are derived from the exact tested `main` revision.

Published tags and assets are immutable identities. Debian packaging, install/purge qualification and central-repository lifecycle testing are part of the release contract rather than optional post-release checks.

## Specialist documents

- [MODELS.md](MODELS.md) — chronology, clock and astronomy evidence, provenance, continuation rules and model failure semantics.
- [PORTABILITY.md](PORTABILITY.md) — language, platform, representation, locale and compatibility boundaries.
- [VALIDATION.md](VALIDATION.md) — what evidence is automated, manual or environment-dependent.
