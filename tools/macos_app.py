#!/usr/bin/env python3
"""Puts Wipeout XL.app together (CMakeLists.txt, macOS): the program, its Info.plist and icon, and every library
it uses from outside the system (Homebrew's SDL3, libarchive and theirs) in Contents/Frameworks, the program and
the libraries pointed at those copies and signed for this Mac (ad hoc).

    tools/macos_app.py PROGRAM APP INFO_PLIST ICNS
"""
import os
import shutil
import subprocess
import sys

SYSTEM = ("/usr/lib/", "/System/")


def run(*command):
    """Quiet unless it fails (install_name_tool warns of the signature each change undoes: signed again last)."""
    done = subprocess.run(command, capture_output=True, text=True)
    if done.returncode:
        sys.exit(f"macos_app: {' '.join(command)}:\n{done.stdout}{done.stderr}")


def sign(path):
    run("codesign", "--force", "--sign", "-", path)


def libraries(binary):
    """The libraries `binary` loads (its own name left out for a library), as written in it."""
    lines = subprocess.run(["otool", "-L", binary], check=True, capture_output=True, text=True).stdout.splitlines()[1:]
    names = [line.strip().split(" (")[0] for line in lines]
    own = subprocess.run(["otool", "-D", binary], check=True, capture_output=True, text=True).stdout.splitlines()[1:]
    return [name for name in names if name not in own]


def rpaths(binary):
    lines = subprocess.run(["otool", "-l", binary], check=True, capture_output=True, text=True).stdout.splitlines()
    found = []
    for i, line in enumerate(lines):
        if line.strip() == "cmd LC_RPATH":
            found.append(lines[i + 2].strip().split(" ")[1])
    return found


def resolve(name, binary):
    """A library's file: @rpath, @loader_path and @executable_path as the loader would find it from `binary`."""
    here = os.path.dirname(binary)
    if name.startswith("@rpath/"):
        for path in rpaths(binary):
            path = path.replace("@loader_path", here).replace("@executable_path", here)
            candidate = os.path.join(path, name[len("@rpath/"):])
            if os.path.exists(candidate):
                return os.path.realpath(candidate)
        sys.exit(f"macos_app: {name} (from {binary}) not found")
    if name.startswith("@loader_path/") or name.startswith("@executable_path/"):
        return os.path.realpath(os.path.join(here, name.split("/", 1)[1]))
    return os.path.realpath(name)


def main():
    program, app, plist, icns = sys.argv[1:5]
    contents = os.path.join(app, "Contents")
    frameworks = os.path.join(contents, "Frameworks")
    shutil.rmtree(app, ignore_errors=True)
    for folder in ("MacOS", "Resources", "Frameworks"):
        os.makedirs(os.path.join(contents, folder))
    main_binary = os.path.join(contents, "MacOS", "wipeoutxl")
    shutil.copy2(program, main_binary)
    shutil.copy2(icns, os.path.join(contents, "Resources", "wipeoutxl.icns"))
    with open(os.path.join(contents, "PkgInfo"), "w") as f:
        f.write("APPL????")

    # each binary in the app, the libraries it loads, followed into theirs; copies named by their file names
    copied = {}  # source file -> its name in Frameworks
    pending = [(main_binary, program)]
    while pending:
        binary, source = pending.pop()
        os.chmod(binary, 0o755)
        for name in libraries(binary):
            if name.startswith(SYSTEM):
                continue
            file = resolve(name, source)
            if file not in copied:
                copied[file] = os.path.basename(name)
                target = os.path.join(frameworks, copied[file])
                shutil.copy2(file, target)
                os.chmod(target, 0o755)
                run("install_name_tool", "-id", "@rpath/" + copied[file], target)
                pending.append((target, file))
            run("install_name_tool", "-change", name, "@rpath/" + copied[file], binary)
    for path in rpaths(main_binary):
        run("install_name_tool", "-delete_rpath", path, main_binary)
    run("install_name_tool", "-add_rpath", "@executable_path/../Frameworks", main_binary)

    # the plist last: the build's mark that the app is complete
    shutil.copy2(plist, os.path.join(contents, "Info.plist"))
    for name in sorted(os.listdir(frameworks)):
        sign(os.path.join(frameworks, name))
    sign(app)


if __name__ == "__main__":
    main()
