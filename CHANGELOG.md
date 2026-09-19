# Changelog

This changelog records user-visible, compatibility, architecture and validation changes for Calendar. Detailed commit-by-commit history remains in Git.

## Unreleased

- Align the canonical documentation structure with the implemented architecture, including dedicated model and portability contracts.
- Preserve invalid configured astronomical coordinates as unavailable instead of silently normalising non-finite values to Greenwich.
- Include the canonical documentation set in local source payloads and Debian copyright coverage, and update release-model regression checks accordingly.

- Documentation baseline aligned with the Infiltrator project family.

## Recording policy

Record additions, removals, behavioural fixes, compatibility changes, dependency changes that affect consumers, and material validation/release changes. Pure refactoring needs an entry only when it changes maintenance or portability expectations.

## Historical releases

Existing Git tags and GitHub Releases remain the authoritative identity for exact historical source and release assets. Do not reconstruct detailed historical claims here without evidence from those immutable records.
