# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calendar. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

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
