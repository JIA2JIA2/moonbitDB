#!/usr/bin/env python3
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SERVER = ROOT / "lib" / "server.mbt"
README = ROOT / "README.md"


def command_list():
    source = SERVER.read_text(encoding="utf-8")
    match = re.search(r"pub fn Database::command\(\) -> Array\[String\] \{(.*?)\n  \]", source, re.S)
    if not match:
        raise SystemExit("cannot find Database::command() in lib/server.mbt")
    return re.findall(r'"([A-Z_]+)"', match.group(1))


def readme_commands():
    commands = []
    for line in README.read_text(encoding="utf-8").splitlines():
        match = re.match(r"\|\s*`([A-Z_]+)`\s*\|", line)
        if match and match.group(1) not in commands:
            commands.append(match.group(1))
    return commands


def main():
    actual = command_list()
    documented = readme_commands()
    actual_set = set(actual)
    documented_set = set(documented)

    errors = []
    if len(actual) != len(actual_set):
        errors.append("Database::command() contains duplicate command names")
    if len(documented) != len(documented_set):
        errors.append("README command table contains duplicate command names")
    if len(actual) != len(documented):
        errors.append(f"command count mismatch: source={len(actual)}, README={len(documented)}")
    missing_in_readme = sorted(actual_set - documented_set)
    missing_in_source = sorted(documented_set - actual_set)
    if missing_in_readme:
        errors.append(f"missing in README: {', '.join(missing_in_readme)}")
    if missing_in_source:
        errors.append(f"missing in source: {', '.join(missing_in_source)}")

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print(f"command table ok: {len(actual)} commands")
    return 0


if __name__ == "__main__":
    sys.exit(main())