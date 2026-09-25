# Changelog

## 1.0.64 - 2026-09-25

- Align the Cinnamon month heading with the 27 px publisher page-hero scale while retaining Common's brand/display face.
- Preserve Cinnamon-native layout, temporal behaviour, calendar engines, dependencies and Common APIs unchanged.

## 1.0.63 - 2026-09-25

- Give Calendar popup menu actions the suite-wide 30 px desktop control height while keeping their existing 6 px compact radius.
- Preserve Cinnamon-native layout, temporal behaviour, calendar engines, dependencies and Common APIs unchanged.


## 1.0.62 - 2026-09-25

- Give Calendar popup menu items Common's 6 px compact radius so Cinnamon actions share the suite's compact control geometry.
- Keep Cinnamon-native layout, temporal behaviour, calendar engines, dependencies and Common APIs unchanged.


## 1.0.61 - 2026-09-25

- Align the Cinnamon event pane's internal vertical rhythm with Common's 10 px control spacing while retaining the 12 px card radius.
- Preserve Cinnamon-native layout mechanics, temporal behaviour, calendar engines, dependencies and Common APIs unchanged.

## 1.0.60 - 2026-09-25

- Align the Cinnamon Today/home control with the suite-wide 30 px desktop control height while preserving its existing Common 10 px control radius.
- Preserve Cinnamon-native layout, temporal behaviour, calendar engines, dependencies and Common APIs unchanged.

## 1.0.59 - 2026-09-25

- Align calendar weekday headings with the suite-wide 10 px compact label scale.
- Preserve Cinnamon-native layout, temporal behaviour, calendar engines, dependencies and Common APIs unchanged.


## 1.0.59 - 2026-09-24

- Stabilise the Cinnamon panel clock so ordinary clock ticks can grow the latched allocation but never shrink it; width is reset only when a genuine layout input changes.
- Request tabular numeral glyphs for the panel clock so seconds, decimal time, binary fields and other numeric clock modes retain constant digit advances when the active font supports them.
- Preserve the existing MB Corpo typography, temporal policy, specialised clock rendering and Cinnamon preferred-width ownership model.

## 1.0.57 - 2026-09-24

- Align Cinnamon weekday headings with the suite-wide 11 px supporting-text scale.
- Preserve Calendar behaviour, Cinnamon-native structure, chronology, event integration and dependencies unchanged.


## 1.0.56 - 2026-09-24

- Align the selected-date heading in the Cinnamon event pane with the suite-wide 16 px section-heading scale.
- Preserve Calendar behaviour, Cinnamon-native structure, chronology, event integration and dependencies unchanged.

## 1.0.55 - 2026-09-24

- Align the Cinnamon month heading with the 28 px publisher title scale.
- Preserve Calendar behaviour, Cinnamon-native structure and dependencies unchanged.


## 1.0.54 - 2026-09-24

- Pin the exact released Infiltratr Common 1.19.25 clock-presentation authority.
- Route every specialised Calendar clock rendering request through Common so Calendar, System Settings and System Monitor produce the same clock text for the same instant and policy.
- Remove Calendar's duplicate specialised clock formatter callbacks and retain only exact next-boundary scheduling needed by the Cinnamon panel.
- Keep System Settings temporal-v3 authority, OS/Cinnamon conventional-time fallback, Calendar chronology and timer behaviour unchanged.

## 1.0.53 - 2026-09-24

- Give the Cinnamon month heading the Common brand/display typography role while retaining the normal Common UI face for dates, weekdays and event text.
- Keep calendar, clock, event, Cinnamon integration and dependency behaviour unchanged.


## 1.0.52 - 2026-09-22

- Replace the panel clock's direct `St.Bin.min_width` mutation with a registered latched-width container, preserving two-glyph hysteresis while avoiding the Clutter allocation warning fixed upstream by Cinnamon.
- Advance the reviewed Cinnamon integration baseline to `40d419f2c0951ace9f69155700993f611f22eca1` after confirming the watched change is limited to that width-latch correction.
- Pin the exact released Infiltratr Common 1.19.24 commit `748e089ae175329471d4cf375522c44081371bd5`; the release hardens existing graphics operations without adding or removing Common APIs and Calendar has no graphics-surface callers.
- Record the exact Calendar 1.0.51 live Cinnamon qualification from Linux Mint 22.3/Cinnamon 6.6.9 while keeping location-dependent presentation explicitly unclaimed.

