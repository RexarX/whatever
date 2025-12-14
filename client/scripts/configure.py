#!/usr/bin/env python3
"""
Client Configuration Script

Cross-platform CMake configuration script for the project.

This script handles:
- Compiler detection and selection
- Build system configuration (Ninja, MSBuild, Make)
- CMake configuration with build options
- Conan dependency integration

This script should be called before building, or will be called automatically
by build.py.
"""

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple


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
    print(msg)


def print_success(msg: str) -> None:
    """Print success message in green"""
    print(f"{Colors.GREEN}{msg}{Colors.NC}")


def print_warning(msg: str) -> None:
    """Print warning message in yellow"""
    print(f"{Colors.YELLOW}{msg}{Colors.NC}")


def print_error(msg: str) -> None:
    """Print error message in red"""
    print(f"{Colors.RED}{msg}{Colors.NC}", file=sys.stderr)


def print_header(msg: str) -> None:
    """Print a header message"""
    print("=" * 60)
    print(msg)
    print("=" * 60)


def run_command(
    cmd: List[str], cwd: Optional[Path] = None, env: Optional[dict] = None
) -> Tuple[int, str, str]:
    """
    Run a command and return the exit code, stdout, and stderr

    Args:
        cmd: Command and arguments as a list
        cwd: Working directory for the command
        env: Environment variables for the command

    Returns:
        Tuple of (exit_code, stdout, stderr)
    """
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return result.returncode, result.stdout, result.stderr
    except FileNotFoundError:
        return 1, "", f"Command not found: {cmd[0]}"


def check_command(cmd: str) -> bool:
    """Check if a command is available in PATH"""
    return shutil.which(cmd) is not None


def ask_yes_no(prompt: str, default: bool = True, color: str = Colors.YELLOW) -> bool:
    """
    Ask user a yes/no question

    Args:
        prompt: Question to ask
        default: Default answer if user just presses Enter
        color: ANSI color code for prompt (default: yellow)

    Returns:
        True for yes, False for no
    """
    suffix = "[Y/n]" if default else "[y/N]"
    while True:
        try:
            response = input(f"{color}{prompt} {suffix} {Colors.NC}").strip().lower()
        except (KeyboardInterrupt, EOFError):
            print()
            return False

        if not response:
            return default
        if response in ("y", "yes"):
            return True
        if response in ("n", "no"):
            return False
        print("Please answer yes or no.")


def get_cpu_count() -> int:
    """Get the number of CPU cores"""
    try:
        return os.cpu_count() or 4
    except Exception:
        return 4


def read_cmake_cache(build_dir: Path) -> dict:
    """Read CMakeCache.txt and return a dictionary of cache variables"""
    cache_file = build_dir / "CMakeCache.txt"
    if not cache_file.exists():
        return {}

    cache = {}
    try:
        with open(cache_file, "r") as f:
            for line in f:
                line = line.strip()
                # Skip comments and empty lines
                if not line or line.startswith("#") or line.startswith("//"):
                    continue
                # Parse cache entries: NAME:TYPE=VALUE
                if "=" in line and ":" in line:
                    parts = line.split("=", 1)
                    if len(parts) == 2:
                        key_type = parts[0]
                        value = parts[1]
                        # Extract just the variable name (before the colon)
                        key = key_type.split(":")[0]
                        cache[key] = value
    except Exception as e:
        print_warning(f"Failed to read CMakeCache.txt: {e}")
        return {}

    return cache


