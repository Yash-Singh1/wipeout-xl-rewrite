#!/usr/bin/env python3
"""Builds the release's downloads into build/release-assets/, from macOS:

    WipeoutXL-VERSION-macOS-Universal.zip   Wipeout XL.app, one program for Apple Silicon and Intel Macs
    WipeoutXL-VERSION-Windows-x64.exe       64-bit Windows on Intel and AMD
    WipeoutXL-VERSION-Windows-arm64.exe     Windows on Arm

    tools/release.py [--version 0.0.4] [--only mac,windows-x64,windows-arm64] [--smoke-disc GAME.cue]

Each is self-contained: SDL3, libarchive and the compression libraries libarchive reads archives with (zlib, xz,
zstd, lz4) are downloaded and built into the program, never the build machine's (Homebrew's are for this Mac
only). The builds are kept under build/release/, so a second run only rebuilds what changed.

Needs Xcode's command line tools, CMake and Ninja; for x64 Windows Homebrew's mingw-w64; for Windows on Arm
llvm-mingw (https://github.com/mstorsjo/llvm-mingw: --llvm-mingw, the environment's LLVM_MINGW, or its bin/ on
PATH). --smoke-disc boots each of the Mac program's two halves (the Intel one under Rosetta) for 900 frames.
"""
import argparse
import glob
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORK = os.path.join(ROOT, "build", "release")
ASSETS = os.path.join(ROOT, "build", "release-assets")
MAC_MINIMUM = "11.0"  # the first macOS for Apple Silicon

# the compression libraries: (name, archive URL, its SHA-256, the CMake project's folder in it, its options)
LIBRARIES = [
    ("zlib", "https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.xz",
     "d7a0654783a4da529d1bb793b7ad9c3318020af77667bcae35f95d0e42a792f3", "",
     ["-DZLIB_BUILD_SHARED=OFF", "-DZLIB_BUILD_TESTING=OFF"]),
    ("xz", "https://github.com/tukaani-project/xz/releases/download/v5.8.4/xz-5.8.4.tar.xz",
     "4ce24038fd4221e0d13bc1a2de7a4db56e90b92b3bf75321f6c14be73f65de4b", "",
     ["-DBUILD_SHARED_LIBS=OFF", "-DBUILD_TESTING=OFF", "-DXZ_NLS=OFF", "-DXZ_DOC=OFF", "-DXZ_TOOL_XZ=OFF",
      "-DXZ_TOOL_XZDEC=OFF", "-DXZ_TOOL_LZMADEC=OFF", "-DXZ_TOOL_LZMAINFO=OFF", "-DXZ_TOOL_SCRIPTS=OFF"]),
    ("zstd", "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz",
     "eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3", "build/cmake",
     ["-DZSTD_BUILD_SHARED=OFF", "-DZSTD_BUILD_PROGRAMS=OFF", "-DZSTD_BUILD_TESTS=OFF"]),
    ("lz4", "https://github.com/lz4/lz4/releases/download/v1.10.0/lz4-1.10.0.tar.gz",
     "537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b", "build/cmake",
     ["-DBUILD_SHARED_LIBS=OFF", "-DBUILD_STATIC_LIBS=ON", "-DLZ4_BUILD_CLI=OFF"]),
]

# Windows' own libraries a program may load; anything else beside it would be missing on the player's PC
WINDOWS_DLL = re.compile(r"(api-ms-win-.*|kernel32|user32|gdi32|shell32|ole32|oleaut32|advapi32|imm32|winmm|"
                         r"version|setupapi|cfgmgr32|opengl32|dinput8|dxgi|d3d11|d3d12|xinput.*|hid|ws2_32|"
                         r"comdlg32|uuid|msvcrt|ucrtbase|bcrypt|crypt32|shlwapi|dwmapi|uxtheme)\.dll", re.I)


def run(*command, cwd=None, env=None):
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, env=env, check=True)