This changelog records user-visible, compatibility, architecture and validation changes for Calendar. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

## 1.0.51 - 2026-09-22

- Complete the chronology-family modularisation after a second forensic cohesion pass.
- Move Japanese imperial-era, Coptic/Ethiopic and Indian National arithmetic out of the central coordinator into private family modules without changing the public arithmetic ABI or chronology behaviour.
- Keep family-owned era/year mapping beside each family conversion so generic navigation cannot acquire a competing chronology interpretation.
- Reduce the coordinator to cross-family dispatch, simple Gregorian-derived era mapping and generic month/year navigation while retaining the exact Common 1.19.23 foundation.

## 1.0.50 - 2026-09-22

- Split the large arithmetic implementation along proven chronology-family boundaries without changing the public arithmetic API or calendar behaviour.
- Keep Islamic civil/tabular and Umm al-Qura rules together, Hebrew cycle/postponement/navigation rules together, and Persian leap/correction rules together in private core modules.
- Reduce the arithmetic coordinator to cross-family dispatch, era mapping and generic navigation while preserving the existing reference, boundary, sanitizer and architecture test surface.
- Document the new private ownership boundary and retain Common 1.19.23 as the exact shared foundation.

## 1.0.49 - 2026-09-22

- Finish the forensic Common → Calendar ownership pass against Common 1.19.23.
- Derive temporal monitor parent/basename identity from Common-returned paths, fixing provider-marker overrides and removing hardcoded `infiltrator`/`temporal-v3` monitor assumptions.
- Remove Calendar's duplicate local civil-day phase algorithm and consume Common's promoted exact temporal primitive directly.
- Let the Common-backed native clock registry classify native modes, remove the duplicate JavaScript coordinate clamp and keep coordinate validation/normalisation in the native engine.
- Enumerate the Common-backed calendar catalogue in PGO/property/round-trip loops while retaining one explicit catalogue test as an independent supported-set oracle.
- Replace residual adapter string equality and POSIX path manipulation with Common primitives and add regression guards for the tightened ownership boundary.

## 1.0.48 - 2026-09-22

- Complete the forensic source, test, packaging, comment and maintained-documentation review for Calendar's declared Linux/Cinnamon product scope.
- Stop redistributing proprietary MB Corpo font binaries; retain Common's preferred typography family names and require normal Cinnamon/system fallback when those fonts are not installed.
- Make package-input, local-installer and release-artifact contracts reject any accidental return of the proprietary font payload.
- Document the arithmetic-engine invariants, temporal-authority snapshot/lifecycle contract and the maintenance-complete boundary without adding narrational comments to straightforward code.
- Preserve the existing runtime behaviour and qualify the completed source through GCC, Clang, ASan/UBSan, coverage, clang-tidy, package-install, reproducibility and release-artifact gates.

## 1.0.47 - 2026-09-21

- Finish the Common → Calendar ownership pass without changing Common: use Common's deterministic ASCII classifiers for event-colour and locale-region parsing.
- Make Infiltratr Common the sole runtime owner of the 30 calendar IDs and English presentation names; Calendar now retains only chronology/backend implementation policy and resolves catalogue metadata through Common.
- Remove the About helper's stale local build-profile aliases and derive the displayed author identity from Common's InfiltratrProjectInfo contract.
- Strengthen portable and release-model regression coverage so duplicated Common-owned metadata and ASCII helpers cannot silently return.

## 1.0.46 - 2026-09-21

- Make the Cinnamon panel date follow System Settings' selected calendar instead of leaving Gregorian date text in the bottom-right panel clock.
- Keep Cinnamon WallClock responsible for native Gregorian date formatting only; when Positivist or another non-Gregorian calendar is selected, Calendar renders the selected date through its own chronology engine and composes it with the active conventional or specialised clock.
- Add regression coverage proving non-Gregorian panel dates do not leak Gregorian WallClock text and compose correctly with both conventional and native clock modes.

## 1.0.45 - 2026-09-21

- Keep the agenda pane structurally present while CalendarServer is still activating so the popup opens at its final width instead of expanding several seconds later.
- Separate event-pane presentation readiness from event-transport readiness; selected-date presentation now updates while the asynchronous CalendarServer connection is pending.
- Make Cinnamon About dialogs one-shot, synchronously release the popup-menu grab before opening them, and clear Calendar's dialog references on destruction so reopening About cannot target stale modal state or strand pointer input.
- Rework the About card into a centred native Cinnamon layout with stable icon, identity, build, temporal-authority, author, description and licence hierarchy.
- Add permanent regression checks for startup agenda geometry and the safe About modal lifecycle.

