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
mksync is currently only a beta version, there is no release installer, you need to compile and install it yourself from the source code.

mksync is built using xmake, for a related xmake tutorial check out [xmake-io](https://xmake.io/).

### configure

Before building, you need to configure the project.

#### For windows with msvc
```
xmake f -p windows -a x64 -m releasedbg -k shared --runtimes=MD --third_party_kind=shared -cv
```
#### For windows with clang-cl
```
xmake f -p windows -a x64 -m releasedbg -k shared --runtimes=MD --toolchain=clang-cl --third_party_kind=shared -cv
```
#### For windows with mingw
```
xmake f -p mingw -a x86_64 -m releasedbg -k shared --runtimes=stdc++_shared --third_party_kind=shared -cv
```
#### For linux with gcc
```
xmake f -a x86_64 -m release -k shared --runtimes=stdc++_shared --third_party_kind=shred -cv
```
#### For linux with gcc-13
```
xmake f -a x86_64 -m release -k shared --runtimes=stdc++_shared --toolchain=gcc-13 --third_party_kind=shred -cv
```
#### For linux with llvm
```
xmake f -a x86_64 -m release -k shared --runtimes=stdc++_shared --toolchain=llvm --third_party_kind=shred -cv
```

### compile

After configure, it's time to compile this project with this command:
```
xmake
```
Or you can use this command to recompile it all:
```
xmake -r
```
Or you can use this command to display compilation details:
```
xmake -v
```
And their combinations:
```
xmake -rv
```