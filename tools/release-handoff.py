#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Create and verify Calendar's release handoff.

This tool is the single owner of release-version transition detection and the
qualified artifact manifest. Workflows transport the handoff; they do not
reconstruct its metadata independently.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")


def fail(message: str) -> "NoReturn":
    raise SystemExit(message)


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def makefile_version(text: str) -> str:
    match = re.search(r"^VERSION := ([0-9]+\.[0-9]+\.[0-9]+)$", text, re.MULTILINE)
    if not match:
        fail("Makefile must contain one literal VERSION := X.Y.Z assignment")
    return match.group(1)


def changelog_version(text: str) -> str:
    match = re.match(r"infiltrator-calendar \(([0-9]+\.[0-9]+\.[0-9]+)\) ", text)
    if not match:
        fail("debian/changelog does not start with an infiltrator-calendar X.Y.Z stanza")
    return match.group(1)


def current_version() -> str:
    version = makefile_version(read("Makefile"))
    metadata = json.loads(read("src/cinnamon/metadata.json"))
    metadata_version = metadata.get("version")
    changelog = changelog_version(read("debian/changelog"))
    if metadata_version != version or changelog != version:
        fail(
            "release version identities disagree: "
            f"Makefile={version}, metadata={metadata_version}, changelog={changelog}"
        )
    return version


def version_key(version: str) -> tuple[int, int, int]:
    if not VERSION_PATTERN.fullmatch(version):
        fail(f"invalid release version: {version}")
    return tuple(int(part) for part in version.split("."))