## 1.0.44 - 2026-09-21

- Make Common 1.19.20 the runtime authority for native clock-mode identifiers, presentation names and capability metadata while Calendar retains its formatter and next-boundary implementations.
- Add executable and static regression checks that compare Calendar's provider bindings against Common's complete temporal catalogue so metadata drift cannot silently return.
- Refresh current architecture and generated design commentary to the actual pinned Common 1.19.20 foundation.

## 1.0.43 - 2026-09-21

- Decouple Calendar's verified GitHub release from the central APT repository's independently scheduled tokenless pull refresh.
- Let a newer publish run supersede an obsolete in-progress publisher instead of holding a completed release open.
- Preserve strict exact-main, tag and immutable-asset verification for Calendar itself while making external repository propagation asynchronous.

## 1.0.42 - 2026-09-21

- Replace the generic `system-settings` executable/PATH probe with Common 1.19.20's validated installed `temporal-v3` provider capability.
- Resolve one cached effective temporal snapshot, eliminating repeated policy reads and guaranteeing internally consistent clock/calendar/seconds/location state.
- Monitor the XDG policy directory and installed provider directory so atomic policy replacement and live provider install/removal are reflected without a daemon.
- Launch System Settings through its package-owned desktop application identity and retain Cinnamon Date & Time as the deterministic fallback.
- Show the live temporal authority in About and add precedence tests covering stale policy, no policy, valid richer policy, invalid policy fallback and runtime provider removal.
- Require Common's provider marker contents to identify the exact System Settings provider/contract instead of trusting pathname existence alone, and consume Common's stricter coordinate/path persistence contracts.
- Route the About temporal-authority label and provider names through Calendar's gettext domain instead of leaving new user-facing authority text as raw English.

## 1.0.41 - 2026-09-21

- Harden Mint-only operation so a stale `~/.config/infiltrator/presentation.conf` cannot override Cinnamon after System Settings has been uninstalled.
- Treat installed `system-settings` plus a valid temporal policy as the richer authority; otherwise resolve to conventional Gregorian/Cinnamon behaviour.
- Lock the Mint fallback clock matrix in executable JavaScript tests: 12/24-hour mode, date visibility, seconds and vertical-panel formatting.
- Document the fallback boundary explicitly, including Cinnamon first-day-of-week, locale and timezone ownership.

## 1.0.40 - 2026-09-21

- Bring Calendar's About build identity up to the System Monitor standard.
- Expose the compiled Calendar build profile through the native introspected ABI and render Infiltratr Common's canonical shared label in the Cinnamon/St About card.
- Generic repository packages now identify themselves as `Generic / APT package`; local hardware-native PGO installations identify themselves as `Native / local machine compile`.
- Add ABI, native, GJS and release-contract coverage so future About changes cannot silently lose the real build identity.

## 1.0.39 - 2026-09-21

- Upgrade the released local `.run` installer from static native tuning alone to a two-pass hardware-native PGO pipeline.
- Instrument Calendar and the pinned Infiltratr Common library with GCC profile generation, train representative calendar-grid/navigation, clock/tick, event-timing and automated-test paths on the actual machine, then rebuild with `-fprofile-use -fprofile-correction -fprofile-partial-training`.
- Retain `-O3 -march=native -mtune=native -flto=auto` while deliberately excluding `-Ofast` and `-ffast-math` so chronology and astronomical calculations keep their existing correctness semantics.
- Identify locally trained packages as `+nativepgo1` and record the PGO-trained profile in installed BUILD-INFO.

## 1.0.38 - 2026-09-21

- Make Infiltrator System Settings an optional enrichment rather than a runtime requirement: a valid shared temporal policy enables the extended clock/calendar/location behaviour, while a missing or invalid policy falls back to native Mint/Cinnamon behaviour.
- Mirror Cinnamon's `clock-show-seconds` preference when no Infiltrator temporal policy exists, alongside the existing native 12/24-hour, locale and date-format path.
- Open Infiltrator System Settings from Calendar when installed, otherwise fall back to Cinnamon's native `cinnamon-settings calendar` Date & Time panel.
- Watch Cinnamon's seconds preference live so a Mint-only Calendar replacement responds immediately without maintaining a duplicate Calendar-local setting.

