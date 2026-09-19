<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to Calendar

Calendar combines a portable native chronology/event core, thin adapters and a Cinnamon JavaScript frontend. Contributions must preserve those boundaries, keep behaviour verifiable and avoid unnecessary repository complexity.

## Engineering rules

- Keep portable calendar, clock and event logic in `src/core/`.
- Keep GLib, GVariant and platform integration in `src/adapters/`.
- Keep project identity and the About helper in `src/app/`.
- Keep Cinnamon runtime code and settings in `src/cinnamon/`.
- Reuse the pinned Infiltratr Common API when it is the correct shared abstraction; improve Common first if Calendar has the stronger generic implementation.
- Treat unsupported or ambiguous behaviour as unavailable rather than inventing results.
- Preserve the published runtime ABI unless a deliberate ABI change is part of the work.
- Add deterministic regression coverage for behavioural, parser, lifecycle, timing, ABI or packaging changes.
- Do not add parallel documentation where the canonical documents already own the subject.

## Language and dependency policy

C and C++ are preferred for project-owned native code; neither is preferred over the other merely by language. Cinnamon integration necessarily uses its JavaScript platform boundary. ICU/CLDR and Cinnamon are used where their documented contracts are authoritative; they do not replace Calendar's ownership of chronology, continuation rules or project-specific behaviour.

## Build and validation

Clone recursively because Calendar pins Infiltratr Common:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Calendar.git
cd Calendar
make check
```

For a release-equivalent local pass:

```bash
make release-check
```

Changes should remain warning-clean and preserve architecture, ABI, translation, runtime-integrity, packaging and reproducibility gates. Upstream Cinnamon drift is reviewed for compatibility rather than copied mechanically.

## Documentation and comments

Read `docs/README.md` for document authority. Architecture belongs in `docs/ARCHITECTURE.md`; rationale in `docs/DESIGN.md`; durable choices in `docs/DECISIONS.md`; direction in `docs/ROADMAP.md`; validation evidence in `docs/VALIDATION.md`; chronology/clock/astronomy provenance and continuation rules in `docs/MODELS.md`; and cross-platform representation, language and compatibility boundaries in `docs/PORTABILITY.md`.

Comments should capture information expensive to reconstruct: units, validity ranges, historical/astronomical authority, invariants, ownership, ABI constraints and deliberate deviations. Do not narrate obvious syntax.

## Repository discipline

`main` is the authoritative development and release branch. Keep commits focused. Published tags/releases are immutable identities.

Participation standards remain in [CODE_OF_CONDUCT.md](.github/CODE_OF_CONDUCT.md).

## Licence

Contributions are accepted under GPL-3.0-or-later unless explicitly agreed otherwise beforehand.