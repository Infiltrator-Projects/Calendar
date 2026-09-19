# Architecture

## Purpose

Calendar is a native Cinnamon clock and calendar whose chronology, astronomy, alternative time systems and event behaviour are owned by the project rather than inherited blindly from a stock desktop applet.

## System decomposition

- portable native chronology and astronomy core
- Cinnamon presentation and settings layer
- CalendarServer/event integration
- platform and GLib adapters
- pinned Common dependency
- chronology, property, ABI and runtime regression tests

## Ownership boundaries

Calendar owns chronology models, continuation rules, alternative clocks, astronomy and its product behaviour. Cinnamon owns desktop integration; ICU/CLDR is used where its locale data is authoritative; Common supplies only product-neutral primitives.

The architectural rule is that mechanisms may come from an operating system, toolkit, shared first-party library or documented external API, but product semantics remain with their owning repository. Dependencies are accepted because their contract is useful, not as a substitute for understanding the behaviour being exposed to users.

## Source of truth

Implementation and tests define executable behaviour. This document defines module ownership and dependency direction. Specialist documents refine particular subsystems but must not create a competing architecture.

## Change rules

Cross-layer shortcuts require a documented reason. Platform handles and toolkit objects should not leak into portable/domain contracts. Failure states must remain representable across boundaries rather than being converted into plausible-looking values.

## Specialist documents

- No additional architecture document is required for the baseline; subsystem details belong beside the subsystem when they become necessary.
