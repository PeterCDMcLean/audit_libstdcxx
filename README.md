# Audit Loader for libstdc++

When building C++ applications with broad support for different Linux distributions, applications that dynamically link to libstdc++ can ship with the
compiling-system's libstdc++ to ensure that target users systems can execute the application regardless of the users' libstdc++ version.

However, if the installation ends up on a system whose system libstdc++ is greater than the shipped libstdc++, there could be
conflicts between shared libraries that were built against the higher system libstdc++ and the application which was compiled against the lower version
shipped libstdc++. One example, is exceptions, which can be passed across the boundaries of the two. To avoid these issues, applications should always use the
higher version libstdc++ that is available on a system. ABI backwards compatibilty is nearly always guaranteed for libstdc++ so the highest version should be
safest.

This project is an audit library which will seamlessly choose the highest version libstdc++ between the shipped version and the first system version.
An application will specify this audit library in the DT_AUDIT ELF property. This will cause it to be loaded first and informed of the events for loading and
choosing specific libraries to load. It will compare the libstdc++ shared libraries and choose the higher glibcxx version.

Take note, however, that up until glibc version 2.32, DT_AUDIT ELF property is not used by ld.so. This is why the default RUNPATH includes the shipped libstdc++
On rare user systems that have a glibc version of < 2.32 and a system libstdc++ that is higher than the shipped application, there is the possibility of runtime
errors due to mismatched ABI of exceptions or other library crossing objects. The best way to avoid this situation is to ship very recent version of libstdc++.
User systems with glibc version < 2.32 are unlikely to have the most recent libstdc++ version.

# How does it work?

The goal of this project was to create a system that could dynamically choose and libstdc++ library to load dependending on the version of the system.
In addition, it is highly preferable to avoid the need for a 'wrapper' script or wrapper binary for seamless behavior.

The Linux loader (ld.so) has several stages of operation and interaction when loading an exectuable, it's dynamic dependencies and initializing and starting
execution.

The loader executes several loading initialization steps before main() is invoked:

(There are more details and other copying and mapping that happen, but the below focuses solely on the points where we can inject behavior before loading)

For the executable and each dependency:
1. load LD_PRELOAD
2. load dynamic dependencies
 - Recurse steps 1 and 2 for each dependency
4. PRE_INIT of dependencies
5. INIT of dependencies
6. PRE_INIT of executable
7. INIT of executable

After all dependencies have been loaded and init, run main.

Both the pre_init and init happen after dynamic dependencies are loaded. Which means that the libstdc++ of the system, RPATH or RUNPATH will already be loaded
by the time any library code or exectable code in any pre_init or init section is reached.

There is, however, a special feature of the loader we can use to inject execution before the loading. It is call an audit library.

The loader audit library implements a set of hooks that the ld.so calls between steps. By using the DT_AUDIT field of the exectable ELF file, our audit library
will seamlessly execute code to intercept and redirect which libstdc++ is loaded.

The steps of the loader with an audit library are as follows:


0. load the DT_AUDIT library
1. Call `la_version` of the audit library

For the executable and each dependency:

2. Call `la_objsearch` of the audit library for LD_PRELOAD
3. load LD_PRELOAD
4. Call `la_objsearch` of the audit library for dynamic dependencies
5. load dynamic dependencies
  - Recurse steps 0-5 for each dependency
7. PRE_INIT of dependencies
8. INIT of dependencies
9. PRE_INIT of executable
10. INIT of executable

Now the audit library can change the linking behavior of the exectuable dynamically for libstdc++

In the `la_version` function, the audit library performs introspection of the ORIGIN, RUNPATH and RPATH properties of the executable to find the 'shipped'
libstdc++ and retrieve its path and version.

In the `la_objsearch` function, the audit library rejects RUNPATH searches (as we did this in `la_version`) and waits to find the system libstdc++. When the
system libstdc++ is discovered, the versions are compared and the higher version libstdc++ is chosen.

There are several 'gotchas' of the audit library:

- It is only Linux compatible
- It can use only C
- It cannot use `malloc`

# Using the audit library

Please refer to the example for reference.

First, set the `AuditLibstdcxx_LIBSTDCXX_SO_PATHS` cache variable either through the one of the following methods:
  - Command Line:
    -DAuditLibstdcxx_LIBSTDCXX_SO_PATHS="<Path1> <Path2>"
  - preset JSON:
    "AuditLibstdcxx_LIBSTDCXX_SO_PATHS":<Path1> <Path2>",
  - CMakeLists.txt (BEFORE `find_package`):
  ```
    set(AuditLibstdcxx_LIBSTDCXX_SO_PATHS "<Path1> <Path2>"
        CACHE STRING "Set candidate paths for libstdc++.so.6 smart resolution")
  ```

The audit library CMake target must be searched for and found by `find_package(AuditLibstdcxx REQUIRED)`

