# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calendar. Detailed commit-by-commit history remains in Git.

## Unreleased

No unreleased changes.

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
