# Debian binary package

This directory deliberately builds a binary `.deb` without Xmake's Xpack Debian backend. The
script consumes executables that have already been built, calculates their runtime dependencies
with `dpkg-shlibdeps`, stages private libraries such as `libilias`, and calls `dpkg-deb` directly.
It never invokes `debuild` or `dpkg-buildpackage`, so Xmake and `build-essential` are not package
build dependencies.

On Ubuntu, install the small packaging tool set:

```bash
sudo apt install dpkg-dev patchelf
```

The supported entry point is an Xmake phony target. It builds the targets enabled by the current
configuration, passes their exact output paths and the selected `ilias` package directory to the
external script, and writes to `dist/` by default:

```bash
xmake f -c -m release --stdcxx=23 --enable_gui=y --enable_tests=n
xmake build package_deb
```

Configure another output directory in the same configuration command, for example
`xmake f -c -m release --enable_gui=y --deb_outputdir=/path/to/dist`. With `--enable_gui=n`, the
same target creates a CLI-only package. `build.sh` remains directly callable as a
lower-level/debugging interface, but callers then need to supply relocated binaries and private
library directories themselves.