## 1.0.37 - 2026-09-20

- Make System Settings the sole temporal authority for Calendar.
- Remove Calendar's clock-mode, seconds, geographic-location, calendar-system and custom date/time format controls.
- Remove the obsolete Follow System Settings switch; Calendar now follows the system temporal policy unconditionally.
- Remove the retired secondary-calendar model and secondary-date rendering completely.
- Move Calendar to Common 1.19.18 temporal policy v3 with one authoritative calendar.
- Replace the old primary/secondary native policy getters with one package-internal system-calendar getter.
- Keep Calendar-specific settings only: appearance, events, week numbers and keyboard shortcut.
- Preserve the branded Cinnamon-native About repair introduced in 1.0.36.

## 1.0.36 - 2026-09-20

- Repair the Calendar About surface regression introduced when the GTK About dialog was replaced in 1.0.32.
- Replace the bare Cinnamon message box with a branded Cinnamon/St About card using the installed Calendar icon.
- Restore clear product hierarchy: Calendar name, version, author, concise description, GPL identity and legal information.
- Keep Website and Close actions native to Cinnamon while retaining the no-GTK-runtime architecture.
- Add scoped MB Corpo About styling and update release tests so CI rejects a return to the generic MessageDialogContent layout.

## 1.0.35 - 2026-09-20

- Make System Settings the default temporal authority for Calendar.
- Add a single **Follow System Settings** switch covering clock mode, primary calendar, secondary calendar, seconds and geographic location together.
- Read the shared version-2 temporal policy through Common 1.19.16 and react to policy-file changes while Calendar is running.
- Retain Calendar's complete 21-clock and 30-calendar settings as local overrides when Follow System Settings is disabled.
- Open Infiltrator System Settings from Calendar's Date and Time Settings menu entry.
- Publish the complete system temporal policy getters in ABI 2.3.

## 1.0.34 - 2026-09-20

- Advance Calendar to Infiltratr Common 1.19.14 and consume Common's shared decimal-10 formatter instead of retaining a second decimal clock implementation.
- Make Calendar's existing Standard clock mode follow the system-wide Infiltrator temporal presentation policy while preserving Cinnamon's normal 12/24-hour behaviour when no extended policy is selected.
- Monitor the shared temporal policy so an open Calendar can resynchronise its panel clock when System Settings changes the clock profile.
- Preserve every explicit Calendar clock mode as an application-local override of the system presentation policy.

## 1.0.33 - 2026-09-20

- Move Gregorian, Hebrew, Persian, Buddhist, Minguo, Japanese, Indian National, Coptic, Ethiopic, Islamic civil/tabular, Umm al-Qura and Ethiopic Amete Alem calendar arithmetic and navigation out of ICU into Calendar-owned deterministic engines.
- Replace ICU weekend queries with a compact Calendar-owned CLDR weekData projection for locale-aware workday styling.
- Keep ICU only for the remaining astronomical/lunisolar providers and locale-sensitive calendar formatting while those final contracts are migrated without reducing chronology or localisation fidelity.

## 1.0.32 - 2026-09-20

- Remove Calendar's project-owned Python/GTK settings host and delegate settings presentation to Cinnamon's native `xlet-settings` path while retaining the same schema and persisted values.
- Move the user-facing About surface into Cinnamon/St and reduce the installed `calendar-plus-about` helper to a metadata probe for package qualification.
- Drop Calendar's direct runtime `python3` and GTK 3 package dependencies; Cinnamon/GJS remains the platform UI boundary and ICU remains the calendar-data dependency.
- Remove the build-time ripgrep dependency by using the platform grep implementation for the sole source-identity guard.

## 1.0.31 - 2026-09-20

- Keep calendar-day selection available when events are hidden or CalendarServer is unavailable; the date grid no longer depends on event transport to select a day.
- Use a roving keyboard-focus target so only the selected day participates in the Tab sequence while arrow, Home/End and Page keys retain spatial navigation inside the grid.
- Align year navigation with the standard date-grid convention: Page Up/Down moves one native calendar period and Shift+Page Up/Down moves one native calendar year.
- Add JavaScript and static regression coverage for the corrected keyboard and selection semantics.

