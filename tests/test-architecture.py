#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Enforce Calendar's source ownership and platform boundary contracts.

The checks deliberately reason about declared ownership, include closure and
source-tree membership rather than implementation spelling. A refactor may
change APIs used inside an adapter without weakening the architectural rule
that portable core code cannot reach desktop/platform implementation details.
"""

from __future__ import annotations

import re
from collections import deque
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAKEFILE = ROOT / "Makefile"
LOCAL_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
C_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
C_LINE_COMMENT_RE = re.compile(r"//.*?$", re.MULTILINE)


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def code(relative: str) -> str:
    source = C_BLOCK_COMMENT_RE.sub("", read(relative))
    return C_LINE_COMMENT_RE.sub("", source)


def make_list(name: str) -> tuple[str, ...]:
    """Return a simple backslash-continued Make variable as path tokens."""
    lines = MAKEFILE.read_text(encoding="utf-8").splitlines()
    prefix = f"{name} :="
    collecting = False
    values: list[str] = []

    for raw in lines:
        if not collecting:
            if not raw.startswith(prefix):
                continue
            collecting = True
            raw = raw[len(prefix):]

        text = raw.strip()
        continued = text.endswith("\\")
        if continued:
            text = text[:-1].rstrip()
        if text:
            values.extend(text.split())
        if not continued:
            break

    assert collecting, f"missing Make variable {name}"
    assert values, f"empty Make variable {name}"
    return tuple(values)


def resolve_local_include(relative: str, include: str) -> str | None:
    """Resolve quoted includes that belong to this source tree."""
    owner = ROOT / relative
    candidates = (
        owner.parent / include,
        ROOT / include,
        ROOT / "src/core" / include,
        ROOT / "src/adapters" / include,
        ROOT / "src/app" / include,
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve().relative_to(ROOT.resolve()).as_posix()
    return None


def include_closure(roots: tuple[str, ...]) -> tuple[set[str], dict[str, str]]:
    """Walk local quoted includes and retain parents for diagnostics."""
    seen: set[str] = set()
    parent: dict[str, str] = {}
    queue = deque(roots)

    while queue:
        relative = queue.popleft()
        if relative in seen:
            continue
        assert (ROOT / relative).is_file(), f"missing declared path {relative}"
        seen.add(relative)

        for include in LOCAL_INCLUDE_RE.findall(code(relative)):
            dependency = resolve_local_include(relative, include)
            if dependency is None or dependency in seen:
                continue
            parent.setdefault(dependency, relative)
            queue.append(dependency)

    return seen, parent


def include_chain(path: str, parent: dict[str, str]) -> str:
    chain = [path]
    while chain[-1] in parent:
        chain.append(parent[chain[-1]])
    chain.reverse()
    return " -> ".join(chain)


def local_files(directory: str, suffix: str) -> set[str]:
    return {
        path.relative_to(ROOT).as_posix()
        for path in (ROOT / directory).glob(f"*{suffix}")
        if path.is_file()
    }


def main() -> None:
    core_sources = make_list("CORE_SOURCES")
    core_headers = make_list("CORE_HEADERS")
    adapter_sources = make_list("ADAPTER_SOURCES")
    public_headers = make_list("PUBLIC_HEADERS")
    private_headers = make_list("PRIVATE_HEADERS")

    declared_core = set(core_sources) | set(core_headers)
    declared_adapter_headers = {
        path for path in (*public_headers, *private_headers)
        if path.startswith("src/adapters/")
    }

    # Source-tree membership is itself an ownership contract. New native files
    # cannot silently exist outside the Makefile's declared layer.
    actual_core = local_files("src/core", ".c") | local_files("src/core", ".h")
    assert actual_core == declared_core, (
        "src/core ownership differs from CORE_SOURCES/CORE_HEADERS: "
        f"undeclared={sorted(actual_core - declared_core)}, "
        f"missing={sorted(declared_core - actual_core)}"
    )

    actual_adapter_sources = local_files("src/adapters", ".c")
    assert actual_adapter_sources == set(adapter_sources), (
        "src/adapters C ownership differs from ADAPTER_SOURCES: "
        f"undeclared={sorted(actual_adapter_sources - set(adapter_sources))}, "
        f"missing={sorted(set(adapter_sources) - actual_adapter_sources)}"
    )
    actual_adapter_headers = local_files("src/adapters", ".h")
    assert actual_adapter_headers == declared_adapter_headers, (
        "adapter header ownership differs from public/private declarations: "
        f"undeclared={sorted(actual_adapter_headers - declared_adapter_headers)}, "
        f"missing={sorted(declared_adapter_headers - actual_adapter_headers)}"
    )

    assert all(path.startswith("src/core/") for path in declared_core)
    assert all(path.startswith("src/adapters/") for path in adapter_sources)
    assert not (set(core_sources) & set(adapter_sources))

    closure, parent = include_closure(core_sources + core_headers)

    # Every repository-local dependency reached from the portable core must be
    # explicitly declared as part of that core. This catches indirect platform
    # leakage without depending on which concrete GLib API an adapter uses.
    undeclared = closure - declared_core
    assert not undeclared, (
        "core include closure reaches non-core files: "
        + "; ".join(
            include_chain(path, parent) for path in sorted(undeclared)
        )
    )

    forbidden_tokens = (
        "glib-object.h",
        "GObject",
        "GVariant",
        "g_timeout",
        "Gtk",
        "Gdk",
        "Gjs",
        "Cinnamon",
    )
    for relative in sorted(closure):
        implementation = code(relative)
        for token in forbidden_tokens:
            assert token not in implementation, (
                f"{include_chain(relative, parent)} leaks platform token {token}"
            )

    # Private components must be real translation units, not textual .c/.inc
    # inclusions that merely make a facade look smaller while preserving one
    # compilation/lifetime boundary.
    assert not list((ROOT / "src/adapters").glob("*.inc")), (
        "adapter implementation fragments must be compiled modules, not .inc files"
    )
    assert "src/adapters/temporal-policy-source.c" in adapter_sources
    assert "src/adapters/temporal-policy-source.h" in private_headers
    system_clock = code("src/adapters/system-clock.c")
    assert '#include "temporal-policy-source.h"' in system_clock
    assert "temporal-policy-source.inc" not in system_clock

    # The build must preserve a separately testable portable archive and the
    # architecture check itself as a required validation gate.
    makefile = read("Makefile")
    assert "libcalendar-plus-core.a" in makefile
    assert "test-portable-core.c $(CORE_SOURCES)" in makefile
    assert "$(CORE_CFLAGS)" in makefile
    assert "validate-architecture" in makefile

    # CalendarServer transport vocabulary belongs at the adapter boundary;
    # neutral event input records remain private to the core/source interface.
    # Protect the type/API boundary itself, not a particular documentation
    # sentence: prose may improve without weakening this contract.
    public_event_adapter = code("src/adapters/event-store.h")
    assert "GVariant *event_data" in public_event_adapter
    assert "CalendarPlusEventInput" not in public_event_adapter
    event_source = code("src/core/event-source.h")
    assert "CalendarPlusEventInput" in event_source
    assert "CalendarPlusEventSource" in event_source


if __name__ == "__main__":
    main()
