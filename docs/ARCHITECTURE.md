<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Architecture

Calendar separates independently installed chronology, clock scheduling and astronomy resources from application event semantics, Cinnamon presentation, GObject/GVariant/main-loop adapters and reusable Common mechanisms. Desktop code presents completed resource results and application event state without implementing chronology or timekeeping rules.

The versioned `calendar_format_date_v1` export is a narrow cross-application boundary over the native chronology engine. It accepts a calendar ID, a Gregorian civil date and `short`/`full` presentation, writes bounded UTF-8 into caller storage, and leaves output unchanged on failure. Common's POSIX adapter owns discovery and validation of that entry point. This route does not construct GObject calendar types or transfer chronology into Common.

## First-principles design

Calendar begins with the behaviour that chronology, astronomy, historical evidence and time standards actually justify rather than treating an existing desktop calendar as the specification.

First principles does not mean reimplementing every dependency. Cinnamon, GLib, ICU/CLDR where still required, and Libraries are appropriate where their documented contracts are the strongest engineering choice. The project owns the semantics that are specific to Calendar and delegates only the mechanisms or data for which another component is authoritative.

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

ICU / CLDR                    Libraries (exact pinned release)
     ↓                                  ↓
locale/calendar authority     reusable checked arithmetic /
where explicitly delegated    formatting / timing / loading /
                               UTF-8 / allocation primitives
