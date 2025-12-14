#!/usr/bin/env python3
"""
Embedded - Static Analysis Tool

This script runs clang-tidy on the embedded C++ source code.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import List, Optional


class Colors:
    """ANSI color codes for terminal output"""

    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[1;33m"
    BLUE = ""
    NC = "\033[0m"  # No Color

    @classmethod
    def disable(cls):
        """Disable colors (for Windows terminals that don't support ANSI)"""
        cls.RED = ""
        cls.GREEN = ""
        cls.YELLOW = ""
        cls.BLUE = ""
        cls.NC = ""


# Enable colors on Windows 10+
if sys.platform == "win32":
    try:
        import ctypes

        kernel32 = ctypes.windll.kernel32
        kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    except Exception:
        Colors.disable()


def print_info(msg: str) -> None:
    """Print info message"""
    print(f"{Colors.BLUE}{msg}{Colors.NC}")


def print_success(msg: str) -> None:
    """Print success message"""
    print(f"{Colors.GREEN}{msg}{Colors.NC}")


def print_warning(msg: str) -> None:
    """Print warning message"""
    print(f"{Colors.YELLOW}{msg}{Colors.NC}")


def print_error(msg: str) -> None:
    """Print error message"""
    print(f"{Colors.RED}{msg}{Colors.NC}", file=sys.stderr)


def check_clang_tidy() -> bool:
    """Check if clang-tidy is installed"""
    try:
        subprocess.run(
            ["clang-tidy", "--version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=True,
        )
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def find_compile_commands(project_root: Path) -> Optional[Path]:
    """Find compile_commands.json in build directory"""
    build_dir = project_root / "build"

    if not build_dir.exists():
        return None

    compile_commands = build_dir / "compile_commands.json"
    if compile_commands.exists():
        return compile_commands

    return None


def find_source_files(
    source_dirs: List[Path], extensions: List[str], exclude_dirs: List[str]
) -> List[Path]:
    """Find all source files in the given directories"""
    source_files = []

    for source_dir in source_dirs:
        if not source_dir.exists():
            print_warning(f"Directory does not exist: {source_dir}")
            continue

        for ext in extensions:
            for file_path in source_dir.rglob(f"*.{ext}"):
                # Check if file is in an excluded directory
                excluded = False
                for exclude_dir in exclude_dirs:
                    try:
                        file_path.relative_to(exclude_dir)
                        excluded = True
                        break
                    except ValueError:
                        pass

                if not excluded:
                    source_files.append(file_path)

    return sorted(source_files)


def run_clang_tidy(
    file_path: Path,
    compile_commands: Optional[Path],
    config_file: Optional[Path],
) -> bool:
    """
    Run clang-tidy on a single file

    Args:
        file_path: Path to the file to analyze
        compile_commands: Path to compile_commands.json
        config_file: Path to .clang-tidy config file

    Returns:
        True if no issues found, False otherwise
    """
    cmd = ["clang-tidy"]

    # Add compile commands if available
    if compile_commands:
        cmd.extend(["-p", str(compile_commands.parent)])

    # Add config file if specified
    if config_file and config_file.exists():
        cmd.extend(["--config-file", str(config_file)])

    # Add the file to analyze
    cmd.append(str(file_path))

    try:
        result = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        # clang-tidy returns 0 if no errors, non-zero if errors found
        if result.returncode != 0 or result.stdout:
            if result.stdout:
                print(result.stdout)
            if result.stderr:
                print_error(result.stderr)
            return False

        return True
    except subprocess.CalledProcessError as e:
        print_error(f"clang-tidy failed: {e}")
        return False


def main() -> int:
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Run static analysis on embedded C++ code"
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Apply fixes suggested by clang-tidy",
    )
    parser.add_argument(
        "--config",
        type=Path,
        help="Path to .clang-tidy config file",
    )

    args = parser.parse_args()

    # Get script and project directories
    script_dir = Path(__file__).parent.resolve()
    project_root = script_dir.parent

    print_info("Embedded - Static Analysis Tool")
    print_info("=" * 45)

    # Check if clang-tidy is installed
    if not check_clang_tidy():
        print_error("clang-tidy is not installed. Please install it first.")
        print_info("\nInstallation instructions:")
        print_info("  Ubuntu/Debian: sudo apt-get install clang-tidy")
        print_info("  macOS: brew install llvm")
        print_info("  Windows: Install LLVM from https://releases.llvm.org/")
        return 1

    # Look for compile_commands.json
    print_info("\nLooking for compile_commands.json...")
    compile_commands = find_compile_commands(project_root)

    if compile_commands:
        print_success(f"Found: {compile_commands}")
    else:
        print_warning("compile_commands.json not found")
        print_info("Run 'idf.py build' first to generate it")
        print_info("Continuing without compile commands (may have limited analysis)...")

    # Look for .clang-tidy config
    config_file = args.config
    if not config_file:
        config_file = project_root / ".clang-tidy"

    if config_file and config_file.exists():
        print_success(f"Using config: {config_file}")
    else:
        print_warning("No .clang-tidy config file found, using defaults")
        config_file = None

    # Define source directories
    source_dirs = [
        project_root / "main",
        project_root / "src",
        project_root / "include",
    ]

    # Define file extensions to process
    extensions = ["cpp", "cc", "cxx"]

    # Define directories to exclude
    exclude_dirs = [
        project_root / "build",
        project_root / "components" / "nanopb",  # Don't analyze third-party code
    ]

    # Find all source files
    print_info("\nFinding source files...")
    source_files = find_source_files(source_dirs, extensions, exclude_dirs)

    if not source_files:
        print_warning(
            f"No source files found. Checked directories: {', '.join(str(d) for d in source_dirs)}"
        )
        return 0

    print_info(f"Found {len(source_files)} source files to analyze.")

    # Analyze files
    print_info("\nAnalyzing files...")
    issues_found = []

    for file_path in source_files:
        print_info(f"Analyzing: {file_path}")

        if not run_clang_tidy(file_path, compile_commands, config_file):
            issues_found.append(file_path)

    # Summary
    print_info("\n" + "=" * 60)
    if not issues_found:
        print_success("No issues found!")
        return 0
    else:
        print_error(f"Issues found in {len(issues_found)} file(s):")
        for file_path in issues_found:
            print_error(f"  - {file_path}")

        if args.fix:
            print_info("\nRerun with --fix to apply suggested fixes")

        return 1


if __name__ == "__main__":
    sys.exit(main())
