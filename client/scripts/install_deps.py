#!/usr/bin/env python3
"""Client Dependency Installer

This script handles installation of dependencies for the Client.
It can use Conan or system packages based on user preference.

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
    live_output: bool = False,
) -> Tuple[int, str, str]:
    """Run a command and return exit code, stdout, stderr"""
    try:
        if live_output:
            result = subprocess.run(cmd, cwd=cwd, check=check)
            return result.returncode, "", ""
        else:
            result = subprocess.run(
                cmd, cwd=cwd, capture_output=True, text=True, check=check
            )
            return result.returncode, result.stdout, result.stderr
    except subprocess.CalledProcessError as e:
        if live_output:
            return e.returncode, "", ""
        return e.returncode, e.stdout, e.stderr
    except FileNotFoundError:
        return 127, "", f"Command not found: {cmd[0]}"


def check_command(command: str) -> bool:
    """Check if a command is available in PATH"""
    return shutil.which(command) is not None


def ask_yes_no(question: str, default: bool = True, interactive: bool = True) -> bool:
    """Ask a yes/no question"""
    if not interactive:
        return default

    default_str = "Y/n" if default else "y/N"
    while True:
        response = (
            input(f"{Colors.YELLOW}{question} [{default_str}]: {Colors.END}")
            .strip()
            .lower()
        )
        if not response:
            return default
        if response in ("y", "yes"):
            return True
        if response in ("n", "no"):
            return False
        print_error("Please answer 'y' or 'n'")


class DependencyInstaller:
    """Main dependency installer class"""

    def __init__(self, args):
        self.args = args
        self.system = platform.system()
        self.interactive = not args.no_interactive
        self.project_root = Path(__file__).parent.parent.resolve()
        self.package_manager = self._detect_package_manager()
        self._msvc_env = None

    def get_platform_name(self) -> str:
        """Get lowercase platform name for build directory"""
        if self.system == "Windows":
            return "windows"
        elif self.system == "Linux":
            return "linux"
        elif self.system == "Darwin":
            return "macos"
        return "unknown"

    def get_build_dir(self, build_type: str) -> Path:
        """Get the build directory based on build type and platform"""
        build_type_lower = build_type.lower()
        platform_name = self.get_platform_name()
        return self.project_root / "build" / build_type_lower / platform_name

    def get_conan_dir(self, build_type: str) -> Path:
        """Get the Conan installation directory"""
        return self.get_build_dir(build_type)

    def _detect_package_manager(self) -> Optional[str]:
        """Detect available system package manager"""
        managers = {
            "apt": "apt-get",
            "dnf": "dnf",
            "yum": "yum",
            "pacman": "pacman",
            "zypper": "zypper",
            "brew": "brew",
            "choco": "choco",
            "scoop": "scoop",
        }

        for name, cmd in managers.items():
            if check_command(cmd):
                return name

        return None

    def _setup_msvc_environment(self) -> dict:
        """Setup MSVC environment for building on Windows."""
        if self._msvc_env is not None:
            return self._msvc_env

        env = os.environ.copy()

        if self.system != "Windows":
            return env

        # Check if we already have MSVC environment
        if check_command("cl") and "INCLUDE" in os.environ:
            print_info("MSVC environment already configured")
            self._msvc_env = env
            return env

        # Try to locate vswhere
        vswhere_path = (
            Path(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"))
            / "Microsoft Visual Studio"
            / "Installer"
            / "vswhere.exe"
        )
        fallback_vswhere = (
            Path(os.environ.get("ProgramFiles", "C:\\Program Files"))
            / "Microsoft Visual Studio"
            / "Installer"
            / "vswhere.exe"
        )
        if not vswhere_path.exists() and fallback_vswhere.exists():
            vswhere_path = fallback_vswhere

        vs_path = ""
        if vswhere_path.exists():
            try:
                result = subprocess.run(
                    [
                        str(vswhere_path),
                        "-latest",
                        "-products",
                        "*",
                        "-requires",
                        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                        "-property",
                        "installationPath",
                    ],
                    capture_output=True,
                    text=True,
                    check=True,
                )
                vs_path = result.stdout.strip()
            except subprocess.CalledProcessError:
                pass

        # Fallback to common paths
        if not vs_path:
            common_paths = [
                Path("C:/Program Files/Microsoft Visual Studio/2022/Community"),
                Path("C:/Program Files/Microsoft Visual Studio/2022/Professional"),
                Path("C:/Program Files/Microsoft Visual Studio/2022/Enterprise"),
                Path("C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"),
                Path(
                    "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional"
                ),
                Path("C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise"),
            ]
            for p in common_paths:
                if p.exists():
                    vs_path = str(p)
                    break

        if not vs_path:
            print_warning("Visual Studio installation not found")
            self._msvc_env = env
            return env

        # Find vcvarsall.bat
        vcvars = Path(vs_path) / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
        if not vcvars.exists():
            print_warning(f"vcvarsall.bat not found at {vcvars}")
            self._msvc_env = env
            return env

        # Run vcvarsall.bat and capture environment
        try:
            cmd = f'"{vcvars}" amd64 && set'
            proc = subprocess.run(
                cmd, shell=True, capture_output=True, text=True, check=True
            )

            for line in proc.stdout.splitlines():
                if "=" in line:
                    key, _, value = line.partition("=")
                    env[key] = value

            print_success("MSVC environment configured for Conan build")
            self._msvc_env = env
            return env
        except subprocess.CalledProcessError as e:
            print_warning(f"Failed to setup MSVC environment: {e}")
            self._msvc_env = env
            return env

    def check_python_version(self) -> bool:
        """Check if Python version is sufficient"""
        version = sys.version_info
        if version.major < 3 or (version.major == 3 and version.minor < 8):
            print_error(f"Python 3.8+ required, found {version.major}.{version.minor}")
            return False
        print_success(f"Python {version.major}.{version.minor}.{version.micro}")
        return True

    def check_cmake(self) -> bool:
        """Check if CMake is installed"""
        if not check_command("cmake"):
            print_error("CMake not found. Please install CMake 3.25 or higher")
            return False

        returncode, stdout, _ = run_command(["cmake", "--version"], check=False)
        if returncode == 0:
            version_line = stdout.split("\n")[0]
            print_success(f"CMake: {version_line}")
        return True

    def check_git(self) -> bool:
        """Check if Git is installed"""
        if not check_command("git"):
            print_error("Git not found. Please install Git")
            return False

        returncode, stdout, _ = run_command(["git", "--version"], check=False)
        if returncode == 0:
            print_success(f"Git: {stdout.strip()}")
        return True

    def check_compiler(self) -> bool:
        """Check if a suitable C++ compiler is available"""
        compilers = []

        if check_command("g++"):
            returncode, stdout, _ = run_command(["g++", "--version"], check=False)
            if returncode == 0:
                compilers.append(
                    f"g++: {stdout.split()[2] if len(stdout.split()) > 2 else 'found'}"
                )

        if check_command("clang++"):
            returncode, stdout, _ = run_command(["clang++", "--version"], check=False)
            if returncode == 0:
                lines = stdout.split("\n")
                if lines:
                    compilers.append(f"clang++: {lines[0]}")

        if check_command("clang-cl"):
            compilers.append("Clang-CL (clang-cl)")

        if check_command("cl") or check_command("cl.exe"):
            compilers.append("MSVC (cl.exe)")

        if not compilers:
            print_error("No C++ compiler found")
            print_info("Please install one of the following:")
            if self.system == "Linux":
                print_info("  - GCC: sudo apt install g++ (Debian/Ubuntu)")
                print_info("  - GCC: sudo dnf install gcc-c++ (Fedora)")
                print_info("  - GCC: sudo pacman -S gcc (Arch)")
                print_info("  - Clang: sudo apt install clang (Debian/Ubuntu)")
            elif self.system == "Darwin":
                print_info("  - Clang: xcode-select --install")
                print_info("  - GCC: brew install gcc")
            elif self.system == "Windows":
                print_info("  - MSVC: Install Visual Studio 2022")
                print_info("  - Clang: choco install llvm")
            return False

        for compiler in compilers:
            print_success(compiler)

        return True

    def check_qt(self) -> bool:
        """Check if Qt is installed"""
        # Try to find qmake or Qt6
        if check_command("qmake"):
            returncode, stdout, _ = run_command(["qmake", "--version"], check=False)
            if returncode == 0:
                print_success(f"Qt found: {stdout.strip()}")
                return True
        if check_command("qmake6"):
            returncode, stdout, _ = run_command(["qmake6", "--version"], check=False)
            if returncode == 0:
                print_success(f"Qt6 found: {stdout.strip()}")
                return True

        print_warning("Qt not found via qmake. CMake may still find it.")
        return True  # Don't fail, CMake might find it

    def check_opencv(self) -> bool:
        """Check if OpenCV is installed"""
        # Check via pkg-config
        if check_command("pkg-config"):
            returncode, stdout, _ = run_command(
                ["pkg-config", "--modversion", "opencv4"], check=False
            )
            if returncode == 0:
                print_success(f"OpenCV 4: {stdout.strip()}")
                return True

            # Try opencv (without version suffix)
            returncode, stdout, _ = run_command(
                ["pkg-config", "--modversion", "opencv"], check=False
            )
            if returncode == 0:
                print_success(f"OpenCV: {stdout.strip()}")
                return True

        print_warning("OpenCV not found via pkg-config. CMake may still find it.")
        return True  # Don't fail, CMake might find it

    def install_conan(self) -> bool:
        """Install or verify Conan installation"""
        if check_command("conan"):
            returncode, stdout, _ = run_command(["conan", "--version"], check=False)
            if returncode == 0:
                print_success(f"Conan: {stdout.strip()}")
                return True

        if not self.args.install_conan:
            if not ask_yes_no(
                "Conan not found. Install it now?",
                default=True,
                interactive=self.interactive,
            ):
                return False

        print_info("Installing Conan via pip...")
        cmd = [sys.executable, "-m", "pip", "install", "--user", "conan"]
        returncode, _, stderr = run_command(cmd, check=False)

        if returncode != 0:
            print_error(f"Failed to install Conan: {stderr}")
            return False

        print_success("Conan installed successfully")
        return True

    def setup_conan_profile(self) -> bool:
        """Setup Conan profile"""
        print_info("Setting up Conan profile...")

        # Detect default profile
        returncode, _, _ = run_command(
            ["conan", "profile", "detect", "--force"], check=False
        )

        if returncode != 0:
            print_warning("Failed to detect Conan profile, using default")

        print_success("Conan profile configured")
        return True

    def install_conan_dependencies(self, build_type: str) -> bool:
        """Install dependencies via Conan"""
        print_header("Installing Conan Dependencies")

        build_dir = self.get_conan_dir(build_type)
        build_dir.mkdir(parents=True, exist_ok=True)

        # Build type mapping
        conan_build_type = build_type
        if build_type.lower() == "relwithdebinfo":
            conan_build_type = "RelWithDebInfo"
        elif build_type.lower() == "debug":
            conan_build_type = "Debug"
        elif build_type.lower() == "release":
            conan_build_type = "Release"

        # Run conan install
        cmd = [
            "conan",
            "install",
            str(self.project_root),
            "--output-folder",
            str(build_dir),
            "--build=missing",
            f"-s:h=build_type={conan_build_type}",
        ]

        print_info(f"Running: {' '.join(cmd)}")

        # Setup environment for MSVC if on Windows
        env = os.environ.copy()
        if self.system == "Windows":
            env = self._setup_msvc_environment()

        result = subprocess.run(cmd, cwd=self.project_root, env=env)

        if result.returncode != 0:
            print_error("Conan dependency installation failed")
            return False

        print_success("Conan dependencies installed successfully")
        return True

    def install_system_dependencies(self) -> bool:
        """Install system dependencies"""
        print_header("Checking System Dependencies")

        # Ask if user wants to auto-install
        auto_install = False
        if self.interactive and self.package_manager in [
            "apt",
            "pacman",
            "dnf",
            "brew",
        ]:
            auto_install = ask_yes_no(
                "Do you want to automatically install system dependencies?",
                default=True,
                interactive=self.interactive,
            )

        if self.package_manager == "apt":
            packages = [
                "build-essential",
                "cmake",
                "ninja-build",
                "git",
                "pkg-config",
                "libboost-all-dev",
                "libopencv-dev",
                "qt6-base-dev",
                "qt6-multimedia-dev",
                "qt6-serialport-dev",
                "qt6-connectivity-dev",
                "libprotobuf-dev",
                "protobuf-compiler",
            ]

            if auto_install:
                print_info("Installing packages for Debian/Ubuntu...")
                cmd = ["sudo", "apt-get", "install", "-y"] + packages
                returncode, _, stderr = run_command(cmd, live_output=True, check=False)
                if returncode != 0:
                    print_error(f"Failed to install packages: {stderr}")
                    return False
                print_success("Packages installed successfully")
            else:
                print_info(f"Recommended packages for Debian/Ubuntu:")
                print_info(f"  sudo apt-get install {' '.join(packages)}")

        elif self.package_manager == "pacman":
            packages = [
                "base-devel",
                "cmake",
                "ninja",
                "git",
                "pkg-config",
                "boost",
                "boost-libs",
                "opencv",
                "qt6-base",
                "qt6-multimedia",
                "qt6-serialport",
                "qt6-connectivity",
                "protobuf",
            ]

            if auto_install:
                print_info("Installing packages for Arch Linux...")
                cmd = ["sudo", "pacman", "-S", "--needed", "--noconfirm"] + packages
                returncode, _, stderr = run_command(cmd, live_output=True, check=False)
                if returncode != 0:
                    print_error(f"Failed to install packages: {stderr}")
                    return False
                print_success("Packages installed successfully")
            else:
                print_info(f"Recommended packages for Arch Linux:")
                print_info(f"  sudo pacman -S {' '.join(packages)}")

        elif self.package_manager == "dnf":
            packages = [
                "gcc",
                "gcc-c++",
                "cmake",
                "ninja-build",
                "git",
                "pkg-config",
                "boost-devel",
                "opencv-devel",
                "qt6-qtbase-devel",
                "qt6-qtmultimedia-devel",
                "qt6-qtserialport-devel",
                "qt6-connectivity-devel",
                "protobuf-devel",
                "protobuf-compiler",
            ]

            if auto_install:
                print_info("Installing packages for Fedora/RHEL...")
                cmd = ["sudo", "dnf", "install", "-y"] + packages
                returncode, _, stderr = run_command(cmd, live_output=True, check=False)
                if returncode != 0:
                    print_error(f"Failed to install packages: {stderr}")
                    return False
                print_success("Packages installed successfully")
            else:
                print_info(f"Recommended packages for Fedora/RHEL:")
                print_info(f"  sudo dnf install {' '.join(packages)}")

        elif self.package_manager == "brew":
            packages = [
                "cmake",
                "ninja",
                "boost",
                "opencv",
                "qt6",
                "protobuf",
            ]

            if auto_install:
                print_info("Installing packages for macOS...")
                for pkg in packages:
                    returncode, _, _ = run_command(["brew", "list", pkg], check=False)
                    if returncode != 0:
                        print_info(f"Installing {pkg}...")
                        run_command(
                            ["brew", "install", pkg], live_output=True, check=False
                        )
                print_success("Packages installed successfully")
            else:
                print_info(f"Recommended packages for macOS:")
                print_info(f"  brew install {' '.join(packages)}")

        else:
            print_info("Please install the following dependencies manually:")
            print_info("  - Build tools (cmake, ninja, git, pkg-config)")
            print_info("  - boost")
            print_info("  - OpenCV 4")
            print_info(
                "  - Qt6 (with QtMultimedia, QtSerialPort and QtConnectivity modules)"
            )
            print_info("  - Protobuf (libprotobuf + protoc compiler)")

        return True

    def run(self) -> int:
        """Run the dependency installation"""
        print_header("Client Dependency Installer")

        # Check prerequisites
        print_header("Checking Prerequisites")

        if not self.check_python_version():
            return 1

        if not self.check_cmake():
            return 1

        if not self.check_git():
            return 1

        if not self.check_compiler():
            return 1

        # Check optional dependencies
        self.check_qt()
        self.check_opencv()

        # Determine build type
        build_type = self.args.build_type or "Release"

        # Install dependencies
        if self.args.use_conan:
            # Install Conan if needed
            if not self.install_conan():
                return 1

            # Setup Conan profile
            if not self.setup_conan_profile():
                return 1

            # Install Conan dependencies
            if not self.install_conan_dependencies(build_type):
                return 1
        else:
            # Show system dependency information
            self.install_system_dependencies()

        print_header("Installation Complete")
        print_success("All dependencies are ready!")
        print_info("")
        print_info("Next steps:")
        print_info("  1. Configure the project:")
        print_info("     python scripts/configure.py")
        print_info("")
        print_info("  2. Build the project:")
        print_info("     python scripts/build.py")
        print_info("")

        return 0


def main() -> int:
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Install dependencies for Client",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive installation
  python install_deps.py

  # Use Conan for dependencies
  python install_deps.py --use-conan

  # Non-interactive with specific build type
  python install_deps.py --use-conan --build-type Release --no-interactive

  # Just check dependencies without installing
  python install_deps.py --check-only
""",
    )

    parser.add_argument(
        "--use-conan",
        action="store_true",
        help="Use Conan to install dependencies",
    )

    parser.add_argument(
        "--build-type",
        choices=["Debug", "Release", "RelWithDebInfo"],
        default="Release",
        help="Build type for Conan dependencies (default: Release)",
    )

    parser.add_argument(
        "--install-conan",
        action="store_true",
        help="Automatically install Conan if not found",
    )

    parser.add_argument(
        "--no-interactive",
        action="store_true",
        help="Don't prompt for input, use defaults",
    )

    parser.add_argument(
        "--check-only",
        action="store_true",
        help="Only check dependencies, don't install anything",
    )

    args = parser.parse_args()

    installer = DependencyInstaller(args)

    if args.check_only:
        print_header("Checking Dependencies")
        installer.check_python_version()
        installer.check_cmake()
        installer.check_git()
        installer.check_compiler()
        installer.check_qt()
        installer.check_opencv()
        return 0

    return installer.run()


if __name__ == "__main__":
    sys.exit(main())