class BuildConfig:
    """Configuration for the build"""

    def __init__(self):
        self.script_dir = Path(__file__).parent.resolve()
        self.project_root = self.script_dir.parent

        # Build configuration
        self.build_type = "Release"
        self.compiler = None
        self.build_system = None
        self.platform = self.get_platform_name()

        # Options
        self.build_tests = False
        self.use_conan = False
        self.clean = False
        self.interactive = True
        self.cmake_args = []

    def get_platform_name(self) -> str:
        """Get lowercase platform name for build directory"""
        system = platform.system()
        if system == "Windows":
            return "windows"
        elif system == "Linux":
            return "linux"
        elif system == "Darwin":
            return "macos"
        return "unknown"

    def get_build_dir(self) -> Path:
        """Get the build directory based on build type and platform"""
        build_type_lower = self.build_type.lower()
        return self.project_root / "build" / build_type_lower / self.platform

    def get_conan_dir(self) -> Path:
        """Get the Conan installation directory"""
        return self.get_build_dir()


class CompilerDetector:
    """Detects and sets up compilers"""

    def __init__(self):
        self.detected_compiler = None
        self.detected_variant = None

    def _setup_clang_cl_path(self) -> bool:
        """Ensure clang-cl is in PATH for Windows builds"""
        if platform.system() != "Windows":
            return True

        if check_command("clang-cl"):
            return True

        print_info("clang-cl not in PATH, searching common locations...")

        vs_paths = [
            Path("C:/Program Files/Microsoft Visual Studio/2022/Community"),
            Path("C:/Program Files/Microsoft Visual Studio/2022/Professional"),
            Path("C:/Program Files/Microsoft Visual Studio/2022/Enterprise"),
            Path("C:/Program Files (x86)/Microsoft Visual Studio/2019/Community"),
            Path("C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional"),
            Path("C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise"),
        ]

        for vs_path in vs_paths:
            if not vs_path.exists():
                continue

            clang_paths = [
                vs_path / "VC" / "Tools" / "Llvm" / "x64" / "bin",
                vs_path / "VC" / "Tools" / "Llvm" / "bin",
            ]

            for clang_path in clang_paths:
                clang_cl = clang_path / "clang-cl.exe"
                if clang_cl.exists():
                    print_success(f"Found clang-cl at: {clang_cl}")
                    os.environ["PATH"] = (
                        str(clang_path) + os.pathsep + os.environ["PATH"]
                    )

                    if check_command("clang-cl"):
                        print_success("clang-cl added to PATH successfully")
                        return True

        llvm_paths = [
            Path("C:/Program Files/LLVM/bin"),
            Path("C:/Program Files (x86)/LLVM/bin"),
        ]

        for llvm_path in llvm_paths:
            clang_cl = llvm_path / "clang-cl.exe"
            if clang_cl.exists():
                print_success(f"Found clang-cl at: {clang_cl}")
                os.environ["PATH"] = str(llvm_path) + os.pathsep + os.environ["PATH"]

                if check_command("clang-cl"):
                    print_success("clang-cl added to PATH successfully")
                    return True

        print_warning("clang-cl not found in common locations")
        return False

    def detect_msvc(self) -> Optional[str]:
        """Detect MSVC installation"""
        if platform.system() != "Windows":
            return None

        vswhere_path = (
            Path(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"))
            / "Microsoft Visual Studio"
            / "Installer"
            / "vswhere.exe"
        )

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
                if result.stdout.strip():
                    return "msvc"
            except subprocess.CalledProcessError:
                pass

        return None

    def setup_msvc_environment(self) -> dict:
        """Setup MSVC environment for building on Windows"""
        env = os.environ.copy()

        if platform.system() != "Windows":
            return env

        if check_command("cl") and "INCLUDE" in os.environ:
            print_info("MSVC environment already configured")
            return env

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
            return env

        vcvars = Path(vs_path) / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
        if not vcvars.exists():
            print_warning(f"vcvarsall.bat not found at {vcvars}")
            return env

        try:
            cmd = f'"{vcvars}" amd64 && set'
            proc = subprocess.run(
                cmd, shell=True, capture_output=True, text=True, check=True
            )

            for line in proc.stdout.splitlines():
                if "=" in line:
                    key, _, value = line.partition("=")
                    env[key] = value

            print_success("MSVC environment configured")
            return env
        except subprocess.CalledProcessError as e:
            print_warning(f"Failed to setup MSVC environment: {e}")
            return env

    def detect_gcc(self) -> bool:
        """Check if GCC is available"""
        return check_command("g++")

    def detect_clang(self) -> bool:
        """Check if Clang is available"""
        if platform.system() == "Windows":
            return check_command("clang-cl") or self._setup_clang_cl_path()
        return check_command("clang++")

    def select_compiler_interactive(self) -> Optional[str]:
        """Interactively select a compiler"""
        compilers = []

        # On Windows, prioritize MSVC as the default compiler
        if platform.system() == "Windows" and self.detect_msvc():
            compilers.append(("msvc", "MSVC"))
        if self.detect_clang():
            if platform.system() == "Windows":
                compilers.append(("clang-cl", "Clang-CL (Windows)"))
            else:
                compilers.append(("clang", "Clang"))
        if self.detect_gcc():
            compilers.append(("gcc", "GCC"))

        if not compilers:
            print_error("No compilers found!")
            return None

        print_info("Available compilers:")
        for i, (_, name) in enumerate(compilers, 1):
            print_info(f"  {i}. {name}")

        while True:
            try:
                choice = input(
                    f"{Colors.YELLOW}Select compiler [1]: {Colors.NC}"
                ).strip()
            except (KeyboardInterrupt, EOFError):
                print()
                return None

            if not choice:
                return compilers[0][0]

            try:
                idx = int(choice) - 1
                if 0 <= idx < len(compilers):
                    return compilers[idx][0]
            except ValueError:
                pass

            print_error("Invalid choice")

    def setup_compiler(self, compiler: str, config: BuildConfig) -> dict:
        """Setup compiler environment and return CMake arguments"""
        cmake_args = {}

        if compiler in ("gcc", "g++"):
            cmake_args["CMAKE_C_COMPILER"] = "gcc"
            cmake_args["CMAKE_CXX_COMPILER"] = "g++"
        elif compiler in ("clang", "clang++"):
            cmake_args["CMAKE_C_COMPILER"] = "clang"
            cmake_args["CMAKE_CXX_COMPILER"] = "clang++"
        elif compiler == "clang-cl":
            self._setup_clang_cl_path()
            cmake_args["CMAKE_C_COMPILER"] = "clang-cl"
            cmake_args["CMAKE_CXX_COMPILER"] = "clang-cl"
        elif compiler in ("msvc", "cl"):
            # MSVC doesn't need explicit compiler setting with VS generator
            pass

        return cmake_args


class BuildSystemSelector:
    """Selects and configures build systems"""

    def __init__(self):
        pass

    def detect_ninja(self) -> bool:
        """Check if Ninja is available"""
        return check_command("ninja")

    def detect_make(self) -> bool:
        """Check if Make is available"""
        return check_command("make")

    def detect_msbuild(self) -> bool:
        """Check if MSBuild is available (Windows)"""
        if platform.system() != "Windows":
            return False

        vswhere_path = (
            Path(os.environ.get("ProgramFiles(x86)", "C:\\Program Files (x86)"))
            / "Microsoft Visual Studio"
            / "Installer"
            / "vswhere.exe"
        )

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
                if result.stdout.strip():
                    return True
            except subprocess.CalledProcessError:
                pass

        return False

    def select_build_system_interactive(self, compiler: str) -> Optional[str]:
        """Interactively select a build system"""
        systems = []

        if self.detect_ninja():
            systems.append(("Ninja", "Ninja (fast)"))
        if platform.system() != "Windows" and self.detect_make():
            systems.append(("Unix Makefiles", "Unix Makefiles"))
        if platform.system() == "Windows" and self.detect_msbuild():
            if compiler in ("msvc", "cl"):
                systems.append(("Visual Studio 17 2022", "Visual Studio 2022"))

        if not systems:
            print_warning("No build systems found, using CMake default")
            return None

        # If only one option, use it
        if len(systems) == 1:
            print_info(f"Using build system: {systems[0][1]}")
            return systems[0][0]

        print_info("Available build systems:")
        for i, (_, name) in enumerate(systems, 1):
            print_info(f"  {i}. {name}")

        while True:
            try:
                choice = input(
                    f"{Colors.YELLOW}Select build system [1]: {Colors.NC}"
                ).strip()
            except (KeyboardInterrupt, EOFError):
                print()
                return None

            if not choice:
                return systems[0][0]

            try:
                idx = int(choice) - 1
                if 0 <= idx < len(systems):
                    return systems[idx][0]
            except ValueError:
                pass

            print_error("Invalid choice")

    def setup_build_system(self, build_system: str) -> List[str]:
        """Return CMake generator arguments"""
        if build_system:
            return ["-G", build_system]
        return []


class ConanManager:
    """Manages Conan dependency installation"""

    def __init__(self):
        pass

    def find_conan_toolchain(self, config: BuildConfig) -> Optional[Path]:
        """Find existing Conan toolchain file"""
        build_dir = config.get_build_dir()
        toolchain = build_dir / "conan_toolchain.cmake"
        if toolchain.exists():
            return toolchain
        return None

    def ask_use_conan(self, interactive: bool = True) -> bool:
        """Ask user if they want to use Conan"""
        if not interactive:
            return False

        if not check_command("conan"):
            print_info("Conan not found. Will use system packages or CPM.")
            return False

        return ask_yes_no("Use Conan for dependencies?", default=False)

    def setup_conan(self, config: BuildConfig) -> bool:
        """Install Conan dependencies"""
        print_header("Installing Conan Dependencies")

        if not check_command("conan"):
            print_error("Conan not found. Please install Conan first:")
            print_info("  pip install conan")
            return False

        build_dir = config.get_build_dir()
        build_dir.mkdir(parents=True, exist_ok=True)

        # Determine Conan profile
        # Check if we have a custom profile for this compiler
        custom_profile = config.project_root / "conan_profiles" / "gcc15"
        if custom_profile.exists() and config.compiler in ("gcc", "g++", None):
            # Check GCC version
            try:
                result = subprocess.run(
                    ["gcc", "--version"],
                    capture_output=True,
                    text=True,
                    check=True,
                )
                if "gcc (GCC) 15" in result.stdout or "gcc (GCC) 16" in result.stdout:
                    profile = str(custom_profile)
                    print_info(f"Using custom GCC 15+ profile: {profile}")
                else:
                    profile = "default"
            except (subprocess.CalledProcessError, FileNotFoundError):
                profile = "default"
        else:
            profile = "default"

        # Build type mapping
        # Use Release for dependencies when building RelWithDebInfo to avoid recompiling everything
        build_type = config.build_type
        if build_type.lower() == "relwithdebinfo":
            build_type = "RelWithDebInfo"
            # Use Release dependencies for RelWithDebInfo builds
            conan_build_type = "Release"
            print_info(
                "Using Release dependencies for RelWithDebInfo build (saves compilation time)"
            )
        else:
            conan_build_type = build_type

        # Run conan install
        # For large packages like Qt, use pre-built binaries only
        # Build from source only for smaller packages
        cmd = [
            "conan",
            "install",
            str(config.project_root),
            "--output-folder",
            str(build_dir),
            "--build=missing",
            "--build=!qt/*",  # Never build Qt from source, use pre-built binaries only
            f"--profile={profile}",
            f"-s:h=build_type={conan_build_type}",
        ]

        print_info(f"Running: {' '.join(cmd)}")

        result = subprocess.run(cmd, cwd=config.project_root)

        if result.returncode != 0:
            print_error("Conan dependency installation failed")
            return False

        print_success("Conan dependencies installed successfully")
        return True


class CMakeBuilder:
    """Handles CMake configuration and building"""

    def __init__(self):
        pass

    def select_build_type_interactive(self) -> str:
        """Interactively select build type"""
        print_info("Select build type:")
        print_info("  1. Debug")
        print_info("  2. RelWithDebInfo")
        print_info("  3. Release")

        while True:
            try:
                choice = input(
                    f"{Colors.YELLOW}Select build type [3]: {Colors.NC}"
                ).strip()
            except (KeyboardInterrupt, EOFError):
                print()
                return "Release"

            if not choice or choice == "3":
                return "Release"
            elif choice == "1":
                return "Debug"
            elif choice == "2":
                return "RelWithDebInfo"

            print_error("Invalid choice")

    def select_tests_interactive(self) -> bool:
        """Interactively select whether to build tests"""
        return ask_yes_no("Build tests?", default=False)

    def configure(self, config: BuildConfig) -> bool:
        """Configure CMake"""
        print_header("Configuring CMake")

        build_dir = config.get_build_dir()
        build_dir.mkdir(parents=True, exist_ok=True)

        # Build CMake command
        cmd = ["cmake"]

        # Generator
        build_system_selector = BuildSystemSelector()
        cmd.extend(build_system_selector.setup_build_system(config.build_system))

        # Compiler setup
        compiler_detector = CompilerDetector()
        compiler_args = compiler_detector.setup_compiler(config.compiler, config)
        for key, value in compiler_args.items():
            cmd.append(f"-D{key}={value}")

        # Build type
        cmd.append(f"-DCMAKE_BUILD_TYPE={config.build_type}")

        # Options
        cmd.append(f"-DCLIENT_BUILD_TESTS={'ON' if config.build_tests else 'OFF'}")

        # Conan
        if config.use_conan:
            conan_manager = ConanManager()
            toolchain = conan_manager.find_conan_toolchain(config)
            if toolchain:
                cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
                cmd.append("-DCLIENT_USE_CONAN=ON")
            else:
                print_warning("Conan toolchain not found. Run install_deps.py first.")
        else:
            cmd.append("-DCLIENT_USE_CONAN=OFF")

        # Additional CMake args
        cmd.extend(config.cmake_args)

        # Source and build directories
        cmd.extend(["-S", str(config.project_root), "-B", str(build_dir)])

        print_info(f"Running: {' '.join(cmd)}")

        # Setup environment for MSVC
        env = os.environ.copy()
        if platform.system() == "Windows" and config.compiler in ("msvc", "cl", None):
            env = compiler_detector.setup_msvc_environment()

        result = subprocess.run(cmd, cwd=config.project_root, env=env)

        if result.returncode != 0:
            print_error("CMake configuration failed")
            return False

        print_success("CMake configuration successful")
        return True


def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Configure script for Client",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive configuration
  python configure.py

  # Quick release build
  python configure.py --type Release --no-interactive

  # Debug build with tests
  python configure.py --type Debug --tests

  # Use Conan for dependencies
  python configure.py --use-conan

  # Specify compiler
  python configure.py --compiler gcc
""",
    )

    parser.add_argument(
        "-t",
        "--type",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="Build type",
    )

    parser.add_argument(
        "-c",
        "--compiler",
        choices=["gcc", "clang", "clang-cl", "msvc", "cl"],
        help="Compiler to use",
    )

    parser.add_argument(
        "-b",
        "--build-system",
        choices=["Ninja", "Unix Makefiles", "Visual Studio 17 2022"],
        help="CMake generator",
    )

    parser.add_argument(
        "--platform",
        choices=["linux", "windows", "macos"],
        help="Target platform (default: auto-detect)",
    )

    parser.add_argument(
        "--tests",
        dest="build_tests",
        action="store_true",
        help="Enable tests",
    )

    parser.add_argument(
        "--no-tests",
        dest="build_tests",
        action="store_false",
        help="Disable tests",
    )

    parser.add_argument(
        "--use-conan",
        dest="use_conan",
        action="store_true",
        help="Use Conan for dependencies",
    )

    parser.add_argument(
        "--no-conan",
        dest="use_conan",
        action="store_false",
        help="Don't use Conan (use system packages or CPM)",
    )

    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build directory before configuring",
    )

    parser.add_argument(
        "--no-interactive",
        action="store_true",
        help="Don't prompt for input, use defaults or command line args",
    )

    parser.add_argument(
        "--cmake-args",
        type=str,
        default="",
        help="Additional CMake arguments",
    )

    parser.set_defaults(build_tests=None, use_conan=None)

    return parser.parse_args()


def main() -> int:
    """Main entry point"""
    args = parse_arguments()

    print_header("Client Configuration")

    config = BuildConfig()
    config.interactive = not args.no_interactive
    config.clean = args.clean

    if args.cmake_args:
        config.cmake_args = args.cmake_args.split()

    if args.platform:
        config.platform = args.platform

    # Compiler detection and selection
    compiler_detector = CompilerDetector()
    build_system_selector = BuildSystemSelector()
    conan_manager = ConanManager()
    cmake_builder = CMakeBuilder()

    # Build type
    if args.type:
        config.build_type = args.type
    elif config.interactive:
        config.build_type = cmake_builder.select_build_type_interactive()
    else:
        config.build_type = "Release"

    # Compiler
    if args.compiler:
        config.compiler = args.compiler
    elif config.interactive:
        config.compiler = compiler_detector.select_compiler_interactive()
        if config.compiler is None:
            return 1
    else:
        # Auto-detect compiler
        if compiler_detector.detect_gcc():
            config.compiler = "gcc"
        elif compiler_detector.detect_clang():
            config.compiler = "clang" if platform.system() != "Windows" else "clang-cl"
        elif platform.system() == "Windows" and compiler_detector.detect_msvc():
            config.compiler = "msvc"
        else:
            print_error("No compiler found")
            return 1

    # Build system
    if args.build_system:
        config.build_system = args.build_system
    elif config.interactive:
        config.build_system = build_system_selector.select_build_system_interactive(
            config.compiler
        )
    else:
        # Auto-select build system
        if build_system_selector.detect_ninja():
            config.build_system = "Ninja"
        elif platform.system() != "Windows":
            config.build_system = "Unix Makefiles"
        elif config.compiler in ("msvc", "cl"):
            config.build_system = "Visual Studio 17 2022"
        else:
            config.build_system = "Ninja"

    # Tests
    if args.build_tests is not None:
        config.build_tests = args.build_tests
    elif config.interactive:
        config.build_tests = cmake_builder.select_tests_interactive()
    else:
        config.build_tests = False

    # Conan
    if args.use_conan is not None:
        config.use_conan = args.use_conan
    elif config.interactive:
        config.use_conan = conan_manager.ask_use_conan(config.interactive)
    else:
        config.use_conan = False

    # Print configuration summary
    print_info("")
    print_header("Configuration Summary")
    print_info(f"  Build Type:    {config.build_type}")
    print_info(f"  Compiler:      {config.compiler}")
    print_info(f"  Build System:  {config.build_system}")
    print_info(f"  Platform:      {config.platform}")
    print_info(f"  Build Tests:   {config.build_tests}")
    print_info(f"  Use Conan:     {config.use_conan}")
    print_info(f"  Build Dir:     {config.get_build_dir()}")
    print_info("")

    # Clean build directory if requested (before Conan setup)
    if config.clean:
        build_dir = config.get_build_dir()
        if build_dir.exists():
            print_info(f"Cleaning build directory: {build_dir}")
            shutil.rmtree(build_dir)

    # Install Conan dependencies if requested
    if config.use_conan:
        if not conan_manager.setup_conan(config):
            return 1

    # Configure CMake
    if not cmake_builder.configure(config):
        return 1

    print_success("Configuration complete!")
    print_info("")
    print_info("To build the project, run:")
    print_info("  python scripts/build.py")
    print_info("")

    return 0


if __name__ == "__main__":
    sys.exit(main())
