import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence


FUNCTION_RE = re.compile(
    r"^\s*(?:[A-Za-z_][\w:<>,~\*&\s]*?)\s+[A-Za-z_][\w:<>~]*\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{\s*$"
)
CLASS_RE = re.compile(r"^\s*class\s+[A-Za-z_][\w]*\b[^;]*\{\s*$")
PY_FUNCTION_RE = re.compile(r"^\s*def\s+[A-Za-z_][\w]*\s*\(")


class FolderRule:
    def __init__(
        self,
        name: str,
        root: str,
        extensions: Sequence[str],
        max_lines: int,
        max_functions: Optional[int] = None,
        max_classes: Optional[int] = None,
    ):
        self.name = name
        self.root = root
        self.extensions = tuple(extensions)
        self.max_lines = max_lines
        self.max_functions = max_functions
        self.max_classes = max_classes


FOLDER_RULES: Sequence[FolderRule] = (
    FolderRule(
        name="firmware",
        root="src",
        extensions=(".cpp",),
        max_lines=350,
        max_functions=30,
        max_classes=1,
    ),
    FolderRule(
        name="dashboard-python",
        root="tools/telemetry_dashboard",
        extensions=(".py",),
        max_lines=350,
        max_functions=30,
    ),
    FolderRule(
        name="dashboard-js",
        root="tools/telemetry_dashboard/static/js",
        extensions=(".js",),
        max_lines=350,
    ),
    FolderRule(
        name="tests",
        root="test",
        extensions=(".cpp",),
        max_lines=320,
        max_functions=30,
        max_classes=1,
    ),
)

CODE_CONTEXT_INDEX_PATH = Path("docs/CODE_CONTEXT_INDEX.md")
KEY_CONTEXT_PATHS = (
    "src/leader/",
    "src/follower/",
    "src/common/",
    "tools/telemetry_dashboard/",
)


def normalize_rel(path: Path) -> str:
    return path.as_posix()


def iter_files_for_rule(project_root: Path, rule: FolderRule) -> List[Path]:
    base = project_root / rule.root
    if not base.is_dir():
        return []

    files: List[Path] = []
    for ext in rule.extensions:
        files.extend(base.rglob(f"*{ext}"))
    return sorted(files)


def count_non_empty_lines(lines: List[str]) -> int:
    return sum(1 for line in lines if line.strip())


def count_function_definitions(lines: List[str]) -> int:
    count = 0
    for line in lines:
        if FUNCTION_RE.match(line):
            count += 1
    return count


def count_python_function_definitions(lines: List[str]) -> int:
    count = 0
    for line in lines:
        if PY_FUNCTION_RE.match(line):
            count += 1
    return count


def count_class_definitions(lines: List[str]) -> int:
    count = 0
    for line in lines:
        if CLASS_RE.match(line):
            count += 1
    return count


def check_file(path: Path, rule: FolderRule, rel_path: str) -> List[str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    line_count = count_non_empty_lines(lines)

    function_count = None
    class_count = None
    suffix = path.suffix.lower()
    if suffix == ".cpp":
        function_count = count_function_definitions(lines)
        class_count = count_class_definitions(lines)
    elif suffix == ".py":
        function_count = count_python_function_definitions(lines)

    errors: List[str] = []

    if line_count > rule.max_lines:
        errors.append(
            f"{rel_path}: {rule.name} file has {line_count} non-empty lines (max {rule.max_lines})"
        )

    if rule.max_functions is not None and function_count is not None and function_count > rule.max_functions:
        errors.append(
            f"{rel_path}: {rule.name} file has {function_count} function definitions (max {rule.max_functions})"
        )

    if rule.max_classes is not None and class_count is not None and class_count > rule.max_classes:
        errors.append(
            f"{rel_path}: {rule.name} file has {class_count} class definitions (max {rule.max_classes})"
        )

    return errors


def run_git(project_root: Path, args: Sequence[str]) -> List[str]:
    result = subprocess.run(
        ["git", *args],
        cwd=str(project_root),
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        return []
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def collect_changed_files(project_root: Path) -> Optional[List[str]]:
    if not (project_root / ".git").exists():
        return None

    unstaged = run_git(project_root, ["diff", "--name-only"])
    staged = run_git(project_root, ["diff", "--cached", "--name-only"])
    untracked = run_git(project_root, ["ls-files", "--others", "--exclude-standard"])

    merged = sorted(set(unstaged + staged + untracked))
    return merged


def requires_index_update(rel_path: str) -> bool:
    normalized = rel_path.replace("\\", "/")
    for prefix in KEY_CONTEXT_PATHS:
        if normalized.startswith(prefix):
            return True
    return False


def check_context_index_sync(project_root: Path, changed_files: Optional[List[str]]) -> List[str]:
    if changed_files is None:
        return []

    key_changes = [path for path in changed_files if requires_index_update(path)]
    if not key_changes:
        return []

    index_changed = any(path.replace("\\", "/") == normalize_rel(CODE_CONTEXT_INDEX_PATH) for path in changed_files)
    if index_changed:
        return []

    sample_paths = ", ".join(key_changes[:3])
    return [
        "docs/CODE_CONTEXT_INDEX.md is not updated while key code paths changed "
        f"(examples: {sample_paths})."
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description="Check structural limits and context-index synchronization")
    parser.add_argument(
        "--project-root",
        default=str(Path(__file__).resolve().parents[1]),
        help="Project root containing src/, tools/, test/, and docs/",
    )
    parser.add_argument(
        "--skip-index-sync",
        action="store_true",
        help="Skip CODE_CONTEXT_INDEX synchronization check",
    )
    args = parser.parse_args()

    project_root = Path(args.project_root).resolve()
    if not project_root.is_dir():
        print(f"error: project root not found: {project_root}", file=sys.stderr)
        return 2

    errors: List[str] = []

    for rule in FOLDER_RULES:
        for path in iter_files_for_rule(project_root, rule):
            rel_path = normalize_rel(path.relative_to(project_root))
            errors.extend(check_file(path, rule, rel_path))

    if not args.skip_index_sync:
        changed_files = collect_changed_files(project_root)
        errors.extend(check_context_index_sync(project_root, changed_files))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    print(f"Structural limits OK for {project_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())