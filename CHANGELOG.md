## 1.0.105 - 2026-10-17

## 1.0.116 — 2026-10-18

- Finish extraction of all thirty calendars and twenty-three explicit clocks.
- Make Calendar's chronology and clock dispatch resource-only; remove duplicated
  application algorithms and the final Common clock-face fallbacks.
- Preserve missing resource selections through the desktop policy boundary and
  show unavailable states instead of silently selecting Gregorian or WallClock.
- Make every date surface, including Gregorian and agenda headers, use the
  selected resource. Preserve failed navigation without changing caller output.
- Keep ICU runtime dependencies and translations with the resource package.
- Install both matching native packages and qualify the complete payload and
  application-removal lifecycle.
- Pin Common 1.19.64.


- Use canonical alpha product and package identities without migration aliases.

# Changelog

Calendar maintains one release-history authority: [`debian/changelog`](debian/changelog).

That file owns the versioned release record used by package validation and by the release handoff that generates GitHub Release notes. `CHANGELOG.md` deliberately does not duplicate those entries; maintaining a second hand-written release ledger had no stronger contract and could drift independently.

Exact historical source and published assets remain preserved by immutable Git tags and GitHub Releases. The detailed pre-consolidation narrative that previously lived in this file remains available in Git history.
