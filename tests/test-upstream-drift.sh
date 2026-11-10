#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Network-free hostile-fixture tests for the upstream Cinnamon drift guard.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/bin"
cat > "$work/bin/curl" <<'MOCK'
#!/usr/bin/env bash
for arg in "$@"; do
  case "$arg" in
    *'/branches/'*) cat "$FIXTURE_BRANCH"; exit 0 ;;
    *'/compare/'*) cat "$FIXTURE_COMPARE"; exit 0 ;;
  esac
done
echo 'Unexpected GitHub API target' >&2; exit 1
MOCK
chmod +x "$work/bin/curl"
base=$(jq -r '.commit' "$root/tools/upstream-calendar-baseline.json")
latest=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
jq -n --arg sha "$latest" '{commit:{sha:$sha}}' > "$work/branch.json"
printf '%s\n' '{"files":[]}' > "$work/compare.json"
export FIXTURE_BRANCH="$work/branch.json" FIXTURE_COMPARE="$work/compare.json"
check() { PATH="$work/bin:$PATH" bash "$root/tools/check-upstream-calendar-drift.sh" > "$work/out" 2>&1; }
check || { cat "$work/out"; exit 1; }
jq -n --arg sha "$base" '{commit:{sha:$sha}}' > "$work/branch.json"
check || { cat "$work/out"; exit 1; }
jq -n --arg sha "$latest" '{commit:{sha:$sha}}' > "$work/branch.json"
jq -n '{files: [{filename:"files/usr/share/cinnamon/applets/calendar@cinnamon.org/calendar.js"}]}' > "$work/compare.json"
if check; then echo 'Watched change was not rejected' >&2; exit 1; fi
jq -n '{files: [range(299) | {filename:("docs/unrelated-" + (tostring) + ".md")}]}' > "$work/compare.json"
check || { cat "$work/out"; exit 1; }
jq -n '{files: [range(300) | {filename:("docs/unrelated-" + (tostring) + ".md")}]}' > "$work/compare.json"
if check; then echo '300-file truncation was not rejected' >&2; exit 1; fi
printf '%s\n' '{"files":"invalid"}' > "$work/compare.json"
if check; then echo 'Non-array files were accepted' >&2; exit 1; fi
printf '%s\n' '{"files":[{}]}' > "$work/compare.json"
if check; then echo 'Malformed changed-file entry was accepted' >&2; exit 1; fi
printf '%s\n' '[]' > "$work/branch.json"
if check; then echo 'Invalid branch response was accepted' >&2; exit 1; fi
echo 'Native upstream drift monitor contract tests passed.'
