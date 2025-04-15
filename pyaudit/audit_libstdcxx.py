#!/usr/bin/env python3
""""
Copyright (c) 2025 Altera

Permission is hereby granted, free of charge, to any person obtaining a copy of this software
and associated documentation files (the "Software"),to deal in the Software without
restriction, including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
"""

# This is both:
#  - a set of useful shared library methods
# and
#  - an executable python script which prints the path to the system libstdc++.so.6
#

import ctypes
import platform
import sys
from ctypes import (CDLL, POINTER, Structure, byref, c_char_p, c_int, c_void_p,
                    cast)
from ctypes.util import find_library


def get_cdll_library_path(cdll_handle):
    class LINKMAP(Structure):
        _fields_ = [("l_addr", c_void_p), ("l_name", c_char_p)]

    libdl = CDLL(find_library("dl"))

    dlinfo = libdl.dlinfo
    dlinfo.argtypes = c_void_p, c_int, c_void_p
    dlinfo.restype = c_int

    # gets typecasted later
    lmptr = c_void_p()

    # 2 equals RTLD_DI_LINKMAP, pass pointer by reference
    dlinfo(cdll_handle._handle, 2, byref(lmptr))

    # typecast to a linkmap pointer and retrieve the name.
    abspath = cast(lmptr, POINTER(LINKMAP)).contents.l_name.decode("utf-8")
    return abspath


if __name__ == "__main__":
    if platform.system() != "Linux":
        raise RuntimeError(
            "This executable is designed and tested to run on Linux systems only"
        )
    try:
        # linkmap structure, we only need the second entry
        abspath = get_cdll_library_path(cdll_handle=ctypes.CDLL(find_library("stdc++")))

        print(abspath)
    except OSError as e:
        raise RuntimeError(f"Failed to load shared library libstdc++.so.6: {e}")
    sys.exit(0)


# Below here is the import flow from sitecustomize.py

import os  # noqa: E402
import subprocess  # noqa: E402
import re  # noqa: E402

from elftools.elf.elffile import ELFFile  # noqa: E402


def parse_version(v):
    match = re.fullmatch(r"(\d+)(?:\.(\d+))?(?:\.(\d+))?", v)
    if not match:
        print(f"audit_libstdcxx WARNING! Invalid ELF GLIBCXX version {v}\n"
              "Runtime link errors may occur\n"
              "Warning thrown from {__file__} at {__line__}", file=sys.stderr
        )
        return (0,0,0)
    major = int(match.group(1))
    minor = int(match.group(2) or 0)
    patch = int(match.group(3) or 0)
    return (major, minor, patch)


def get_glibcxx_versions_from_gnu_version_d(elf_path):
    with open(elf_path, "rb") as f:
        elffile = ELFFile(f)

        # Locate the .gnu.version_d section
        version_d_section = elffile.get_section_by_name(".gnu.version_d")
        if not version_d_section:
            raise ValueError("No .gnu.version_d section found in the ELF file")

        max_ver = (0,0,0)
        # Iterate through version definitions
        for version, version_auxiliaries in version_d_section.iter_versions():
            # Each version entry contains the version string
            for aux in version_auxiliaries:
                # Check if the version name contains 'GLIBCXX'
                version_name = aux.name
                if "GLIBCXX_" in version_name:
                    ver = parse_version(version_name[len("GLIBCXX_"):])
                    if ver > max_ver:
                        max_ver = ver

    return max_ver

# This is the global variable used to pass the libstdcxx path from sitecustomize.py to the audit hook
global_libstdcxx_path = ""

