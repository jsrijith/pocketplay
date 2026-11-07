"""Gives the firmware its version: VERSION's number, then the build (git's commit count) and the
commit it was built from, with a + when there were changes not yet committed. These become
defines that core/version.h reads, so the boot screen and About can show them."""
import subprocess
from pathlib import Path

Import("env")


def git(project, *args):
    try:
        return subprocess.run(["git", *args], cwd=project, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                              text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


project = Path(env.subst("$PROJECT_DIR"))
number = (project / "VERSION").read_text().strip()
build = git(project, "rev-list", "--count", "HEAD") or "0"
commit = git(project, "rev-parse", "--short=7", "HEAD") or "unknown"
if git(project, "status", "--porcelain", "--untracked-files=no"):
    commit += "+"

print(f"PocketPlay {number}, build {build} ({commit})")
env.Append(CPPDEFINES=[
    ("POCKETPLAY_VERSION", env.StringifyMacro(number)),
    ("POCKETPLAY_BUILD", build),
    ("POCKETPLAY_COMMIT", env.StringifyMacro(commit)),
])
