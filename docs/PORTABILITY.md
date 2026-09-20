<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Portability

Calendar is portable at its domain boundaries rather than by pretending that the current Cinnamon implementation is platform-neutral. Chronology, clock, astronomy and event semantics should remain testable without a running desktop. The core may use GLib foundational types, strings, containers and civil-time helpers; GObject/GVariant facades, Cinnamon presentation, desktop lifecycle and other platform integration stay behind explicit seams. ICU/CLDR is delegated only where its documented calendar/locale contract remains authoritative; fixed-rule arithmetic that Calendar can validate directly is owned locally.

## Language and interface policy

Project-owned native code may use C or C++ according to which gives the stronger implementation for the component. Neither language is preferred by policy. Plain C data/functions, procedural C++, value-oriented C++ and object-oriented C++ are implementation techniques rather than project identities.

Cinnamon integration necessarily uses JavaScript because that is the desktop platform boundary. Introducing another language or runtime requires a concrete technical advantage that the existing C/C++/JavaScript architecture cannot reasonably provide.

Portable/domain interfaces must not expose Cinnamon actors, GJS objects, GObject presentation facades, GVariant transport schemas, GTK widgets, file descriptors, D-Bus proxies or other platform-owned handles. Adapters translate those details into Calendar-owned values.

## Platform boundary

The current product targets Cinnamon. Cinnamon owns panel actors, settings presentation, desktop lifecycle and CalendarServer integration mechanics. Calendar owns the chronology, clock, astronomy and event semantics those surfaces expose.

A future desktop or platform backend should be able to implement the same application-facing contracts without reproducing Cinnamon internals. Portability does not require choosing a lowest-common-denominator implementation: platform code should use the strongest native mechanism available as long as platform details do not leak into portable contracts.

## Date and time representation

Representation is part of correctness:

- date-only native algorithms use an integral midnight-based Julian Day Number;
- astronomical Julian Date is a distinct fractional quantity whose conventional boundary is noon;
- native absolute instants use signed Unix microseconds from 1970-01-01T00:00:00Z;
- CalendarServer fields documented as Unix seconds remain seconds until their single normalisation boundary;
- internal proleptic arithmetic may use astronomical year numbering, including year 0, even when presentation uses historical era labels.

Conversions between these representations must occur at explicit boundaries. Unit changes hidden inside helper functions are treated as defects because they make chronology failures difficult to audit.

## Civil, elapsed and astronomical time

Civil time and elapsed time are different domains. Wall-clock presentation, dates, time zones and persisted civil timestamps use civil-time semantics. Durations, retry cadence and performance-sensitive elapsed intervals use monotonic clocks where elapsed time is the requirement.

Astronomical clocks derive from a defined model and configured coordinates. Latitude is north-positive and longitude east-positive, both in degrees. Location-dependent modes report unavailable when location or a required solar crossing cannot be established; they do not silently substitute Greenwich or fabricate polar events.

## Integer and memory assumptions

External sizes, delegated-library allocation counts and arithmetic that can exceed a representation are checked before allocation or conversion. Checked Common arithmetic is used when overflow means the result is invalid. Saturating arithmetic is used only when clamping is the documented Calendar policy.

Binary or ABI-facing structures must use explicit-width types where width is part of the contract. Code must not depend on undefined signed overflow, accidental host alignment or implementation-defined narrowing.

## Unicode, locale and translation boundaries

Project-owned interface strings use gettext. ICU/CLDR and Cinnamon remain authoritative for the locale-sensitive material they own, including calendar names, weekend policy and conventional date/time presentation.

UTF-8 crossing native/JavaScript/platform boundaries must be validated or handled through APIs whose contract already guarantees valid text. Locale-sensitive data must not be converted into hard-coded English policy merely to simplify portable code.

## ABI and compatibility

The installed `libcalendar-plus.so.0` ABI, GObject Introspection identity, gettext domain, applet UUID and established runtime identifiers are compatibility contracts even though the visible product is Calendar and the Debian package is `infiltrator-calendar`.

Portability work must distinguish public/user-facing naming from compatibility identity. Renaming an internal identifier is not an improvement if it needlessly breaks settings, ABI consumers or upgrades.

## Dependency boundary

Calendar may depend on Cinnamon, GLib, pinned Infiltratr Common and the shrinking ICU/CLDR boundary where their documented contracts are the strongest engineering choice. A dependency must not silently become the owner of Calendar-specific policy.

Generic mechanisms belong in Common when their contract is genuinely reusable and at least as strong as the best local implementation. Calendar-specific chronology, continuation rules and astronomical policy remain in Calendar.

## Review test

A portable change should preserve these properties:

- portable/domain code does not gain Cinnamon- or platform-native objects;
- date/time units and epochs remain explicit at every boundary;
- civil, elapsed and astronomical time are not conflated;
- arithmetic cannot silently overflow into plausible-looking output;
- unsupported or physically undefined states remain representable;
- locale and translation ownership remains explicit;
- compatibility identifiers change only through a deliberate migration;
- another frontend could consume the domain contracts without copying Cinnamon internals.
