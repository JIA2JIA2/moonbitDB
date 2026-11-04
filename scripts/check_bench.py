#!/usr/bin/env python3
"""Benchmark regression check.

Usage:
  check_bench.py <moon-bench-output>            # compare against baseline
  check_bench.py <output> --strict              # missing benchmarks fail the check
  check_bench.py <output> --update              # update baseline from current results
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "scripts" / "bench_baseline.json"
UNITS = {"ns": 1.0, "µs": 1000.0, "us": 1000.0, "ms": 1000000.0}
LINE = re.compile(r"^(.*?)\s+([0-9.]+)\s+(ns|µs|us|ms)\s+±")


def parse_results(path: str) -> dict:
    results = {}
    source = Path(path)
    if not source.exists():
        print(f"ERROR: bench output file not found: {path}")
        sys.exit(1)
    for line in source.read_text(encoding="utf-8").splitlines():
        match = LINE.match(line.strip())
        if not match:
            continue
        name = match.group(1).strip()
        value = float(match.group(2)) * UNITS[match.group(3)]
        results[name] = value
    return results


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    if not args:
        print("usage: check_bench.py <moon-bench-output> [--strict] [--update]")
        return 1
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    results = parse_results(args[0])

    if not results:
        print("ERROR: no benchmark results parsed from input — was `moon bench` run with coverage output redirected?")
        return 1

    if "--update" in flags:
        baseline.update(results)
        BASELINE.write_text(
            json.dumps(baseline, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        print(f"baseline updated: {len(baseline)} items")
        return 0

    warnings = 0
    for name, expected in baseline.items():
        if name not in results:
            level = "ERROR" if "--strict" in flags else "WARN"
            print(f"{level}: benchmark missing: {name}")
            warnings += 1
            continue
        ratio = results[name] / expected
        if ratio > 2.0:
            print(f"WARN: benchmark regression: {name} = {results[name]:.0f}ns vs baseline {expected}ns ({ratio:.2f}x)")
            warnings += 1
        else:
            print(f"ok: {name} ({ratio:.2f}x baseline)")

    print(f"benchmark check completed with {warnings} warning(s)")
    if "--strict" in flags and warnings > 0:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