## 1.0.30 - 2026-09-20

- Standardise Calendar artwork on the canonical non-automotive Infiltrator icon family: graphite tile and `#00ADEF` linework, with the same verified source bytes feeding Cinnamon, hicolor and Mint app-install metadata.
- Debounce custom clock and tooltip format application by 500 ms so partially typed strftime tokens are not pushed through WallClock on every keystroke.
- Preserve exact user-authored tooltip casing.
- Install an offline strftime reference and route the format-help action to it.
- In Follow system mode, defer to Cinnamon when a HighContrast theme is active; explicit Day/Night continue to use Common 1.19.10.
- Expose today, selection, event presence, event time and event status semantically to assistive technology instead of relying on colour alone.
- Add a 10,000-event native regression ceiling to catch accidental quadratic event-store behaviour while keeping Calendar a clock/calendar rather than a scheduling suite.

## 1.0.29 - 2026-09-20

- Undo the 1.0.28 settings-host layout/card experiment that produced the oversized grey slabs visible in Night mode.
- Restore the cleaner Cinnamon-native settings geometry from 1.0.27 while retaining Common 1.19.10 typography and System/Day/Night colours.
- Keep Calendar styling focused on titlebar, controls, selection and switches instead of overriding Cinnamon's page/section layout.

## 1.0.28 - 2026-09-20

- Rework the external Cinnamon settings window so it uses the same layered Common 1.19.10 visual language as System Monitor instead of leaving Cinnamon's default grey list rows on a black canvas.
- Project Common panel, card, surface, input, border, hover, selection and accent roles into settings frames, rows, toolbar, menus, tooltips and controls.
- Style switch tracks and sliders explicitly so the host GTK theme cannot leak a mismatched orange/default control treatment into Night mode.
- Replace Cinnamon's oversized 80-pixel settings-page margins and 30-pixel section gaps with Common's shared screen-padding and section-spacing metrics.

## 1.0.27 - 2026-09-20

- Install the user-approved Calendar artwork as the canonical application icon.
- Keep that one canonical PNG as the source for Cinnamon, the native About dialog, hicolor desktop identity and Linux Mint Software Manager alias.
- Pin the verified 256×256 PNG asset by SHA-256 so substituted artwork cannot silently return.

## 1.0.26 - 2026-09-20

- Restore the intended Calendar neon artwork from the previously staged canonical Git blob; the 1.0.15 repair had pinned a different image instead.
- Make the restored artwork the single source for the Cinnamon applet, hicolor desktop identity, Linux Mint Software Manager alias and native About dialog.
- Re-pin release validation to the restored artwork SHA-256 so the wrong image cannot silently return.

## 1.0.25 - 2026-09-20

- Advance the exact shared dependency to published Infiltratr Common 1.19.10, whose Night palette now mirrors the complete Linux MBLINK reference face.
- Fix Day/Night selection by keeping Calendar's popup identity and effective theme in Cinnamon's authoritative custom-style class instead of adding a class that setCustomStyleClass() immediately discarded.
- Preserve the selected popup theme across orientation changes, where Cinnamon rebuilds the menu actor's class list.
- Expand Calendar's generated Day/Night projection to use Common's card, border, heading, summary, status-border, accent and hover roles rather than flattening Night into a few greys.
- Use the MBLINK/Common blue accent for the selected/current calendar day while retaining Common's contrasting accent foreground.
- Make the external Cinnamon settings window follow the same System/Day/Night preference live, including system GTK-theme changes, instead of applying typography alone.
- Add regression guards for the real popup class lifecycle and settings-host theme binding.

## 1.0.24 - 2026-09-20

- Complete the Calendar-side adoption of Infiltratr Common 1.19.8 design contracts by consuming Common structural metrics as well as palette and typography data.
- Apply Common panel/card/control/small radii and section spacing to Calendar's scoped Cinnamon stylesheet through the existing generated-token pipeline.
- Apply Common's control radius to the Cinnamon settings host and consume the native `infiltratr_design_metrics()` API in the GTK About helper.
- Strengthen release-model regression coverage so Common-owned structural values cannot silently drift back into Calendar-owned constants.
- Keep calendar semantics, Cinnamon selectors, GTK integration and platform theme detection Calendar-owned.

## 1.0.23 - 2026-09-19