def check_and_load_compatible_libstdcxx_version():
    if platform.system() != "Linux":
        return

    # Check if libstdc++.so.6 is already loaded

    already_loaded = False
    system_libstdcxx = ""
    try:
        handle = ctypes.CDLL(find_library("stdc++"), mode=os.RTLD_NOLOAD)
        already_loaded = True
        system_libstdcxx = get_cdll_library_path(cdll_handle=handle)
    except OSError:
        # Run ourselves in a subprocess to load libstdc++ and print the path
        # We do this in a subprocess because we actually must load the library to get the _handle
        # we need the _handle to get the true path to the shared library
        # A subprocess means we can load the library without polluting this process' currently loaded libraries
        # and get the path from a _handle
        # NOTE: We cannot use RTLD_NOLOAD because that does not create a handle
        # This script, when used as an executable, only requires builtin imports
        # Strip PYTHONPATH as the env may have the orignal path to this directory
        # We do not want to reintepret sitecustomize.py in the subprocess
        env_no_pythonpath = os.environ.copy()
        env_no_pythonpath.pop("PYTHONPATH", None)
        result = subprocess.run([sys.executable, __file__], env=env_no_pythonpath, capture_output=True, text=True, check=True)
        system_libstdcxx = result.stdout.splitlines()[0].strip() if result.stdout else None

    # Load the shared library
    global global_libstdcxx_path
    libstdcxx_path = global_libstdcxx_path

    if libstdcxx_path is not None and system_libstdcxx is not None and os.path.isfile(libstdcxx_path) and os.path.isfile(system_libstdcxx):
        try:
          system_version = get_glibcxx_versions_from_gnu_version_d(system_libstdcxx)
          shipped_version = get_glibcxx_versions_from_gnu_version_d(libstdcxx_path)
        except ValueError as e:
          print(f"audit_libstdcxx WARNING! Failed to retrieve GLIBCXX version from {system_libstdcxx} or {libstdcxx_path}\n"
                "With OSError exception {e}\n"
                "Runtime link errors may occur\n"
                "Warning thrown from {__file__} at {__line__}", file=sys.stderr
          )
          return


        if system_version < shipped_version:
            if already_loaded:
                print(f"audit_libstdcxx WARNING! Something (perhaps another sitecustomize.py) loaded an incompatible libstdc++.so.6 before this audit_libstdcxx.\n"
                      "This may cause compatibility issues and runtime linking errors.\n"
                      "Please examine your system to find how an upstream python module could have loaded libstdc++.so.6\n"
                      "Warning thrown from {__file__} at {__line__}", file=sys.stderr
                )
            else:
                try:
                    ctypes.CDLL(libstdcxx_path)
                except OSError as e:
                    print(f"audit_libstdcxx WARNING! Failed to load shared library at {libstdcxx_path}\n"
                          "With OSError exception {e}\n"
                          "Runtime link errors may occur\n"
                          "Warning thrown from {__file__} at {__line__}", file=sys.stderr
                    )
                    return
    else:
        if libstdcxx_path is None or not os.path.isfile(libstdcxx_path):
            print(f"audit_libstdcxx WARNING! The path to shipped libstdc++.so.6 is invalid: {libstdcxx_path}\n"
                  "Warning thrown from {__file__} at {__line__}", file=sys.stderr)

        if system_libstdcxx is None or not os.path.isfile(system_libstdcxx):
            print(f"audit_libstdcxx WARNING! The path to system libstdc++.so.6 is invalid: {system_libstdcxx}\n"
                  "Warning thrown from {__file__} at {__line__}", file=sys.stderr)

# This global variable is used to disable the audit hook after we have loaded the compatible libstdc++.so.6
global_disable_audit_hook = False

def check_shared_lib_dependencies(lib_path):
    if (os.path.basename(lib_path) == "libstdc++.so.6"):
        # After this, we will not want our hook to run
        global global_disable_audit_hook
        global_disable_audit_hook = True
        check_and_load_compatible_libstdcxx_version()
        return

    # Only attempt to check if the file exists.
    if not os.path.exists(lib_path):
        return

    try:
        # Call ldd to list dynamic dependencies.
        # Note: This assumes a Unix-like system with ldd installed.
        output = subprocess.check_output(["ldd", lib_path], text=True)
    except Exception as e:
        print(f"audit_libstdcxx WARNING!: Error running ldd on {lib_path}\n"
              "With Exception {e}\n"
              "Warning thrown from {__file__} at {__line__}", file=sys.stderr)
        return

    if "libstdc++.so.6" in output:
        check_shared_lib_dependencies("libstdc++.so.6")

def audit_hook(event, args):
    """
    Audit hook to monitor module imports and shared library loads.
    """
    if global_disable_audit_hook:
        return

    filename = None
    if event == "import" and len(args) > 1:
        filename = args[1]  # Resolved file path, if available
    elif event == "sys.dlload" and len(args) > 0:
        filename = args[0]  # Shared library being loaded
    else:
        return

    if filename and filename.endswith('.so'):  # Check for shared library
        check_shared_lib_dependencies(filename)

def set_hooks_and_audit(libstdcxx_path):
    global global_libstdcxx_path
    if (os.path.isfile(libstdcxx_path)):
        global_libstdcxx_path = libstdcxx_path
    else:
        global_libstdcxx_path = os.path.join(libstdcxx_path, "libstdc++.so.6")

    if platform.system() == "Linux":
        # Register the audit hook with the interpreter.
        sys.addaudithook(audit_hook)