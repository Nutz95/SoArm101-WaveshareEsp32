import argparse
import re
import sys
from pathlib import Path
from typing import List, Tuple


MAX_IMPLEMENTATION_LINES = 600
MAX_IMPLEMENTATION_FUNCTIONS = 30
FUNCTION_RE = re.compile(
    r"^\s*(?:[A-Za-z_][\w:<>,~\*&\s]*?)\s+[A-Za-z_][\w:<>~]*\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{\s*$"
)


def iter_implementation_files(root: Path) -> List[Path]:
    return sorted(root.rglob("*.cpp"))


def count_non_empty_lines(lines: List[str]) -> int:
    return sum(1 for line in lines if line.strip())


def count_function_definitions(lines: List[str]) -> int:
    count = 0
    for line in lines:
        if FUNCTION_RE.match(line):
            count += 1
    return count


def check_file(path: Path) -> List[str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    line_count = count_non_empty_lines(lines)
    function_count = count_function_definitions(lines)
    errors: List[str] = []

    if line_count > MAX_IMPLEMENTATION_LINES:
        errors.append(f"{path}: implementation file has {line_count} non-empty lines (max {MAX_IMPLEMENTATION_LINES})")

    if function_count > MAX_IMPLEMENTATION_FUNCTIONS:
        errors.append(f"{path}: implementation file has {function_count} function definitions (max {MAX_IMPLEMENTATION_FUNCTIONS})")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Check structural implementation-file limits")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[1] / "src"))
    args = parser.parse_args()

    root = Path(args.root).resolve()
    if not root.is_dir():
        print(f"error: root directory not found: {root}", file=sys.stderr)
        return 2

    errors: List[str] = []
    for path in iter_implementation_files(root):
      errors.extend(check_file(path))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"Structural limits OK for {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())