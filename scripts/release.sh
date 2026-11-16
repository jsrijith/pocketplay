#!/usr/bin/env bash
# Prepares a release: works out the next version from the commit messages since the last release,
# writes it to VERSION and CHANGELOG.md, commits, and tags. Nothing leaves this machine: the push
# is printed as the next step, and GitHub builds and drafts the release from the tag.
#
#   scripts/release.sh            # prepare the release
#   scripts/release.sh --dry-run  # only say what it would do
#
# The bump follows semantic versioning, read from the commit prefixes:
#   fix: only                          0.2.0 -> 0.2.1  (patch)
#   any feat:                          0.2.1 -> 0.3.0  (minor)
#   a breaking change (feat!: or a     1.4.0 -> 2.0.0  (major; before 1.0 it bumps the minor
#   "BREAKING CHANGE" line in a body)                   number, as semver allows for 0.x)
#   only docs:, chore:, test: ...      no release
# Raising VERSION by hand since the last tag overrides this: that number is released as it is,
# which is how 1.0.0 gets made. With no release yet, VERSION is released as it is.
set -euo pipefail
cd "$(dirname "$0")/.."

DRY=0
case "${1:-}" in
  "") ;;
  --dry-run) DRY=1 ;;
  *) echo "usage: $0 [--dry-run]" >&2; exit 2 ;;
esac

fail() { echo "release: $*" >&2; exit 1; }

[ "$(git rev-parse --abbrev-ref HEAD)" = "main" ] || fail "releases are made from main"
[ -z "$(git status --porcelain --untracked-files=no)" ] || fail "commit or stash your changes first"

current="$(tr -d '[:space:]' < VERSION)"
[[ "$current" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]] || fail "VERSION isn't MAJOR.MINOR.PATCH: '$current'"
major="${BASH_REMATCH[1]}" minor="${BASH_REMATCH[2]}" patch="${BASH_REMATCH[3]}"

last="$(git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null || true)"
if [ -z "$last" ]; then
  range="HEAD"
  next="$current"
  why="the first release, so VERSION is released as it is"
else
  range="$last..HEAD"
  [ -n "$(git log --format=%H "$range")" ] || fail "nothing has been committed since $last"
  if [ "${last#v}" != "$current" ]; then
    next="$current"
    why="VERSION was raised by hand since $last"
  else
    subjects="$(git log --format=%s "$range")"
    bodies="$(git log --format=%b "$range")"
    if grep -qE '^[a-z]+(\([^)]*\))?!:' <<<"$subjects" || grep -q 'BREAKING CHANGE' <<<"$bodies"; then
      if [ "$major" -eq 0 ]; then
        next="0.$((minor + 1)).0"
        why="a breaking change, which bumps the minor number before 1.0"
      else
        next="$((major + 1)).0.0"
        why="a breaking change"
      fi
    elif grep -qE '^feat(\([^)]*\))?:' <<<"$subjects"; then
      next="$major.$((minor + 1)).0"
      why="new features (feat:)"
    elif grep -qE '^fix(\([^)]*\))?:' <<<"$subjects"; then
      next="$major.$minor.$((patch + 1))"
      why="fixes only (fix:)"
    else
      fail "no feat: or fix: commits since $last, so there's nothing to release"
    fi
  fi
fi

tag="v$next"
git rev-parse -q --verify "refs/tags/$tag" >/dev/null && fail "$tag already exists"
changes="$(git log --format='- %s' "$range" | grep -v '^- chore(release):' || true)"

echo "Last release: ${last:-none}"
echo "Next version: $next ($why)"
echo
echo "Changes:"
echo "$changes"
[ "$DRY" -eq 1 ] && { echo; echo "(dry run: nothing changed)"; exit 0; }

if [ "$next" != "$current" ]; then
  echo "$next" > VERSION
  git add VERSION
fi
# What's new, written under "## Unreleased" in CHANGELOG.md, becomes this version's section.
if grep -q '^## Unreleased[[:space:]]*$' CHANGELOG.md 2>/dev/null; then
  sed -i "s/^## Unreleased[[:space:]]*\$/## $next/" CHANGELOG.md
  git add CHANGELOG.md
fi
git commit -q --allow-empty -m "chore(release): $tag"
git tag -a "$tag" -m "PocketPlay $next" -m "$changes"

cat <<EOF

Tagged $tag. Nothing has left this machine yet. To publish it:

  git push --follow-tags

GitHub's Release workflow then builds the image from the tag and creates a draft release with
its notes. Read the draft on the Releases page, then publish it there.
EOF
