#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Reviewed Cinnamon upstream drift monitor; no source downloads during ordinary builds.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
config="$root/tools/upstream-calendar-baseline.json"
for cmd in jq curl; do command -v "$cmd" >/dev/null || { echo "Missing $cmd" >&2; exit 1; }; done
jq -e '
  type == "object" and
  (.repository | type == "string" and test("^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")) and
  (.ref | type == "string" and test("^[A-Za-z0-9_.-]+$")) and
  (.commit | type == "string" and test("^[0-9a-f]{40}$")) and
  (.paths | type == "array" and length > 0 and all(.[]; type == "string" and length > 0))
' "$config" >/dev/null || { echo 'Invalid Cinnamon upstream baseline' >&2; exit 1; }
repository=$(jq -r '.repository' "$config")
reference=$(jq -r '.ref' "$config")
baseline=$(jq -r '.commit' "$config")
request() {
  local -a auth=()
  [[ -z "${GITHUB_TOKEN:-}" ]] || auth=(-H "Authorization: Bearer $GITHUB_TOKEN")
  curl --fail --silent --show-error --retry 3 --max-time 30 \
    -H 'User-Agent: calendar-upstream-drift' \
    -H 'Accept: application/vnd.github+json' \
    -H 'X-GitHub-Api-Version: 2022-11-28' \
    "${auth[@]}" "$1"
}
branch=$(request "https://api.github.com/repos/$repository/branches/$reference") || {
  echo 'Cannot certify Cinnamon calendar drift state: branch request failed' >&2; exit 1;
}
latest=$(jq -er 'if type=="object" then .commit.sha else empty end | select(type == "string" and test("^[0-9a-f]{40}$"))' <<< "$branch") || {
  echo 'Cannot certify Cinnamon calendar drift state: invalid branch response' >&2; exit 1;
}
if [[ "$latest" == "$baseline" ]]; then
  echo "Cinnamon calendar baseline is current at $baseline."
  exit 0
fi
compare=$(request "https://api.github.com/repos/$repository/compare/$baseline...$latest?per_page=100") || {
  echo 'Cannot certify Cinnamon calendar drift state: comparison request failed' >&2; exit 1;
}
jq -e 'type == "object" and (.files | type == "array") and
  (.files | length < 300) and all(.files[]; type == "object" and (.filename | type == "string"))' <<< "$compare" >/dev/null || {
  echo 'Cannot certify Cinnamon calendar drift state: invalid, truncated or 300-file API comparison' >&2; exit 1;
}
paths=$(jq -c '.paths' "$config")
mapfile -t changed < <(jq -r --argjson watched "$paths" '
  [.files[].filename as $name |
    select(any($watched[]; . as $prefix | $name == $prefix or ($name | startswith($prefix)))) |
    $name] | sort | .[]' <<< "$compare")
if (("${#changed[@]}" == 0)); then
  echo "Cinnamon advanced from $baseline to $latest, with no watched calendar changes."
  exit 0
fi
echo 'Relevant upstream Cinnamon calendar drift detected. Review before advancing baseline:' >&2
printf '  %s\n' "${changed[@]}" >&2
printf 'Reviewed baseline: %s\nCurrent upstream:  %s\n' "$baseline" "$latest" >&2
exit 1