- Advance the exact shared dependency to the published Infiltratr Common 1.19.8 release.
- Consume Common's native typography contract in the GTK About helper instead of hardcoding MB Corpo family names and role weights.
- Remove redundant inline Cinnamon typography styles so applet presentation has one generated stylesheet projection of the pinned Common design contract.
- Extend the existing Common design generator to keep both Cinnamon CSS and the separate GTK settings host typography aligned with Common.
- Validate Calendar's bundled MB Corpo archive, filenames and extracted file hashes against Common's immutable typography provenance rather than maintaining an unchecked parallel truth.
- Extend release-model and settings regressions so Common design ownership, public-header coverage and the absence of private typography overrides cannot silently drift.
- Preserve Calendar-owned chronology, astronomy, event semantics, Cinnamon selectors and ICU version-probing policy instead of forcing unrelated Common APIs into product code.

## 1.0.22 - 2026-09-19

- Advance the exact shared dependency to Infiltratr Common 1.19.6, retaining the 1.19.5 build-profile and dynamic-binding consolidation while consuming the unified System/Day/Night appearance contract.
- Replace Calendar's private build-profile display mapping with Common's canonical project-family label contract.
- Replace the About dialog's private required-symbol loading loop with Common's atomic dynamic-symbol table binder while retaining ICU-specific version probing locally.
- Consolidate duplicated local-midnight and instant-equality JavaScript helpers into the shared Cinnamon runtime support module.
- Use Common's NULL-safe string equality for Gregorian ICU-provider selection instead of a second GLib equality path.
- Add regression guards for the Common ownership boundary so these generic mechanics do not drift back into Calendar.

## 1.0.21 - 2026-09-19

- Bundle the canonical MB Corpo font archive from MBLINK directly in Calendar source and verify the archive and all three extracted TTF files by SHA-256.
- Install MB Corpo A Condensed Regular, S Bold and S Regular into `/usr/share/fonts/truetype/infiltrator-calendar` from both generic and hardware-native packages.
- Extend generic-release, local-native and release-model checks so a Calendar package cannot pass while omitting its required typography assets.
- Keep font preparation inside the Makefile rather than introducing a separate font-helper program.

## 1.0.20 - 2026-09-19

- Correct the agenda render-cache identity so changing the selected day or 12/24-hour preference cannot reuse stale rows from another presentation state.
- Make forced queued reloads request authoritative CalendarServer data, and make same-range requests retry after transient failures without stale asynchronous completions corrupting newer request state.
- Cover the retired `calendar-plus` and `cinnamon-calendar` package identities during migration, including the supplied 3.6.0 development package line.
- Make ICU conversion, formatting and navigation consistently proleptic Gregorian before the October 1582 cutover while preserving the separately named Julian and historical providers.
- Propagate ASan/UBSan instrumentation into the pinned Infiltratr Common archive and verify that instrumentation in the sanitizer gate.
- Add regression coverage for the five confirmed functional defects, the sanitizer coverage gap and legacy package migration paths.

## 1.0.19 - 2026-09-19

- Make the Calendar Debian package itself install the `infiltrator-calendar` Linux Mint Software Manager icon alias.
- Remove any architectural dependency on a shared app-install icon helper; Calendar remains the sole owner and publisher of its artwork.
- Extend release-model, generic-package and local-native-package validation so the package cannot ship without its Software Manager icon alias.

## 1.0.18 - 2026-09-19

- Align the canonical documentation structure with the implemented architecture, including dedicated model and portability contracts.
- Preserve invalid configured astronomical coordinates as unavailable instead of silently normalising non-finite values to Greenwich.
- Include the canonical documentation set in local source payloads and Debian copyright coverage, and update release-model regression checks accordingly.
- Documentation baseline aligned with the Infiltrator project family.
- Add exact-source live Cinnamon/location qualification recording without overstating skipped environment checks.
- Add Calendar-specific GitHub bug and feature request forms.
- Centralise provider range and continuation contracts for all 30 calendars and 18 native time modes.
- Align project-owned source copyright headers with the maintained 2016-2026 project span.

## Recording policy

Record additions, removals, behavioural fixes, compatibility changes, dependency changes that affect consumers, and material validation/release changes. Pure refactoring needs an entry only when it changes maintenance or portability expectations.

## Historical releases

Existing Git tags and GitHub Releases remain the authoritative identity for exact historical source and release assets. Do not reconstruct detailed historical claims here without evidence from those immutable records.
