#!/usr/bin/env python3
"""
Embedded - Dependency Installer

This script handles installation of dependencies for the ESP32 embedded firmware.
It checks for ESP-IDF and helps install nanopb for protobuf support.
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple

# Fix encoding issues on Windows
if platform.system() == "Windows":
    import io
    import sys

    if sys.stdout.encoding != "utf-8":
        sys.stdout = io.TextIOWrapper(
            sys.stdout.buffer, encoding="utf-8", errors="replace"
        )
    if sys.stderr.encoding != "utf-8":
        sys.stderr = io.TextIOWrapper(
            sys.stderr.buffer, encoding="utf-8", errors="replace"
        )


class Colors:
    """ANSI color codes for terminal output"""

    BLUE = ""
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    RED = "\033[91m"
    BOLD = "\033[1m"
    END = "\033[0m"

    @staticmethod
    def disable():
        """Disable colors (for non-terminal output)"""
        Colors.GREEN = ""
        Colors.YELLOW = ""
        Colors.RED = ""
        Colors.BOLD = ""
        Colors.END = ""


# Disable colors on Windows without ANSI support or when piped
if platform.system() == "Windows" and not os.environ.get("ANSICON"):
    Colors.disable()
elif not sys.stdout.isatty():
    Colors.disable()


def print_info(message: str) -> None:
    """Print informational message"""
    print(f"{message}")


def print_success(message: str) -> None:
    """Print success message"""
    print(f"{Colors.GREEN}✓ {message}{Colors.END}")


def print_warning(message: str) -> None:
    """Print warning message"""
    print(f"{Colors.YELLOW}⚠ {message}{Colors.END}")


def print_error(message: str) -> None:
    """Print error message"""
    print(f"{Colors.RED}✗ {message}{Colors.END}", file=sys.stderr)


def print_header(message: str) -> None:
    """Print section header"""
    print(f"\n{Colors.BOLD}{'=' * 70}{Colors.END}")
    print(f"{Colors.BOLD}{message}{Colors.END}")
    print(f"{Colors.BOLD}{'=' * 70}{Colors.END}\n")


def run_command(
    cmd: List[str],
    cwd: Optional[Path] = None,
    check: bool = True,
    capture: bool = True,
) -> Tuple[int, str, str]:
    """Run a command and return exit code, stdout, stderr"""
    try:
        if capture:
            result = subprocess.run(
                cmd, cwd=cwd, capture_output=True, text=True, check=check
            )
            return result.returncode, result.stdout, result.stderr
        else:
            result = subprocess.run(cmd, cwd=cwd, check=check)
            return result.returncode, "", ""
    except subprocess.CalledProcessError as e:
        if capture:
            return (
                e.returncode,
                e.stdout if hasattr(e, "stdout") else "",
                e.stderr if hasattr(e, "stderr") else "",
            )
        return e.returncode, "", ""
    except FileNotFoundError:
        return 127, "", f"Command not found: {cmd[0]}"


def check_command(command: str) -> bool:
    """Check if a command is available in PATH"""
    return shutil.which(command) is not None


def get_distro_info() -> Tuple[str, str]:
    """Get Linux distribution name and version"""
    try:
        if Path("/etc/os-release").exists():
            with open("/etc/os-release") as f:
                lines = f.readlines()
                distro_id = ""
                distro_version = ""
                for line in lines:
                    if line.startswith("ID="):
                        distro_id = line.split("=")[1].strip().strip('"')
                    elif line.startswith("VERSION_ID="):
                        distro_version = line.split("=")[1].strip().strip('"')
                return distro_id, distro_version
    except Exception:
        pass
    return "unknown", "unknown"


def check_esp_idf() -> Tuple[bool, Optional[str]]:
    """Check if ESP-IDF is installed and configured"""
    idf_path = os.environ.get("IDF_PATH")

    if not idf_path:
        return False, None

    if not Path(idf_path).exists():
        return False, None

    # Check if idf.py is available
    if not check_command("idf.py"):
        return False, idf_path

    return True, idf_path


def check_nanopb() -> bool:
    """Check if nanopb is available in components"""
    script_dir = Path(__file__).parent.resolve()
    project_root = script_dir.parent

    nanopb_component = project_root / "components" / "nanopb"
    return nanopb_component.exists()


def install_nanopb(force: bool = False) -> bool:
    """Install nanopb component"""
    script_dir = Path(__file__).parent.resolve()
    project_root = script_dir.parent
    components_dir = project_root / "components"
    nanopb_dir = components_dir / "nanopb"

    if nanopb_dir.exists() and not force:
        print_info("Nanopb component already exists")
        return True

    print_info("Installing nanopb component...")

    # Create components directory if it doesn't exist
    components_dir.mkdir(parents=True, exist_ok=True)

    # Clone nanopb repository
    nanopb_repo = "https://github.com/nanopb/nanopb.git"

    if nanopb_dir.exists():
        print_info("Removing existing nanopb directory...")
        shutil.rmtree(nanopb_dir)

    print_info(f"Cloning nanopb from {nanopb_repo}...")
    returncode, stdout, stderr = run_command(
        ["git", "clone", "--depth", "1", nanopb_repo, str(nanopb_dir)],
        capture=True,
    )

    if returncode != 0:
        print_error(f"Failed to clone nanopb: {stderr}")
        return False

    print_success("Nanopb component installed successfully")
    return True


def check_python_packages() -> None:
    """Check for required Python packages"""
    print_info("\nChecking Python packages...")

    required_packages = [
        "protobuf",
    ]

    missing_packages = []

    for package in required_packages:
        try:
            __import__(package)
            print_success(f"{package} is installed")
        except ImportError:
            print_warning(f"{package} is NOT installed")
            missing_packages.append(package)

    if missing_packages:
        print_warning("\nSome Python packages are missing.")
        print_info("Install them with:")
        print_info(f"  pip install {' '.join(missing_packages)}")


def install_esp_idf_prerequisites() -> bool:
    """Install ESP-IDF prerequisites for the current system"""
    system = platform.system()

    if system == "Linux":
        distro_id, _ = get_distro_info()
        print_info(f"Detected Linux distribution: {distro_id}")

        if distro_id in ["ubuntu", "debian", "linuxmint", "pop"]:
            print_info("Installing prerequisites for Ubuntu/Debian...")
            packages = [
                "git",
                "wget",
                "flex",
                "bison",
                "gperf",
                "python3",
                "python3-pip",
                "python3-venv",
                "cmake",
                "ninja-build",
                "ccache",
                "libffi-dev",
                "libssl-dev",
                "dfu-util",
                "libusb-1.0-0",
            ]
            cmd = ["sudo", "apt-get", "install", "-y"] + packages
            returncode, _, stderr = run_command(cmd, capture=False, check=False)
            if returncode != 0:
                print_error(f"Failed to install prerequisites: {stderr}")
                return False
            print_success("Prerequisites installed successfully")
            return True

        elif distro_id in ["centos", "rhel", "fedora", "rocky", "almalinux"]:
            print_info("Installing prerequisites for CentOS/RHEL/Fedora...")
            packages = [
                "git",
                "wget",
                "flex",
                "bison",
                "gperf",
                "python3",
                "python3-pip",
                "cmake",
                "ninja-build",
                "ccache",
                "dfu-util",
                "libusbx",
            ]
            cmd = ["sudo", "yum", "install", "-y"] + packages
            returncode, _, stderr = run_command(cmd, capture=False, check=False)
            if returncode != 0:
                print_error(f"Failed to install prerequisites: {stderr}")
                return False
            print_success("Prerequisites installed successfully")
            return True

        elif distro_id in ["arch", "manjaro", "endeavouros"]:
            print_info("Installing prerequisites for Arch Linux...")
            packages = [
                "gcc",
                "git",
                "make",
                "flex",
                "bison",
                "gperf",
                "python",
                "cmake",
                "ninja",
                "ccache",
                "dfu-util",
                "libusb",
                "python-pip",
            ]
            cmd = ["sudo", "pacman", "-S", "--needed", "--noconfirm"] + packages
            returncode, _, stderr = run_command(cmd, capture=False, check=False)
            if returncode != 0:
                print_error(f"Failed to install prerequisites: {stderr}")
                return False
            print_success("Prerequisites installed successfully")
            return True

        else:
            print_warning(f"Unknown distribution: {distro_id}")
            print_info("Please install prerequisites manually.")
            return False

    elif system == "Darwin":
        print_info("Installing prerequisites for macOS...")

        # Check for Homebrew
        if check_command("brew"):
            print_info("Using Homebrew to install prerequisites...")
            packages = ["cmake", "ninja", "dfu-util", "ccache"]
            for pkg in packages:
                returncode, _, _ = run_command(
                    ["brew", "list", pkg], capture=True, check=False
                )
                if returncode != 0:
                    print_info(f"Installing {pkg}...")
                    run_command(["brew", "install", pkg], capture=False, check=False)
            print_success("Prerequisites installed successfully")
            return True

        # Check for MacPorts
        elif check_command("port"):
            print_info("Using MacPorts to install prerequisites...")
            packages = ["cmake", "ninja", "dfu-util", "ccache"]
            cmd = ["sudo", "port", "install"] + packages
            returncode, _, stderr = run_command(cmd, capture=False, check=False)
            if returncode != 0:
                print_error(f"Failed to install prerequisites: {stderr}")
                return False
            print_success("Prerequisites installed successfully")
            return True

        else:
            print_error("Neither Homebrew nor MacPorts found.")
            print_info(
                'Please install Homebrew: /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"'
            )
            return False

    elif system == "Windows":
        print_warning("Automatic installation not supported on Windows.")
        print_info("Please use the ESP-IDF Windows installer:")
        print_info("  https://dl.espressif.com/dl/esp-idf/")
        return False

    return False


def show_esp_idf_install_instructions() -> None:
    """Show instructions for installing ESP-IDF"""
    print_header("ESP-IDF Installation Required")

    print_info("ESP-IDF is not installed or not configured.")
    print_info("\nTo install ESP-IDF, follow these steps:\n")

    if platform.system() == "Linux":
        distro_id, _ = get_distro_info()
        print_info(f"For {distro_id}:")
        print_info("")
        print_info("1. Prerequisites are handled by this script")
        print_info("")
        print_info("2. Clone ESP-IDF:")
        print_info("   mkdir -p ~/esp")
        print_info("   cd ~/esp")
        print_info("   git clone --recursive https://github.com/espressif/esp-idf.git")
        print_info("")
        print_info("3. Install ESP-IDF tools:")
        print_info("   cd ~/esp/esp-idf")
        print_info("   ./install.sh esp32")
        print_info("")
        print_info("4. Set up environment (run this in every terminal session):")
        print_info("   . $HOME/esp/esp-idf/export.sh")
        print_info("")
        print_info("5. Add to ~/.bashrc for automatic setup:")
        print_info(
            "   echo 'alias get_idf=\". $HOME/esp/esp-idf/export.sh\"' >> ~/.bashrc"
        )

    elif platform.system() == "Darwin":
        print_info("1. Prerequisites are handled by this script")
        print_info("")
        print_info("2. Clone ESP-IDF:")
        print_info("   mkdir -p ~/esp")
        print_info("   cd ~/esp")
        print_info("   git clone --recursive https://github.com/espressif/esp-idf.git")
        print_info("")
        print_info("3. Install ESP-IDF tools:")
        print_info("   cd ~/esp/esp-idf")
        print_info("   ./install.sh esp32")
        print_info("")
        print_info("4. Set up environment (run this in every terminal session):")
        print_info("   . $HOME/esp/esp-idf/export.sh")
        print_info("")
        print_info("5. Add to ~/.zshrc or ~/.bashrc for automatic setup:")
        print_info(
            "   echo 'alias get_idf=\". $HOME/esp/esp-idf/export.sh\"' >> ~/.zshrc"
        )
        print_info("")
        print_info("Note: If using Apple M1/M2, you may need Rosetta 2:")
        print_info("   /usr/sbin/softwareupdate --install-rosetta --agree-to-license")

    elif platform.system() == "Windows":
        print_info("1. Download ESP-IDF Windows installer:")
        print_info("   https://dl.espressif.com/dl/esp-idf/")
        print_info("")
        print_info("2. Run the installer and follow the instructions")
        print_info("")
        print_info("3. The installer will set up everything including:")
        print_info("   - ESP-IDF framework")
        print_info("   - Toolchain")
        print_info("   - Python environment")
        print_info("   - ESP-IDF PowerShell/Command Prompt shortcut")

    print_info("\nFor detailed instructions, visit:")
    print_info(
        "  https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/"
    )
    print_info("")


def main() -> int:
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Install dependencies for embedded firmware"
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Force reinstall of components even if they exist",
    )
    parser.add_argument(
        "--skip-checks",
        action="store_true",
        help="Skip environment checks",
    )
    parser.add_argument(
        "--install-prerequisites",
        action="store_true",
        help="Install ESP-IDF prerequisites (requires sudo on Linux)",
    )

    args = parser.parse_args()

    print_header("Embedded - Dependency Installer")

    # Install prerequisites if requested
    if args.install_prerequisites:
        print_info("Installing ESP-IDF prerequisites...")
        if not install_esp_idf_prerequisites():
            print_error("Failed to install prerequisites")
            return 1

    # Check ESP-IDF
    if not args.skip_checks:
        print_info("Checking ESP-IDF installation...")
        idf_ok, idf_path = check_esp_idf()

        if not idf_ok:
            if idf_path:
                print_error(
                    f"ESP-IDF path is set ({idf_path}) but idf.py is not available"
                )
                print_info("Make sure to run: . $IDF_PATH/export.sh")
            else:
                print_error("ESP-IDF is not installed or not configured")
                print_info("")
                print_info("You can install prerequisites with:")
                print_info(f"  {sys.executable} {__file__} --install-prerequisites")
                print_info("")
                show_esp_idf_install_instructions()
            return 1

        print_success(f"ESP-IDF is configured: {idf_path}")

        # Check ESP-IDF version
        returncode, stdout, stderr = run_command(["idf.py", "--version"], capture=True)
        if returncode == 0:
            print_info(f"ESP-IDF version: {stdout.strip()}")

    # Check nanopb
    print_info("\nChecking nanopb component...")
    if check_nanopb() and not args.force:
        print_success("Nanopb component is installed")
    else:
        if not install_nanopb(force=args.force):
            print_error("Failed to install nanopb component")
            return 1

    # Check Python packages
    check_python_packages()

    # Summary
    print_header("Installation Summary")
    print_success("All required dependencies are available")
    print_info("\nNext steps:")
    print_info("  1. Configure the project: idf.py menuconfig")
    print_info("  2. Build the firmware: idf.py build")
    print_info("  3. Flash to device: idf.py -p PORT flash")
    print_info("")
    print_info("Or use the Makefile:")
    print_info("  make build")
    print_info("  make flash PORT=/dev/ttyUSB0")
    print_info("")
    print_info("To install prerequisites in the future:")
    print_info(f"  {sys.executable} {__file__} --install-prerequisites")
    print_info("")

    return 0


if __name__ == "__main__":
    sys.exit(main())