def output(*command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def targets(llvm_mingw):
    """Each download's CMake options (beyond the build type) for the libraries and the game alike."""
    windows_arm = ["-DCMAKE_TOOLCHAIN_FILE=" + os.path.join(ROOT, "cmake", "mingw-w64-aarch64.cmake")]
    if llvm_mingw:
        windows_arm.append("-DLLVM_MINGW=" + llvm_mingw)
    return {
        "mac": ["-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + MAC_MINIMUM,
                "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local"],
        "windows-x64": ["-DCMAKE_TOOLCHAIN_FILE=" + os.path.join(ROOT, "cmake", "mingw-w64-x86_64.cmake")],
        "windows-arm64": windows_arm,
    }


def source(name, url, sha256):
    """The library's unpacked source (downloaded once, checked against its hash)."""
    archive = os.path.join(WORK, "downloads", os.path.basename(url))
    if not os.path.exists(archive):
        os.makedirs(os.path.dirname(archive), exist_ok=True)
        print("download", url, flush=True)
        with urllib.request.urlopen(url) as response, open(archive + ".part", "wb") as f:
            shutil.copyfileobj(response, f)
        os.replace(archive + ".part", archive)
    with open(archive, "rb") as f:
        if hashlib.sha256(f.read()).hexdigest() != sha256:
            sys.exit(f"release: {archive} is not the expected file (SHA-256); delete it to download it again")
    folder = os.path.join(WORK, "src", re.sub(r"\.tar\.\w+$", "", os.path.basename(url)))
    if not os.path.isdir(folder):
        with tarfile.open(archive) as tar:
            if hasattr(tarfile, "data_filter"):  # (Python 3.12, and later 3.8 to 3.11 releases)
                tar.extractall(os.path.dirname(folder), filter="data")
            else:
                tar.extractall(os.path.dirname(folder))
    return folder


def build_libraries(target, options, jobs):
    """The compression libraries for `target`, static, installed in build/release/TARGET/deps."""
    prefix = os.path.join(WORK, target, "deps")
    for name, url, sha256, subfolder, own in LIBRARIES:
        folder = os.path.join(source(name, url, sha256), subfolder)
        build = os.path.join(WORK, target, "deps-build", name)
        if not os.path.exists(os.path.join(build, "CMakeCache.txt")):
            run("cmake", "-S", folder, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_INSTALL_PREFIX=" + prefix, "-DCMAKE_POLICY_VERSION_MINIMUM=3.5", *options, *own)
        run("cmake", "--build", build, "-j", str(jobs))
        run("cmake", "--install", build)
    return prefix


def build_game(target, options, prefix, jobs):
    build = os.path.join(WORK, target, "game")
    zlib = [path for path in glob.glob(os.path.join(prefix, "lib", "libz*.a"))
            if re.fullmatch(r"libzs?\.a", os.path.basename(path))]  # (libzs.a on Windows)
    if len(zlib) != 1:
        sys.exit(f"release: zlib's static library in {prefix}/lib: {zlib}")
    found = ["-DCMAKE_PREFIX_PATH=" + prefix] if target == "mac" else ["-DCMAKE_FIND_ROOT_PATH=" + prefix]
    env = dict(os.environ, PKG_CONFIG_LIBDIR=os.path.join(prefix, "lib", "pkgconfig"), PKG_CONFIG_PATH="")
    if not os.path.exists(os.path.join(build, "CMakeCache.txt")):
        run("cmake", "-S", ROOT, "-B", build, "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=" + ("RelWithDebInfo" if target == "mac" else "Release"),
            "-DWIPEOUT_BUILD_SDL=ON", "-DWIPEOUT_BUILD_LIBARCHIVE=ON",
            "-DZLIB_LIBRARY=" + zlib[0], "-DZLIB_INCLUDE_DIR=" + os.path.join(prefix, "include"),
            *found, *options, env=env)
    run("cmake", "--build", build, "-j", str(jobs), env=env)
    return build


def check_mac(app):
    program = os.path.join(app, "Contents", "MacOS", "wipeoutxl")
    archs = output("lipo", "-archs", program).split()
    if sorted(archs) != ["arm64", "x86_64"]:
        sys.exit(f"release: {program} is for {archs}, not arm64 and x86_64")
    binaries = [program] + glob.glob(os.path.join(app, "Contents", "Frameworks", "*"))
    for binary in binaries:
        for line in output("otool", "-L", binary).splitlines():
            if not line[:1].isspace():  # "binary (architecture ...):"
                continue
            name = line.strip().split(" (")[0]
            if not name.startswith(("/usr/lib/", "/System/", "@rpath/")):
                sys.exit(f"release: {binary} loads {name}, which a player's Mac does not have")
    run("codesign", "--verify", "--deep", "--strict", app)


def check_windows(exe, machine, objdump):
    headers = output(objdump, "-p", exe)
    if not re.search(machine, headers):
        sys.exit(f"release: {exe} is not for {machine}")
    for dll in re.findall(r"DLL Name: (\S+)", headers):
        if not WINDOWS_DLL.fullmatch(dll):
            sys.exit(f"release: {exe} loads {dll}, which is not part of Windows")


def smoke(program, disc, arch):
    """Boots the game from `disc` with one half of the Mac program for 900 frames, hidden and silent."""
    save = os.path.join(WORK, "smoke-" + arch)
    shutil.rmtree(save, ignore_errors=True)
    os.makedirs(save)
    env = dict(os.environ, WIPEOUT_HIDDEN="1", WIPEOUT_MUTE="1", WIPEOUT_FIXED_STEP="1", WIPEOUT_FRAMES="900",
               WIPEOUT_CD_SPEED="8", WIPEOUT_NO_GAMEPAD="1", WIPEOUT_SAVE_DIR=save)
    done = subprocess.run(["arch", "-" + arch, program, "--disc", disc, "--window", "320x240"], env=env,
                          capture_output=True, text=True)
    shown = re.findall(r"(\d+) pictures shown", done.stdout + done.stderr)
    if done.returncode or not shown or int(shown[-1]) < 800:
        sys.exit(f"release: the {arch} program did not run the game (exit {done.returncode}, "
                 f"{shown[-1] if shown else 'no'} pictures shown):\n{(done.stdout + done.stderr)[-2000:]}")
    print(f"smoke {arch}: {shown[-1]} pictures shown", flush=True)
    shutil.rmtree(save)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", help="the release's version in the file names (default: git describe)")
    parser.add_argument("--only", default="mac,windows-x64,windows-arm64", help="the downloads to build")
    parser.add_argument("--llvm-mingw", default=os.environ.get("LLVM_MINGW"), help="the llvm-mingw toolchain")
    parser.add_argument("--smoke-disc", help="a disc image (.cue) to boot the Mac program with")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count())
    args = parser.parse_args()
    version = args.version or output("git", "-C", ROOT, "describe", "--tags", "--always").strip()
    version = version.removeprefix("v")
    every = targets(args.llvm_mingw and os.path.abspath(os.path.expanduser(args.llvm_mingw)))
    chosen = args.only.split(",")
    for target in chosen:
        if target not in every:
            sys.exit(f"release: no download {target} (only {', '.join(every)})")
    os.makedirs(ASSETS, exist_ok=True)

    made = []
    for target in chosen:
        print(f"\n==== {target} ====", flush=True)
        prefix = build_libraries(target, every[target], args.jobs)
        build = build_game(target, every[target], prefix, args.jobs)
        if target == "mac":
            app = os.path.join(build, "Wipeout XL.app")
            check_mac(app)
            if args.smoke_disc:
                for arch in ("arm64", "x86_64"):
                    smoke(os.path.join(app, "Contents", "MacOS", "wipeoutxl"), args.smoke_disc, arch)
            asset = os.path.join(ASSETS, f"WipeoutXL-{version}-macOS-Universal.zip")
            if os.path.exists(asset):
                os.remove(asset)
            run("zip", "-r", "-y", "-q", asset, "Wipeout XL.app", cwd=build)
        else:
            exe = os.path.join(build, "wipeoutxl.exe")
            with open(os.path.join(build, "CMakeCache.txt")) as f:
                objdump = re.search(r"^CMAKE_OBJDUMP:FILEPATH=(.*)$", f.read(), re.M).group(1)
            if target == "windows-x64":
                check_windows(exe, r"file format (pei|coff)-x86-64", objdump)
                asset = os.path.join(ASSETS, f"WipeoutXL-{version}-Windows-x64.exe")
            else:
                check_windows(exe, r"file format (pei|coff)-(arm64|aarch64)", objdump)
                asset = os.path.join(ASSETS, f"WipeoutXL-{version}-Windows-arm64.exe")
            shutil.copy2(exe, asset)
        made.append(asset)

    print()
    for asset in made:
        print(f"{os.path.getsize(asset):>10}  {os.path.relpath(asset, ROOT)}")


if __name__ == "__main__":
    main()
