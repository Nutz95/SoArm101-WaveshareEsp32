#!/usr/bin/env python3
"""Syntax-check telemetry dashboard Python modules (Phase 0 hygiene)."""

from __future__ import annotations

import argparse
import py_compile
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="py_compile telemetry dashboard tools")
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="ESP32 firmware root (contains tools/telemetry_dashboard)",
    )
    args = parser.parse_args()

    dashboard_dir = args.project_root / "tools" / "telemetry_dashboard"
    if not dashboard_dir.is_dir():
        print(f"ERROR: dashboard dir not found: {dashboard_dir}", file=sys.stderr)
        return 1

    py_files = sorted(dashboard_dir.glob("*.py"))
    if not py_files:
        print(f"ERROR: no .py files in {dashboard_dir}", file=sys.stderr)
        return 1

    failed = 0
    for path in py_files:
        try:
            py_compile.compile(str(path), doraise=True)
            print(f"OK {path.name}")
        except py_compile.PyCompileError as exc:
            failed += 1
            print(f"FAIL {path.name}: {exc}", file=sys.stderr)

    if failed:
        print(f"Dashboard syntax check failed ({failed} file(s))", file=sys.stderr)
        return 1

    print(f"Dashboard syntax OK ({len(py_files)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
