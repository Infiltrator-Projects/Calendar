# Calendar branch retirement — 2026-10-12

Calendar 1.0.91 consolidates active development onto a single supported branch: `main`.

At retirement time there were zero open pull requests. The non-main refs below were development, hardening, release-preparation, diagnostic, or superseded implementation lineages. Some were strict historical ancestors of later work; some were divergent abandoned alternatives. Divergent refs were intentionally **not** merged over the newer 1.0.91 implementation merely to make the graph look linear. Their exact heads were recorded before deletion, and the guarded retirement workflow refused deletion if any ref had moved.

The retirement workflow also verified that `main` remained at the guarded release-candidate SHA while removing the old refs. After retirement the remote branch list contained only `main`.

| Retired branch | Recorded head |
| --- | --- |
| `diagnostic/calendar-1.0.91-live` | `f682b742b797dba5252ed6910cf7f3ce4478b737` |
| `fix/calendar-1.0.89-reproducibility` | `3018a8cb19706ec69dc7c46e377ac96f825d0226` |
| `hal/calendar-1.0.88-harden-top-three` | `15a9a54a4798b7774ceac96de851750f115320cb` |
| `hal/calendar-1.0.89-quality-hardening` | `48ce07786e8628997c7dc0b6924fbba92d4ac1b8` |
| `hal/calendar-quality-hardening-1.0.89` | `eec8d3a4879c637397419fbf0afc6f02441e4511` |
| `hal/calendar-runtime-hardening` | `0ae05aff62906812d992300d20e0db945a209af3` |
| `hal/decouple-brittle-boundaries` | `56bf65c10a96bf894c53ab3cf116e0e253fa7e2f` |
| `hal/forensic-hardening-1.0.90` | `3be04960ae8d133ec4b3ef6296e2f2891b5fd363` |
| `hal/forensic-hardening-1.0.91` | `291afd570533e58d7b94591d48914c93a4c4a5bf` |
| `hal/forensic-hardening-ci3` | `2ef25f796534f9be8b244e69524b604b8e6c91a5` |
| `hal/forensic-hardening-manifest` | `0931948ffd5e63d6850d9e60a9e52b587195bc4e` |
| `hal/forensic-quality-hardening-2026-10-11` | `e015b4ed5d1334d8d1ec92b51c8ba5ae678c2852` |
| `hal/quality-hardening-1.0.89` | `04bb5e72552b83cd7aaf240c2995eef4a981991c` |
| `hardening/1.0.91-architecture-cleanup` | `49256c739043b84f52a32a22f361150339c66b5e` |
| `hardening/1.0.91-ci-fixes` | `1571e80d34ea6e15723310cb2f0533083e348ec0` |
| `refactor/reduce-brittle-boundaries-2-clean` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `refactor/reduce-brittle-boundaries-2` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-brittle-boundaries` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-candidate` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-final` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-final-pr` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-integration` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-merge-candidate` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-pr` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-pr-final` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-pr-ready` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-review` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.87-squash` | `1ee08514273b9315cb0094d1da59deabcc613242` |
| `release/calendar-1.0.89-quality-hardening` | `5036868f61bcdb57c1734157bb8b3c082d38f2a5` |
| `release/1.0.90` | `fba85e98e796341d877e69990d39226f50f2bf71` |

The temporary maintenance branch and retirement workflow deleted themselves after the guarded cleanup. No operational cleanup branch or workflow is retained in the product tree.

For 1.0.91, release qualification additionally uses the exact CI-built Debian artifact for BigBedroom Cinnamon qualification. The package is loaded in a fresh isolated Cinnamon process with the candidate native library and typelib visible from process start, preventing a long-lived desktop process from satisfying the gate with an older already-loaded GI namespace. The release publisher then independently rebuilds and byte-compares its package and local-folder installer against the CI-qualified artifacts before publishing them.
