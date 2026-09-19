# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calendar. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

## 1.0.22 - 2026-09-19

- Advance the exact shared dependency to Infiltratr Common 1.19.5.
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