```

The source tree reflects those responsibilities:

```text
src/
├── core/       Calendar grid/event logic and resource ABI adapters
├── adapters/   GObject, GVariant, main-loop and native integration
├── app/        Project identity, version API and About helper
├── cinnamon/   Cinnamon composition, presentation and narrow desktop adapters
├── i18n/       Gettext sources and translations
├── abi/        Exported-library version map
└── vendor/     Pinned Libraries submodule
```

## Contracts and ownership

The native application core owns grid construction, canonical civil-date coordinates and event semantics. Independently installed Calendar Resources own chronology models, continuation rules, clock faces and astronomical calculations. It deliberately uses GLib foundational facilities such as fixed-width types, strings, containers and civil-time helpers, but remains independent of GObject presentation facades, GVariant transport schemas, Cinnamon actors and desktop lifecycle APIs.

Cinnamon owns panel integration, settings presentation, lifecycle and CalendarServer transport mechanics, but those platform mechanics are split into narrow owners. `desktopIntegration.js` owns Gio desktop settings, global keybindings, suspend/resume compatibility, theme/panel observation and application launching. `applet.js` remains the composition root and receives only semantic callbacks from that adapter. `civilDate.js` owns the date-only JavaScript boundary, including valid civil dates that have no representable local instant. `calendarEventProjection.js` owns visible-range/event-colour projection, leaving `calendar.js` focused on native grid navigation and actor presentation.

Live applet and desktop settings terminate at the composition root; a dedicated temporal-policy resolver decodes and validates the native effective snapshot, and lower presentation modules receive only resolved scalar policy rather than settings objects. A dedicated JavaScript native-boundary decoder is the sole presentation-side owner of positional GVariant output layouts, while raw CalendarServer input tuples are interpreted only by the native event adapter. All thirty chronology providers and twenty-three explicit clocks are installed resources. Common owns discovery, both executable provider ABIs, validation, loading and the policy bridge. Calendar's registry contains only adapters for validated resources. No compiled application provider or clock-face formatter may replace an unavailable resource. Each clock module owns both its rendered text and next-visible-tick boundary. The desktop preserves selected resource IDs when modules are missing and reports an unavailable state. Gregorian date presentation follows the same provider boundary; OS-following standard time alone remains a platform policy choice. Calendar's Cinnamon surface projects the pinned Common design JSON into toolkit-specific CSS under generated, regression-checked markers rather than carrying independent typography values. Configuration uses Cinnamon's own xlet-settings renderer. About is a package-owned native GTK 3 helper launched out-of-process by the Cinnamon desktop-integration boundary; GTK therefore remains a presentation/platform-edge dependency and does not leak into Calendar's portable core or native library. Calendar owns no Python/GTK settings shim.

The small Cinnamon bootstrap that locates `runtimeSupport.js` must execute before shared JavaScript code can itself be loaded. Its unavoidable textual copies therefore have one canonical generator/checker in `tools/update-runtime-loader.py`; independent hand-maintained loader variants are not permitted.

The calendar algorithms, astronomical rules and their private engine live in `Libraries/resources/temporal`. Only Common's independently installed resource modules link that engine; the Calendar application links no resource algorithms. Common owns the provider ABIs and discovery, while each module owns its chronology or clock semantics. Shared Gregorian/JDN coordinate helpers supply no selectable chronology or clock fallback.

The independently released `infiltrator-calendar-resources` package owns ICU runtime requirements and resource translations. Calendar's native installer installs only Calendar; removing a consumer does not take ownership of the system temporal resources.

Ownership is visible at API boundaries. Caller-owned values, returned heap data, snapshots, compatibility ABI objects and adapter-owned platform state should not be inferred from accidental implementation detail.

Cross-layer shortcuts require a documented reason. GObject/GVariant, toolkit or desktop-platform objects should not leak into platform-neutral chronology merely because doing so is convenient.

## Date and time representation

Calendar uses more than one time representation because the domains are different:

- `CalendarDate` is a proleptic-Gregorian civil coordinate used as the stable date boundary;
- date-only native algorithms use an integral midnight-based Julian Day Number;
- astronomical Julian Date is a separate fractional quantity whose conventional boundary is noon;
- clock and astronomy absolute instants use signed Unix microseconds from 1970-01-01T00:00:00Z;
- the established event source/index ABI uses signed Unix seconds, matching CalendarServer's seconds-based contract and normalising all-day exclusive end points once inside that event domain.

Internal proleptic arithmetic may use astronomical year numbering, including year 0, when a provider requires it. Presentation is responsible for historical era labels.

Representation conversion belongs at explicit boundaries. A helper that silently changes epoch, unit or day-boundary convention violates the architecture because it makes a plausible-looking chronology error difficult to detect. The complete cross-platform representation policy is maintained in [PORTABILITY.md](PORTABILITY.md).

## Event model and snapshot consistency

CalendarServer all-day end points enter as exclusive following-midnight values and are normalised exactly once into Calendar's inclusive final-instant contract. Timed event end points are inclusive under the native event model.

The native event index is single-owner-thread state. Identity lookup is hash-indexed, while two non-owning sorted temporal indexes bound interval candidates by local start/end day. Long events still remain one record; Calendar does not expand them per day. Consumers obtain detached deep-copy snapshots rather than sharing mutable index internals. Snapshot revisions are opaque equality tokens; callers compare them for change detection rather than treating them as timestamps or ordering metadata.

This design keeps CalendarServer lifecycle and transport behaviour out of portable event semantics while allowing the Cinnamon presentation layer to refresh safely from coherent Calendar-owned state. Cinnamon event transport isolates CalendarServer discovery, reconnect and range-call mechanics behind a dedicated transport object. EventsManager consumes that narrow port and owns event/cache semantics, while visible-range request generation, admission, retry and queued-force transitions remain in a dedicated state object. CalendarServer callbacks therefore cannot mutate an accidental constellation of controller flags.

Civil visible ranges are constructed from actual local date boundaries rather than assuming every day is 86,400 seconds. DST transitions, skipped midnights and wholly skipped dates remain explicit boundary cases in both implementation and behavioural tests.

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

Calendar providers resolve by Calendar's stable IDs. Common validates executable resource metadata per provider; a missing or invalid implementation is unavailable and does not disable unrelated installed resources. Release builds still pin an exact Common revision and validate catalogue agreement as release evidence rather than making runtime chronology depend on array-order or global catalogue completeness.

Malformed external data is rejected or normalised once at the adapter/domain boundary. The portable core should not need to understand transport-specific error recovery.

## Localisation and text ownership

Project-owned strings use gettext. ICU/CLDR and Cinnamon remain authoritative for locale-sensitive calendar names, weekend policy and conventional date/time presentation where Calendar explicitly delegates those responsibilities.

UTF-8, locale and translation handling must not become hidden chronology policy. A locale difference may affect presentation or ICU-owned data; it must not silently change a Calendar-owned mathematical rule.

## Common

`src/vendor/infiltratr-common` is pinned to one exact Common release commit. Common is the authoritative home for reusable mechanisms; the independently installed Calendar Resources own chronology, historical continuation and astronomy implementations. The application owns no duplicate selectable calendar or clock algorithm.

If Calendar contains a stronger implementation of a capability that is fundamentally generic, the correct direction is to improve Common so its generic contract preserves that correctness, performance and resilience. Once Common is at least as strong, Calendar should consume it and remove the duplicate implementation.

Do not weaken specialised chronology merely to increase reuse. Equally, do not preserve a private generic helper indefinitely when its advantages can be incorporated into Common. The exact Common release pinned in the Makefile is therefore consumed wherever its public contract is genuinely stronger or more general: checked/saturating arithmetic, timing, strings, dynamic loading, project metadata and the shared design/typography contract. Calendar does not manufacture artificial callers for unrelated Common APIs such as POSIX hardware readers, byte order helpers or graphics surfaces.

## Native ABI and compatibility

The installed `libcalendar.so.0` and versioned symbol map form a runtime stability contract for Calendar itself. They are not currently advertised as a separately supported third-party C SDK.

The typelib, About helper, applet UUID, gettext domain and established `Calendar` / `CALENDAR_*` identifiers are compatibility identities. The user-facing application is Calendar and the Debian package is `calendar`; compatibility naming is intentionally allowed to differ from product naming.

A supported third-party SDK would require installed headers, package metadata and its own explicit compatibility policy rather than silently promoting internal headers into a public contract.

## Security and trust model

Calendar consumes event data, D-Bus/GVariant payloads, locale data, settings and platform-service responses. Those inputs are treated as external even when they originate from the local desktop.

Parsing and conversion boundaries validate structure, size and range before Calendar-owned state is updated. Untrusted or malformed event data must not be able to turn an adapter failure into memory corruption or fabricated chronology.

The applet installs no project-owned daemon, polling service or autostart entry. CalendarServer reconnect behaviour and Cinnamon lifecycle integration remain within the desktop session rather than creating a parallel background service.

## Verification and assurance

Correctness is enforced at several levels rather than by one end-to-end test. Portable chronology/property tests exercise deterministic models and boundaries; clock tests cover exact transition behaviour; ABI tests protect the native compatibility surface; JavaScript/settings tests protect the Cinnamon boundary; package and reproducibility tests protect release construction.

Live Cinnamon behaviour is a separate integration boundary. The repository uses a Cinnamon smoke tool for installed-session verification and an upstream-drift workflow to detect changes in the stock Cinnamon integration surface that require deliberate review. Ordinary development runs may report a live probe as unavailable, but a main-branch version transition is not publishable unless the dedicated real Cinnamon qualification is available and succeeds.

The exact release revision must satisfy the required build, sanitizer, static-analysis, coverage, ABI, packaging, reproducibility and release gates. A skipped environment-dependent check is reported as skipped rather than treated as proof, and release-critical live Cinnamon qualification cannot be satisfied by a skip.

## Build and release contract


The repository-root `VERSION` file is the platform-neutral authority for Calendar's source/release identity. The Makefile consumes it directly; Cinnamon metadata and Debian changelog versions are required projections for those platforms and are checked against it before release. Build or release tools must not rediscover the version by parsing unrelated Makefile syntax.

Calendar pins the exact Common version and commit used by the build. Release assets are derived from the exact tested `main` revision. Release intent is the current source version being newer than the latest published GitHub release, not a one-push edge or commit-message prose. A failed publication therefore remains eligible on later green `main` revisions until that version is actually published. One JSON handoff owns the release version, source revision and qualified artifact identities; the publisher consumes those exact bytes without rebuilding them. Draft release identity is tracked by release ID because GitHub creates the protected tag only when publication completes, and that tag is then verified against the qualified revision.

Published tags and assets are immutable identities. Debian packaging, install/purge qualification and central-repository lifecycle testing are part of the release contract rather than optional post-release checks.

## Specialist documents

- [MODELS.md](MODELS.md) — chronology, clock and astronomy evidence, provenance, continuation rules and model failure semantics.
- [PORTABILITY.md](PORTABILITY.md) — language, platform, representation, locale and compatibility boundaries.
- [VALIDATION.md](VALIDATION.md) — what evidence is automated, manual or environment-dependent.
