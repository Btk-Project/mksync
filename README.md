# mksync

## Introduction ([中文](/README_zh.md))
Mksync can synchronize a mouse and keyboard between multiple computers on Windows and Linux systems. (macOS may be in the future plans)

It also supports clipboard sharing, file transfer and drag-drop files. (support will be added in the future through plugins)

## What is it?
Mksync is a free and open source keyboard and mouse sharing library and app.

The app in mksync mimics the functionality of a KVM switch, which historically would allow you to use a single keyboard and mouse to control multiple computers by physically turning a dial on the box to switch the machine you're controlling at any given moment. Mksync does this in software, allowing you to tell it which machine to control by moving your mouse to the edge of the screen, or by using a keypress to switch focus to a different system.

There is no GUI for the base functionality of this app, but we take a plug-in approach to support the GUI.

The mks library is a development library for mksync, you can use mksync's protocols to develop similar apps after including it, or develop plugins for mksync for more functionality.

Mksync was developed based on the inspiration of synergy, barrier and input-leap. You could also say that mksync is a refactored version of them.

## What is different?

Due to the poor connection experience found when using synergy and barrier. I need to set up in advance on the server the username of the user who may connect and set up the screen location of this user. Only after doing these preparations can I connect using a client with the same name. It's very inconvenient, and when some computers have multiple screens, I can't move them around as if they were expandable!

And they don't support transferring files across systems using the clipboard, nor dragging and dropping files.

Due to the presence of the above problems, we decided to refactor based on synergy and barrier. And decide to use a pluginized solution to provide more content.

## Project goals

#### In version v0.1.0
Synchronize a mouse and keyboard between multiple computers on Windows and Linux systems. (It's basic)

#### In version v0.2.0
Implementing the Plugin Manager. Provides mks library support for clipboard sharing, file transfer, file dragging and dropping.

#### In version v0.3.0
Implementing the clipboard sharing.

#### In version v1.0.0
Implementing the file transfer. (At this point, mksync will be launching the official released version)

#### In version after v1.0.0
We will implement file dragging and dropping and support for more systems (e.g. mocOS) on demand only after the official version is available. Of course, the GUI is also planned to be implemented after the official released version.

## How to build?
mksync requires Xmake 3.0.9 or newer. C++23 is both the default and the minimum supported
language level. C++26 is an optional GCC 16.1 compatibility lane; C++20 and older are unsupported.
Ubuntu 24.04 with GCC 14 and Qt 6 is the minimum supported Linux build and package baseline.

Install the complete Ubuntu 24.04 build environment with:

```bash
sudo apt install gcc-14 g++-14 libei-dev libfmt-dev libgmock-dev libgtest-dev \
  libportal-dev libspdlog-dev libwayland-bin libwayland-dev \
  libxcb1-dev libxcb-keysyms1-dev libxcb-randr0-dev libxcb-xinput-dev \
  libxcb-xtest0-dev libxkbcommon-dev pkg-config qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
  qml6-module-qtquick-layouts wayland-protocols x11proto-dev
export CC=gcc-14 CXX=g++-14
```

### configure

A normal CLI build does not require Qt:

```bash
xmake f -c -m debug --stdcxx=23 --enable_gui=n
xmake build mksync
```

Each backend has an independent build option. Disabling one skips both dependency discovery and
its source files:

```bash
xmake f -c --stdcxx=23 \
  --enable_backend_x11=y \
  --enable_backend_wayland_wlr=n \
  --enable_backend_wayland_portal=n
```

Windows uses `--enable_backend_win32`. Linux XCB, Wayland, libei, and portal libraries are
platform-bound system dependencies and are therefore resolved only through system `pkg-config`.

### compile

Run the complete test suite with:

```bash
xmake test
```

### Qt 6 GUI

The Qt/QML frontend lives in the isolated [`ui/`](ui/README.md) directory and is not built by
default. Screen layout and trusted devices remain in the `AppConfig` JSON document. Startup mode,
endpoint, JSON path, and log level use the same `CliCommand` model and TOML parser as the CLI, so
files produced by `--export-toml` can be opened in the GUI and exported for `--import-toml`.

```bash
xmake f -m debug --enable_gui=y
xmake build mksync-gui
xmake run mksync-gui
```

On Linux, Xmake checks for a complete system Qt 6 Quick SDK first and only declares its `qt6quick`
package when the system SDK is missing. Windows CI and release builds use the Xmake-provided SDK.

### Packages and releases

`lua/pack.lua` gives Xpack both the CLI and the enabled GUI target:

```bash
xmake f -c -m release --stdcxx=23 --enable_gui=y --enable_tests=n
xmake pack -f targz -o dist       # Linux archive
xmake pack -f deb -o dist         # Debian/Ubuntu package
```

On Windows, use `xmake pack -f nsis,zip -o dist`; Xmake's Qt install hook invokes
`windeployqt`. Pushing a `vX.Y.Z` tag makes GitHub Actions build Linux and Windows packages and
attach them to a GitHub Release. See [`docs/releasing.md`](docs/releasing.md) for the exact flow.
The Linux package currently targets Ubuntu 24.04; a Flatpak package is planned as the future
distribution-independent runtime.

### Input backends

Backends self-register and provide capability checks. Automatic selection picks the first backend
that satisfies the server or client requirements; CLI and GUI can also select one explicitly:

```bash
mksync backend --list --checked
mksync server 0.0.0.0:1234 --backend x11
mksync server 0.0.0.0:1234 --backend wayland-portal
mksync client 192.0.2.10:1234 --backend wayland-portal
```

`wayland-portal` implements capture and injection with the XDG InputCapture/RemoteDesktop portals
and libei. `wayland-wlr` is limited to Wayland/wlroots protocol capabilities. Neither backend mixes
in X11 APIs or intertwines the two implementations. See
[`docs/wayland_backend.md`](docs/wayland_backend.md) for the exact capability boundaries.
