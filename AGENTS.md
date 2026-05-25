# Repository Guidelines

## Project Structure & Module Organization

`mksync` is an Xmake-based C++ project for keyboard and mouse synchronization. Core source lives in `src/`: `main.cpp` is the current binary entry point, `src/config/` contains generated and platform configuration headers, and `src/refl/` holds reflection/error helpers. Lua build extensions are in `lua/`, including check helpers and packaging/target visibility scripts. `tests/` exists for test code but is currently empty. Build outputs belong in `build/` and `bin/`; do not commit generated files from those directories.

## Build, Test, and Development Commands

- `xmake f -p windows -a x64 -m releasedbg -k shared --runtimes=MD --3rd_kind=shared -cv`: configure a Windows/MSVC shared build.
- `xmake f -a x86_64 -m releasedbg -k shared --runtimes=stdc++_shared --3rd_kind=shared -cv`: configure a Linux/GCC shared build.
- `xmake`: build the configured target.
- `xmake -r`: rebuild from scratch.
- `xmake -v` or `xmake -rv`: build with verbose output for troubleshooting.

The project requires Xmake `3.0.0` or newer and currently targets C++26 by default, with C++23/17/11 selectable through `stdcxx`.

## Coding Style & Naming Conventions

Use UTF-8 source files, 4-space indentation, no tabs, and a 100-column limit as defined in `.clang-format_18` and `.clang-format_19`. The clang-tidy configuration enables bugprone, cert, analyzer, modernize, performance, portability, readability, and related checks. Follow the naming rules in `.clang-tidy`: types and enums use `CamelCase`, functions and local variables use `camelBack`, global variables use `gCamelCase`, and static variables use `kCamelCase`.

## Testing Guidelines

There is no active test target yet. Add new tests under `tests/` and wire them through Xmake when introducing behavior that can regress. Prefer names that describe the component and behavior, such as `refl_enum_error_test.cpp`. Once a test target exists, document and use the corresponding `xmake test` or target-specific command.

## Commit & Pull Request Guidelines

Git history could not be inspected in this workspace because Git marked the repository as unsafe due to ownership mismatch. Until a project-specific convention is established, use short imperative commit subjects, for example `Add reflection enum test`, with an explanatory body for behavioral changes. Pull requests should include a concise summary, build/test commands run, linked issues when applicable, and screenshots or terminal output for user-visible changes.

## Security & Configuration Tips

Do not commit local build state from `.xmake/`, `.vscode/`, `build/`, or `bin/`. Keep dependency changes centralized in `xmake.lua`, and document any new system package requirements near the relevant platform block.
