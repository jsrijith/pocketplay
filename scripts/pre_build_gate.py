"""Stops the firmware build unless the PC tests pass."""
import subprocess
from pathlib import Path

Import("env")

# These only read project information or remove files, and the editor runs some of
# them in the background, so a failing test shouldn't break them.
NO_FIRMWARE_TARGETS = {"clean", "compiledb", "envdump", "idedata", "_idedata", "__idedata"}


def run_gate() -> None:
    project = Path(env.subst("$PROJECT_DIR"))
    result = subprocess.run(
        ["bash", str(project / "scripts" / "check.sh")],
        cwd=project,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    print(result.stdout, end="")

    # Both, because a run that dies early can exit 0 without reaching its verdict.
    lines = result.stdout.strip().splitlines()
    if result.returncode != 0 or not lines or lines[-1] != "CHECK-VERDICT: green":
        print("Build stopped: the PC tests did not pass. See above, or run scripts/check.sh.")
        env.Exit(1)


if not NO_FIRMWARE_TARGETS.intersection(COMMAND_LINE_TARGETS):
    run_gate()
