<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Qualification

This ledger records exact-source environment-dependent evidence for Calendar. It complements [VALIDATION.md](VALIDATION.md): Validation defines the evidence classes and release rules, while this file records observations that cannot be inferred from compilation or headless tests alone.

## Evidence classes

- **Automated conformance** — warnings-as-errors GCC/Clang builds, native tests, property/adversarial tests, ABI checks, JavaScript runtime checks, sanitizers, static analysis, coverage, reproducibility and package lifecycle gates.
- **Installed Cinnamon integration** — the exact source/package revision is installed into a real Cinnamon session and passes runtime-source hashes, typelib/native-provider loading, enabled-applet discovery, panel/popup interaction and session-log checks through `tools/live-cinnamon-ci-smoke.sh` and `tools/cinnamon-smoke.sh`.
- **Location-dependent presentation** — a real Cinnamon session exercises one or more location-dependent astronomical clocks with explicit coordinates, including an unavailable solar-event case where practical.
- **Release publication** — immutable tag/assets derive from the exact tested `main` revision and the central package repository advertises that version.

These are distinct evidence classes. A hosted build does not become Cinnamon-session proof, and a mocked or deterministic astronomy test does not become observation of the installed UI.

## Qualification boundaries

The exact commit's CI run is the authority for automated builds, sanitizers, ABI, reproducibility, package installation and isolated Cinnamon integration. `tools/live-cinnamon-ci-smoke.sh` records the candidate revision and environment (distribution, Cinnamon, session type, locale and time zone). CI logs are immutable release evidence; a skipped gate remains unqualified.

The native temporal-policy source accepts only coherent Common-validated snapshots. A valid persisted `temporal-v3` policy survives removal of its System Settings writer; without valid persisted policy, Calendar follows Mint/Cinnamon. Clock format and tick boundaries come from independently installed resources, never an application-local calculation. These contracts are defined in [Architecture](ARCHITECTURE.md) and covered by their native/JavaScript tests.

Packaging and appearance have separate owners: Common supplies design tokens but no proprietary font binaries; Cinnamon supplies the settings renderer and system font fallback; Calendar supplies the out-of-process GTK About helper. The native build profile identifies generic APT versus locally profiled builds. See [Architecture](ARCHITECTURE.md) and [Portability](PORTABILITY.md) for these contracts rather than maintaining parallel descriptions here.

The table below records **manual observations only**. It must not inherit a pass from a newer automated release, historical tests or a different machine. Manual qualification of every calendar/clock, astronomical location and desktop appearance is not implied by green CI.

## Recorded live qualification

### 2026-09-22 — Calendar 1.0.51

| Field | Observed evidence |
| --- | --- |
| Date | 2026-09-22 |
| Exact source | `d7823a6da45ab5613761ea5a921dd1d160e58048` |
| Calendar version | 1.0.51 |
| OS | Linux Mint 22.3 (Zena), kernel 7.0.0-31-generic |
| Cinnamon | 6.6.9 |
| Session | X11 |
| Locale/time zone | `en_AU.UTF-8`; `Australia/Melbourne` |
| Installed integration | PASS — exact-version installed hashes, native typelib/provider loading, enabled applet discovery, update/reset/resume paths and programmatic popup open/close all passed. |
| Location model | Not separately exercised by this run; no location-dependent presentation claim is made. |
| Result | PASS — 15 local qualification stages passed, 0 failed, 0 skipped. |

This entry applies only to the exact 1.0.51 source above. Later source revisions require their own executable qualification where their changes affect an environment-dependent evidence class.

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
