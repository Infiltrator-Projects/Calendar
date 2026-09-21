<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Qualification

This ledger records exact-source environment-dependent evidence for Calendar. It complements [VALIDATION.md](VALIDATION.md): Validation defines the evidence classes and release rules, while this file records observations that cannot be inferred from compilation or headless tests alone.

## Evidence classes

- **Automated conformance** — warnings-as-errors GCC/Clang builds, native tests, property/adversarial tests, ABI checks, JavaScript runtime checks, sanitizers, static analysis, coverage, reproducibility and package lifecycle gates.
- **Installed Cinnamon integration** — the exact source/package revision is installed into a real Cinnamon session and passes runtime-source hashes, typelib/native-provider loading, enabled-applet discovery, panel/popup interaction and session-log checks through `tools/live-cinnamon-ci-smoke.sh` and `tools/cinnamon-smoke.sh`.
- **Location-dependent presentation** — a real Cinnamon session exercises one or more location-dependent astronomical clocks with explicit coordinates, including an unavailable solar-event case where practical.
- **Release publication** — immutable tag/assets derive from the exact tested `main` revision and the central package repository advertises that version.

These are distinct evidence classes. A hosted build does not become Cinnamon-session proof, and a mocked or deterministic astronomy test does not become observation of the installed UI.

## Current development boundary

The current `main` at version 1.0.47 contains the documented correctness, repository-hardening, package-owned Software Manager icon and MB Corpo typography work, the forensic event-cache/reload/retry/package-migration/proleptic-Gregorian/sanitizer repairs, and the Common 1.19.20 foundation containing shared build-profile labels, atomic dynamic symbol binding, checked/timing primitives, the shared temporal presentation contract and the unified System/Day/Night plus typography contract. Cinnamon typography, structural geometry, Day/Night palette projection and bundled-font provenance are checked against the pinned Common design source; settings now use Cinnamon's own renderer and About uses Cinnamon/St, removing Calendar's direct Python/GTK runtime presentation dependencies; the popup theme class is owned through Cinnamon's custom-style API so orientation changes cannot silently discard the selected mode. JavaScript local-midnight and instant-equality mechanics remain owned once by the Cinnamon runtime support layer. This ledger deliberately makes no claim that automated conformance substitutes for a separately recorded manual Cinnamon/location qualification.

The About surface reports the canonical build profile from the compiled native library through Infiltratr Common, so a repository package identifies itself as `Generic / APT package` and a local `.run` build identifies itself as `Native / local machine compile`. This mirrors System Monitor's build-identity contract.

Temporal authority is explicitly guarded: Common 1.19.20 supplies the canonical POSIX policy path and installed `temporal-v3` provider capability. Calendar caches one effective snapshot and monitors both the policy directory and provider directory, so atomic saves, provider installation and provider removal cannot mix fields from different policy generations. Provider absence, missing policy or invalid policy resolves to Mint/Cinnamon; provider plus valid policy resolves to Infiltrator System Settings. The JavaScript runtime matrix separately covers 12/24-hour, date visibility, seconds and vertical-panel combinations, while the month view follows Cinnamon's first-day-of-week helper and the event layer follows the operating-system timezone.

Automated CI remains authoritative for automated conformance of each commit. A live Cinnamon step that is unavailable is shown as skipped, not converted into a pass.

## Recording a live qualification

Record a live qualification only after the exact commit has been exercised. The entry should contain:

| Field | Required evidence |
| --- | --- |
| Date | Calendar date of the observation |
| Exact source | Full Git commit SHA |
| Calendar version | Source/package version |
| OS | Distribution and release |
| Cinnamon | Exact Cinnamon version |
| Session | X11/Wayland where relevant |
| Locale/time zone | Locale and active time zone |
| Installed integration | Result of the live Cinnamon smoke path |
| Location model | Coordinates used, selected astronomical mode(s), and observed unavailable case if exercised |
| Result | Pass/fail with the smallest useful note |

Do not record a newer documentation-only commit as inheriting live evidence for changed executable source. If executable behaviour changes after a qualification, the affected evidence class must be rerun before the newer source is described as qualified.

## Release evidence rule

A release may rely on automated evidence from the exact release SHA and separately recorded live evidence where the environment supports it. Documentation must state which class was actually observed. Absence of a live environment is an evidence boundary, not evidence of failure and not evidence of success.

Historical release tags and GitHub Actions logs remain the immutable source for exact automated run details; this ledger exists to preserve the environment-dependent evidence that would otherwise be easy to overstate or forget.
