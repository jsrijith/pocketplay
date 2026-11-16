#!/usr/bin/env python3
"""Writes a release's notes to stdout, from .github/release-notes.md with its blanks filled in.

    scripts/release_notes.py BUILD_LOG IMAGE [TAG]

BUILD_LOG is the output of the build that made IMAGE (scripts/package.sh's), and the version,
build and commit are read from the line scripts/version.py printed in it, so the notes describe
the image that was compiled rather than what git says now. What's new is VERSION's section of
CHANGELOG.md, or else TAG's message (the commits release.sh listed), or else the commits since
the last release. Stops with an error for a build with uncommitted changes, or one whose version
isn't VERSION's.
"""
import hashlib
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def fail(message):
    sys.exit(f"release_notes: {message}")


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, stdout=subprocess.PIPE, text=True, check=True).stdout.strip()


def built(log_text):
    matches = re.findall(r"PocketPlay (\S+), build (\d+) \((\S+)\)", log_text)
    if not matches:
        fail("the build log has no 'PocketPlay <version>, build <n> (<commit>)' line")
    return matches[-1]


def changelog_section(version):
    path = ROOT / "CHANGELOG.md"
    if not path.exists():
        return ""
    text = path.read_text()
    match = re.search(rf"^## {re.escape(version)}[ \t]*\n(.*?)(?=^## |\Z)", text, re.M | re.S)
    return match.group(1).strip() if match else ""


def whats_new(version, tag):
    section = changelog_section(version)
    if section:
        return section
    if tag:
        body = git("for-each-ref", f"refs/tags/{tag}", "--format=%(contents:body)")
        if body:
            print(f"release_notes: CHANGELOG.md has no '## {version}', so using {tag}'s commit list",
                  file=sys.stderr)
            return body
    last = subprocess.run(["git", "describe", "--tags", "--abbrev=0", "--match", "v[0-9]*"], cwd=ROOT,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True).stdout.strip()
    commits = git("log", "--format=- %s", f"{last}..HEAD" if last else "HEAD")
    print(f"release_notes: CHANGELOG.md has no '## {version}', so using the commits since {last or 'the start'}",
          file=sys.stderr)
    return "\n".join(line for line in commits.splitlines() if not line.startswith("- chore(release):"))


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    log_path, image_path = Path(sys.argv[1]), Path(sys.argv[2])
    tag = sys.argv[3] if len(sys.argv) == 4 else ""

    expected = (ROOT / "VERSION").read_text().strip()
    version, build, commit = built(log_path.read_text(errors="replace"))
    if version != expected:
        fail(f"the image is version {version}, but VERSION says {expected}")
    if commit.endswith("+") or commit == "unknown":
        fail(f"the image was built from '{commit}', not a clean commit")

    notes = (ROOT / ".github" / "release-notes.md").read_text()
    # The boot screen shows the commit in capitals (ui/logo.cpp), so the notes do too.
    fills = {
        "{WHATS_NEW}": whats_new(version, tag),
        "{SHA256}": hashlib.sha256(image_path.read_bytes()).hexdigest(),
        "{BOOT_LINE}": f"VERSION {version}  BUILD {build} ({commit.upper()})",
    }
    for blank, value in fills.items():
        notes = notes.replace(blank, value)
    print(notes, end="")


if __name__ == "__main__":
    main()