def git_file(commit: str, path: str) -> str:
    if not SHA_PATTERN.fullmatch(commit):
        fail(f"invalid previous commit SHA: {commit!r}")
    result = subprocess.run(
        ["git", "show", f"{commit}:{path}"],
        cwd=ROOT,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        fail(
            f"cannot read {path} from previous main revision {commit}: "
            f"{result.stderr.strip()}"
        )
    return result.stdout


def release_requested(before: str) -> tuple[bool, str, str]:
    version = current_version()
    if not before or set(before) == {"0"}:
        return False, version, ""
    previous = makefile_version(git_file(before, "Makefile"))
    if previous == version:
        return False, version, previous
    if version_key(version) <= version_key(previous):
        fail(
            "release version changed without increasing: "
            f"previous={previous}, current={version}"
        )
    return True, version, previous


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def release_notes() -> str:
    lines = read("debian/changelog").splitlines()
    notes: list[str] = []
    for line in lines[1:]:
        if line.startswith(" -- "):
            break
        if line.startswith("  * "):
            notes.append(f"- {line[4:]}")
        elif line.strip():
            notes.append(line.strip())
    if not notes:
        fail("current debian/changelog stanza contains no release notes")
    return "\n".join(notes) + "\n"


def expected_assets(version: str) -> tuple[str, str]:
    return (
        f"calendar_{version}_amd64.deb",
        f"calendar-{version}-local-folder.run",
    )


def create_handoff(dist: Path, source_sha: str) -> None:
    version = current_version()
    if not SHA_PATTERN.fullmatch(source_sha):
        fail(f"invalid source SHA: {source_sha!r}")
    dist.mkdir(parents=True, exist_ok=True)

    deb_name, installer_name = expected_assets(version)
    assets = []
    for name in (deb_name, installer_name):
        path = dist / name
        if not path.is_file() or path.stat().st_size <= 0:
            fail(f"qualified release asset is missing or empty: {path}")
        assets.append(
            {
                "name": name,
                "sha256": sha256(path),
                "size": path.stat().st_size,
            }
        )

    (dist / "release-notes.md").write_text(release_notes(), encoding="utf-8")
    manifest = {
        "schema": 1,
        "version": version,
        "tag": f"v{version}",
        "source_sha": source_sha,
        "assets": assets,
    }
    (dist / "release-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def load_manifest(dist: Path) -> dict[str, object]:
    path = dist / "release-manifest.json"
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"cannot read release manifest {path}: {exc}")
    if not isinstance(manifest, dict):
        fail("release manifest root must be an object")
    return manifest


def verify_handoff(dist: Path, expected_sha: str, github_env: Path | None) -> None:
    if not SHA_PATTERN.fullmatch(expected_sha):
        fail(f"invalid expected source SHA: {expected_sha!r}")
    manifest = load_manifest(dist)

    version = manifest.get("version")
    source_sha = manifest.get("source_sha")
    tag = manifest.get("tag")
    assets = manifest.get("assets")
    if manifest.get("schema") != 1:
        fail("unsupported release manifest schema")
    if not isinstance(version, str) or not VERSION_PATTERN.fullmatch(version):
        fail("release manifest contains an invalid version")
    if source_sha != expected_sha:
        fail(
            "release handoff source SHA does not match the qualified revision: "
            f"manifest={source_sha}, expected={expected_sha}"
        )
    if tag != f"v{version}":
        fail(f"release manifest tag does not match version {version}")
    if not isinstance(assets, list) or len(assets) != 2:
        fail("release manifest must describe exactly two assets")

    expected_names = set(expected_assets(version))
    seen: dict[str, dict[str, object]] = {}
    for asset in assets:
        if not isinstance(asset, dict):
            fail("release manifest asset entry must be an object")
        name = asset.get("name")
        digest = asset.get("sha256")
        size = asset.get("size")
        if (
            not isinstance(name, str)
            or name not in expected_names
            or name in seen
            or not isinstance(digest, str)
            or not re.fullmatch(r"[0-9a-f]{64}", digest)
            or not isinstance(size, int)
            or size <= 0
        ):
            fail(f"invalid release manifest asset entry: {asset!r}")
        path = dist / name
        if not path.is_file():
            fail(f"release handoff is missing asset {name}")
        if path.stat().st_size != size:
            fail(f"release handoff size mismatch for {name}")
        if sha256(path) != digest:
            fail(f"release handoff digest mismatch for {name}")
        seen[name] = asset

    notes = dist / "release-notes.md"
    if not notes.is_file() or not notes.read_text(encoding="utf-8").strip():
        fail("release handoff is missing release-notes.md")

    if github_env is None:
        return

    deb_name, installer_name = expected_assets(version)
    deb = seen[deb_name]
    installer = seen[installer_name]
    values = {
        "RELEASE_VERSION": version,
        "RELEASE_TAG": str(tag),
        "RELEASE_HEAD": expected_sha,
        "RELEASE_DEB": str(dist / deb_name),
        "RELEASE_DEB_NAME": deb_name,
        "RELEASE_DEB_SHA256": str(deb["sha256"]),
        "RELEASE_DEB_SIZE": str(deb["size"]),
        "RELEASE_INSTALLER": str(dist / installer_name),
        "RELEASE_INSTALLER_NAME": installer_name,
        "RELEASE_INSTALLER_SHA256": str(installer["sha256"]),
        "RELEASE_INSTALLER_SIZE": str(installer["size"]),
        "RELEASE_NOTES": str(notes),
    }
    with github_env.open("a", encoding="utf-8") as stream:
        for key, value in values.items():
            if "\n" in value or "\r" in value:
                fail(f"unsafe newline in release environment value {key}")
            stream.write(f"{key}={value}\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)

    requested = commands.add_parser(
        "release-requested",
        help="report whether main changed to a newer release version",
    )
    requested.add_argument("--before", default="")

    create = commands.add_parser(
        "create",
        help="create the immutable release handoff for already-qualified assets",
    )
    create.add_argument("--dist", type=Path, required=True)
    create.add_argument("--source-sha", required=True)

    verify = commands.add_parser(
        "verify",
        help="verify a release handoff without rebuilding it",
    )
    verify.add_argument("--dist", type=Path, required=True)
    verify.add_argument("--expected-sha", required=True)
    verify.add_argument("--github-env", type=Path)

    args = parser.parse_args()
    if args.command == "release-requested":
        requested_flag, version, previous = release_requested(args.before)
        print(f"requested={'true' if requested_flag else 'false'}")
        print(f"version={version}")
        print(f"previous_version={previous}")
    elif args.command == "create":
        create_handoff(args.dist, args.source_sha)
    elif args.command == "verify":
        verify_handoff(args.dist, args.expected_sha, args.github_env)


if __name__ == "__main__":
    main()
