# Debian binary package

This directory deliberately builds a binary `.deb` without Xmake's Xpack Debian backend. The
script consumes executables that have already been built, calculates their runtime dependencies
with `dpkg-shlibdeps`, stages explicitly selected private libraries, and calls `dpkg-deb` directly.
It never invokes `debuild` or `dpkg-buildpackage`, so Xmake and `build-essential` are not package
build dependencies.

The default package follows distribution packaging conventions: Qt, required QML imports, QPA
plugins, XCB/Wayland, libei/libportal, compiler runtimes, glibc, and graphics libraries remain
target-system packages. `dpkg-shlibdeps` records minimum ELF ABI versions, while the script adds
the runtime-loaded QML and QPA packages explicitly. Only project-private libraries without a
stable distribution ABI are carried by default.

Pass `--bundle-qt` to the packaging task to create the larger self-contained Qt variant. That
variant carries the matching Qt libraries, selected QML imports, and XCB/Wayland Qt plugins, while
still leaving their ABI-compatible system dependencies to the package manager. The flag applies to
that invocation only and does not alter the cached build configuration.

On Ubuntu, install the small packaging tool set:

```bash
sudo apt install dpkg-dev patchelf
```

The supported entry point is an Xmake task. It builds the targets enabled by the current
configuration, passes their exact output paths and the selected `ilias` package directory to the
external script, and writes to `dist/` by default:

```bash
xmake f -c -m release --stdcxx=23 --enable_gui=y --enable_tests=n
xmake package_deb
xmake package_deb --bundle-qt
xmake package_deb --output-dir=/path/to/dist
```

The task streams packaging progress and prints the absolute output path. The legacy
`xmake build package_deb` and `xmake run package_deb` forms only print a warning pointing to the
task. With `--enable_gui=n`, the same task creates a CLI-only package. `build.sh` remains directly
callable as a lower-level/debugging interface, but callers then need to supply relocated binaries
and private library directories themselves. A direct self-contained GUI invocation must pass
`--bundle-qt`, `--qt-library-dir`, `--qt-plugin-dir`, and `--qt-qml-dir` from the same Qt SDK.
