#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "scripts" / "bench_baseline.json"
UNITS = {"ns": 1.0, "µs": 1000.0, "us": 1000.0, "ms": 1000000.0}
LINE = re.compile(r"^(.*?)\s+([0-9.]+)\s+(ns|µs|us|ms)\s+±")


def main():
    if len(sys.argv) < 2:
        print("usage: check_bench.py <moon-bench-output>")
        return 1
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    results = {}
    for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
        match = LINE.match(line.strip())
        if not match:
            continue
        name = match.group(1).strip()
        value = float(match.group(2)) * UNITS[match.group(3)]
        results[name] = value

    warnings = 0
    for name, expected in baseline.items():
        if name not in results:
            print(f"WARN: benchmark missing: {name}")
            warnings += 1
            continue
        ratio = results[name] / expected
        if ratio > 2.0:
            print(f"WARN: benchmark regression: {name} = {results[name]:.0f}ns vs baseline {expected}ns ({ratio:.2f}x)")
            warnings += 1
        else:
            print(f"ok: {name} ({ratio:.2f}x baseline)")

    print(f"benchmark check completed with {warnings} warning(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())