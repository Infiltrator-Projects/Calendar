<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security

## Supported source

Security fixes target current `main` and, where appropriate, the latest published release. Older releases should not be assumed to receive backports.

## Reporting

Do not open a public issue for a vulnerability that could expose user data, local system information, package/installer integrity, release infrastructure or other sensitive material.

Use GitHub private vulnerability reporting when available. Otherwise contact `infiltratr@yandex.com` with the subject `Calendar security report`.

Include the affected version/commit, operating system and Cinnamon version, impact, reproduction steps and relevant sanitised logs. Remove unrelated credentials or personal data.

## Security-sensitive boundaries

Reports are especially relevant for:

- memory-safety and integer/bounds faults;
- CalendarServer, D-Bus, GVariant or event-data parsing;
- native ABI/library boundaries;
- local file and settings handling;
- dependency pinning, package integrity and release automation;
- behaviour reachable through untrusted calendar/event data.

## Response and validation

Security defects are correctness defects. Reproduce the issue, add a regression test where practical, fix the underlying contract and validate the affected boundary. Parser defects require malformed-input evidence; ABI defects require ABI/runtime evidence; packaging defects require package/release evidence.

## Disclosure

Public disclosure should follow a fix or clear mitigation so affected and corrected source identities are known. Testing must be limited to systems and data the reporter is authorised to use.