During find_package, the libstdcxx_so target will wrap the libstdc++.so.6 with the highest GLIBCXX ELF symbol version.

The user must accomplish three objectives:
  1. Link exectuables with the `AuditLibstdcxx::libstdcxx_exe` CMake Target.
      - This will transitively link with the link_audit_libstdcxx target which will set the -audit flag to the linker
  2. Set the `AUDIT_LIBRARIES` custom property of the exectuable to the audit library's relative path
      - This sets the DT_AUDIT ELF property through a CMake custom property
      - This path must be accessible in both build environment AND install environment for full functionality. See step 4.
  3. Install the libstdcxx that the was chosen by AuditLibstdcxx
      - By default in the example, the `libstdcxx_so` target is installed to `${CMAKE_INSTALL_PREFIX}/lib`.
  4. Install the audit library for both built and installed environments
      - Copy the audit library to a known path relative to the application's build and installed path
      - NOTE: If a path in `AUDIT_LIBRARIES` does not resolve to an audit library, systems with glibc 2.32+ show a runtime error (but still proceed)

In order to execute an application from the build folder, the build binary and audit library relative paths and AUDIT_LIBRARIES property must be the
same as in an installed target. There are post build steps which copy the binaries to mimic an installed environment.

> [!IMPORTANT]
> CMake treats install as a separate step, it is not a first class citizen like the build step. You cannot know the install layout during build unless
> it is all mapped the same by the user (YOU!)

There are several workarounds for unfortunate CMake bugs:
  - `target_link_options` does not play nicely with `$ORIGIN`. The work around is to use `target_link_libraries` instead.
  - CMake has a bug when escaping `$ORIGIN` for Ninja generator. The example has a workaround

# libstdc++

By default, the example uses the first system libstdc++ of the compiling system to ship. However, libstdc++ depends on glibc.
To ship executables with the widest version compatility, you should consider compiling libstdc++ yourself on a system that has a lower glibc version.
Doing so lowers the glibc version that libstdc++ will depend on. An alterative is to find a trusted libstdc++ that is already built with a low glibc.
In our case, the Altera Quartus software suite ships with a libstdc++ that depends only on glibc version <= 2.17. The `example_libstdcxx` target shows how
to link and ship a non-system default libstdc++

# Python

If your project provides a C++ shared library binding to python or a C++ shared library that is loaded by a python module, there are some additional challenges
to be sure that the correct libstdc++ is loaded. When python, or abstractions such as Jypter notebooks, are the entry point, there is a different strategy
to intercept the loading process and load the correct libstdc++.

Python has its own concept of 'auditing' which is where different events can be hooked. However, it does not provide an easy execution interception.
We use the `sitecustomize.py` flow of python, which is intended for setting up site-packages and adjusting sys.path. This lets us insert execution early into
python before any user module has been begun execution. The `sitecustomize.py` then sets the hooks. The hooks check for shared library loads through module
import or through CDLL load.

When the audit hooks detect a shared library is loaded, it checks if any of that library's dependencies include libstdc++.
If libstdc++ is among the dependencies, the audit library checks the version of the system libstdc++ versus the specified 'shipped' location

This is a step-by-step explanation of the python audit hook and execution sequence:
the execution process is as follows:

  1. sitecustomize.py is invoked by Python as part of spin-up
  2. sitecustomize.py imports audit_libstdcxx and sets the hooks for module import and CDLL load.
  3. sitecustomize.py releases control back to Python
  4. Normal Python invocation of user code. Let's assume it is running Jupyter notebook.
  5. Jupyter will import ZeroMQ, which is a C++ shared library
  6. The import hook triggers audit_libstdcxx hook.
  7. The audit_libstdcxx hook detects a .so is being imported.
  8. It looks at the dependencies of that .so with `ldd`
  9. If libstdc++.so.6 is in the dependencies, it will start the libstdc++ checking process as follows:
  10. it will check the GLIBCXX version of the shipped libstdc++.so.6 by parsing the file with ELF tools
  11. It will find the path of the _system_ libstdc++.so.6 file by __invoking audit_libstdcxx.py as a subprocess executable__ (This prevents actually / accidentally loading the system libstdc++.so.6 into the main process)
  12. Once the path of the system libstdc++.so.6 is known, check its version with ELF tools.
  13. If the system libstdc++ version is less than the shipped, CDLL load the shipped libstdc++.so.6 (if the system libstdc++ version is equal or higher, ld.so loader will automatically load the system libstdc++)
  14. Disable the hook and proceed as normal.


As a user of this library, you will need to customize the `sitecustomize.py.in` template with the libstdc++.so.6 installed path.
The location of the installed `sitecustomize.py` must be prepended to the `PYTHONPATH` of the end user system.

# Testing

This audit library has only been manually tested on 64bit Linux systems
