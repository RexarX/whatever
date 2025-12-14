#!/usr/bin/env python3
"""
Android build helper for the Qt client.

Features:
- Validates Android SDK/NDK/Qt for Android env vars.
- Allows overriding paths via CLI flags.
- Uses existing CMake presets (android-qt-*-arm64).
- Runs configure and build steps.
- CMake handles all deployment via qt_finalize_target() automatically.
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import Dict, Optional, Tuple

DEFAULT_PRESET_DEBUG = "android-qt-debug-arm64"
DEFAULT_PRESET_RELEASE = "android-qt-release-arm64"
DEFAULT_ANDROID_API = "24"
DEFAULT_OPENCV_ANDROID_SDK = "/home/vergil/android/opencv-4.12.0/"


class Colors:
    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    YELLOW = "\033[1;33m"
    NC = "\033[0m"

    @classmethod
    def disable(cls) -> None:
        cls.RED = cls.GREEN = cls.YELLOW = cls.NC = ""


if sys.platform == "win32":
    try:
        import ctypes  # type: ignore

        kernel32 = ctypes.windll.kernel32
        kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)
    except Exception:
        Colors.disable()


def info(msg: str) -> None:
    print(msg)


def success(msg: str) -> None:
    print(f"{Colors.GREEN}{msg}{Colors.NC}")


def warn(msg: str) -> None:
    print(f"{Colors.YELLOW}{msg}{Colors.NC}")


def error(msg: str) -> None:
    print(f"{Colors.RED}{msg}{Colors.NC}", file=sys.stderr)


def run(
    cmd: list, cwd: Optional[Path] = None, env: Optional[Dict[str, str]] = None
) -> int:
    info(f"[cmd] {' '.join(cmd)}")
    return subprocess.call(cmd, cwd=cwd, env=env)


# Note: androiddeployqt is now called automatically by CMake via qt_finalize_target()
# This script no longer needs to manually invoke it


def resolve_path(path: Optional[str]) -> Optional[str]:
    if not path:
        return None
    expanded = os.path.expanduser(os.path.expandvars(path))
    return os.path.abspath(expanded)


def detect_env() -> Dict[str, Optional[str]]:
    return {
        "ANDROID_SDK_ROOT": os.environ.get("ANDROID_SDK_ROOT"),
        "ANDROID_NDK_ROOT": os.environ.get("ANDROID_NDK_ROOT"),
        "QT_ANDROID_DIR": os.environ.get("QT_ANDROID_DIR"),
        # Optional: host Qt directory (e.g. .../Qt/<ver>/gcc_64). Used to locate androiddeployqt.
        "QT_HOST_DIR": os.environ.get("QT_HOST_DIR"),
        "OPENCV_ANDROID_SDK": os.environ.get(
            "OPENCV_ANDROID_SDK", DEFAULT_OPENCV_ANDROID_SDK
        ),
    }


def validate_paths(
    sdk: Optional[str],
    ndk: Optional[str],
    qt: Optional[str],
    opencv: Optional[str],
    qt_host: Optional[str],
) -> Tuple[bool, Dict[str, str]]:
    values: Dict[str, str] = {}
    ok = True

    def _check(label: str, value: Optional[str], must_exist: bool = True) -> None:
        nonlocal ok
        if value:
            resolved = resolve_path(value)
            if must_exist and (not resolved or not Path(resolved).exists()):
                error(f"{label} path does not exist: {value}")
                ok = False
            elif resolved:
                values[label] = resolved
        else:
            warn(f"{label} is not set")

    _check("ANDROID_SDK_ROOT", sdk)
    _check("ANDROID_NDK_ROOT", ndk)
    _check("QT_ANDROID_DIR", qt)
    # Optional (do not fail if unset): host Qt dir used to locate androiddeployqt
    _check("QT_HOST_DIR", qt_host, must_exist=True) if qt_host else None
    _check("OPENCV_ANDROID_SDK", opencv)

    if ok:
        success("Environment validated for Android build")
    return ok, values


# Note: local.properties is now generated automatically by CMake/Qt deployment
# when qt_finalize_target() is called


def ensure_preset(preset: str) -> None:
    if preset not in (DEFAULT_PRESET_DEBUG, DEFAULT_PRESET_RELEASE):
        warn(f"Using custom preset: {preset}")
    else:
        success(f"Using preset: {preset}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Android helper for Qt client")
    parser.add_argument(
        "--preset",
        default=DEFAULT_PRESET_RELEASE,
        help="CMake configure/build preset (default: %(default)s)",
    )
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="Skip configure step, just build preset",
    )
    parser.add_argument("--sdk", help="Override ANDROID_SDK_ROOT path")
    parser.add_argument("--ndk", help="Override ANDROID_NDK_ROOT path")
    parser.add_argument("--qt", help="Override QT_ANDROID_DIR path")
    parser.add_argument(
        "--qt-host",
        help="Optional: override QT_HOST_DIR (host Qt kit, e.g. .../Qt/<ver>/gcc_64). Used to find androiddeployqt.",
    )
    parser.add_argument("--opencv", help="Override OPENCV_ANDROID_SDK path")
    parser.add_argument(
        "--api",
        default=DEFAULT_ANDROID_API,
        help="Android API level (default: %(default)s)",
    )
    parser.add_argument(
        "--jobs", "-j", type=int, help="Parallel build jobs for cmake --build"
    )
    args, unknown = parser.parse_known_args()

    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    os.chdir(project_root)

    env_raw = detect_env()
    sdk = args.sdk or env_raw["ANDROID_SDK_ROOT"]
    ndk = args.ndk or env_raw["ANDROID_NDK_ROOT"]
    qt = args.qt or env_raw["QT_ANDROID_DIR"]
    qt_host = args.qt_host or env_raw["QT_HOST_DIR"]
    opencv = args.opencv or env_raw["OPENCV_ANDROID_SDK"]

    ok, resolved = validate_paths(sdk, ndk, qt, opencv, qt_host)
    if not ok:
        return 1

    env = os.environ.copy()
    env.update(resolved)
    env["ANDROID_SDK_ROOT"] = resolved.get("ANDROID_SDK_ROOT", "")
    env["ANDROID_NDK_ROOT"] = resolved.get("ANDROID_NDK_ROOT", "")
    env["QT_ANDROID_DIR"] = resolved.get("QT_ANDROID_DIR", "")
    env["QT_HOST_DIR"] = resolved.get("QT_HOST_DIR", env.get("QT_HOST_DIR", ""))
    env["OPENCV_ANDROID_SDK"] = resolved.get("OPENCV_ANDROID_SDK", "")
    env["CMAKE_ANDROID_API"] = args.api

    ensure_preset(args.preset)

    # Configure
    if not args.build_only:
        configure_cmd = ["cmake", f"--preset={args.preset}"]
        ret = run(configure_cmd, env=env)
        if ret != 0:
            error("CMake configure failed")
            return ret

    # Build
    build_cmd = ["cmake", f"--build", f"--preset={args.preset}"]
    if args.jobs:
        build_cmd.append(f"-j{args.jobs}")
    ret = run(build_cmd, env=env)
    if ret != 0:
        error("CMake build failed")
        return ret

    # Deployment is now handled automatically by CMake via qt_finalize_target()
    # The build step triggers androiddeployqt internally, so we just need to locate the APK

    if "relwithdebinfo" in args.preset:
        config_dir = "relwithdebinfo"
    elif "debug" in args.preset:
        config_dir = "debug"
    else:
        config_dir = "release"

    build_dir = project_root / "build" / config_dir / "android-arm64"
    android_build_dir = build_dir / "android-build"

    # Check for the generated APK
    apk_candidates = [
        android_build_dir
        / "build"
        / "outputs"
        / "apk"
        / "debug"
        / "android-build-debug.apk",
        android_build_dir
        / "build"
        / "outputs"
        / "apk"
        / "release"
        / "android-build-release-unsigned.apk",
    ]

    found_apk = next((p for p in apk_candidates if p.exists()), None)
    if found_apk:
        success(f"✓ Android APK built: {found_apk}")
    else:
        warn(
            "Build completed but APK not found at expected location.\n"
            f"  Expected locations:\n"
            + "\n".join(f"    - {p}" for p in apk_candidates)
            + f"\n  Check {android_build_dir} for build output."
        )

    success("Android build helper completed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
