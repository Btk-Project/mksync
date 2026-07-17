#!/usr/bin/env bash

set -euo pipefail
export LC_ALL=C

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "$script_dir/../.." && pwd)"

usage() {
    printf '%s\n' \
        "Usage: packaging/debian/build.sh [options]" \
        "" \
        "Build a Debian binary package from already-built mksync executables." \
        "The script never invokes Xmake, debuild, or dpkg-buildpackage." \
        "" \
        "Options:" \
        "  --version VERSION             Debian package version (defaults to xmake.lua)" \
        "  --architecture ARCH           Debian architecture (defaults to host architecture)" \
        "  --cli PATH                    Path to the mksync executable" \
        "  --gui PATH                    Path to the mksync-gui executable" \
        "  --without-gui                 Do not include mksync-gui" \
        "  --private-library-dir PATH    Recursively search PATH for private shared libraries" \
        "  --bundle-library-dir PATH     Search PATH for portable runtime dependencies" \
        "  --bundle-qt                   Carry Qt/QML/plugins instead of system dependencies" \
        "  --qt-library-dir PATH         Qt shared-library directory to bundle" \
        "  --qt-plugin-dir PATH          Qt plugin root to bundle selectively" \
        "  --qt-qml-dir PATH             Qt QML import root to bundle selectively" \
        "  --output-dir PATH             Output directory (defaults to dist)" \
        "  -h, --help                    Show this help"
}

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Required command is missing: %s\n' "$1" >&2
        exit 1
    fi
}

project_version() {
    sed -n 's/^set_version("\([^"]*\)".*/\1/p' "$project_dir/xmake.lua" | head -n 1
}

architecture="$(dpkg --print-architecture)"
version="$(project_version)"
output_dir="$project_dir/dist"
cli_binary=""
gui_binary=""
include_gui=auto
private_library_dirs=()
bundle_library_dirs=()
bundle_qt=no
qt_library_dir=""
qt_plugin_dir=""
qt_qml_dir=""

while (($# > 0)); do
    case "$1" in
        --version)
            version="${2:?--version requires a value}"
            shift 2
            ;;
        --architecture)
            architecture="${2:?--architecture requires a value}"
            shift 2
            ;;
        --cli)
            cli_binary="${2:?--cli requires a path}"
            shift 2
            ;;
        --gui)
            gui_binary="${2:?--gui requires a path}"
            include_gui=yes
            shift 2
            ;;
        --without-gui)
            include_gui=no
            gui_binary=""
            shift
            ;;
        --private-library-dir)
            private_library_dirs+=("${2:?--private-library-dir requires a path}")
            shift 2
            ;;
        --bundle-library-dir)
            bundle_library_dirs+=("${2:?--bundle-library-dir requires a path}")
            shift 2
            ;;
        --bundle-qt)
            bundle_qt=yes
            shift
            ;;
        --qt-library-dir)
            qt_library_dir="${2:?--qt-library-dir requires a path}"
            shift 2
            ;;
        --qt-plugin-dir)
            qt_plugin_dir="${2:?--qt-plugin-dir requires a path}"
            shift 2
            ;;
        --qt-qml-dir)
            qt_qml_dir="${2:?--qt-qml-dir requires a path}"
            shift 2
            ;;
        --output-dir)
            output_dir="${2:?--output-dir requires a path}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "$architecture" in
    amd64)
        xmake_arch=x86_64
        ;;
    arm64)
        xmake_arch=arm64
        ;;
    *)
        xmake_arch="$architecture"
        ;;
esac

if [[ -z "$cli_binary" ]]; then
    cli_binary="$project_dir/build/linux/$xmake_arch/release/mksync"
fi
if [[ "$include_gui" == auto ]]; then
    gui_candidate="$project_dir/build/linux/$xmake_arch/release/mksync-gui"
    if [[ -f "$gui_candidate" ]]; then
        gui_binary="$gui_candidate"
        include_gui=yes
    else
        include_gui=no
    fi
fi

if ((${#private_library_dirs[@]} == 0)); then
    xmake_global_dir="${XMAKE_GLOBALDIR:-${HOME:-}/.xmake}"
    if [[ -d "$xmake_global_dir/packages/i/ilias" ]]; then
        private_library_dirs+=("$xmake_global_dir/packages/i/ilias")
    fi
fi

for command_name in dpkg dpkg-architecture dpkg-deb dpkg-query dpkg-shlibdeps ldd readelf; do
    require_command "$command_name"
done

if [[ -z "$version" ]]; then
    printf 'Could not read the project version; pass --version explicitly.\n' >&2
    exit 1
fi
if ! dpkg --validate-version "$version"; then
    printf 'Invalid Debian package version: %s\n' "$version" >&2
    exit 1
fi
if [[ ! -x "$cli_binary" ]]; then
    printf 'CLI executable is missing or not executable: %s\n' "$cli_binary" >&2
    exit 1
fi
if [[ "$include_gui" == yes && ! -x "$gui_binary" ]]; then
    printf 'GUI executable is missing or not executable: %s\n' "$gui_binary" >&2
    exit 1
fi
for search_dir in \
    "${private_library_dirs[@]+"${private_library_dirs[@]}"}" \
    "${bundle_library_dirs[@]+"${bundle_library_dirs[@]}"}"; do
    if [[ ! -d "$search_dir" ]]; then
        printf 'Library search directory does not exist: %s\n' "$search_dir" >&2
        exit 1
    fi
done
if [[ "$include_gui" == yes && "$bundle_qt" == yes ]]; then
    for qt_dir in "$qt_library_dir" "$qt_plugin_dir" "$qt_qml_dir"; do
        if [[ -z "$qt_dir" || ! -d "$qt_dir" ]]; then
            printf '%s\n' \
                'A valid Qt library, plugin, and QML directory is required for the GUI package.' \
                >&2
            exit 1
        fi
    done
fi

multiarch="$(dpkg-architecture -a"$architecture" -qDEB_HOST_MULTIARCH)"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/mksync-deb.XXXXXX")"
package_root="$work_dir/debian/mksync"
private_lib_dir="$package_root/usr/lib/$multiarch/mksync"
package_bin_dir="$package_root/usr/bin"
package_doc_dir="$package_root/usr/share/doc/mksync"

cleanup() {
    rm -rf -- "$work_dir"
}
trap cleanup EXIT

install -Dm755 "$cli_binary" "$package_bin_dir/mksync"
package_binaries=("$package_bin_dir/mksync")
if [[ "$include_gui" == yes ]]; then
    if [[ "$bundle_qt" == yes ]]; then
        gui_runtime="$private_lib_dir/mksync-gui"
        install -Dm755 "$gui_binary" "$gui_runtime"
        package_binaries+=("$gui_runtime")
        {
            printf '#!/bin/sh\n'
            printf 'runtime_dir=%s\n' "'/usr/lib/$multiarch/mksync'"
            printf '%s%s\n' \
                'export QT_PLUGIN_PATH="$runtime_dir/qt6/plugins' \
                '${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"'
            printf '%s%s\n' \
                'export QML_IMPORT_PATH="$runtime_dir/qt6/qml' \
                '${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"'
            printf '%s%s\n' \
                'export QML2_IMPORT_PATH="$runtime_dir/qt6/qml' \
                '${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"'
            printf 'export QT_QUICK_CONTROLS_STYLE="${QT_QUICK_CONTROLS_STYLE:-Basic}"\n'
            printf 'exec "$runtime_dir/mksync-gui" "$@"\n'
        } >"$package_bin_dir/mksync-gui"
        chmod 755 "$package_bin_dir/mksync-gui"
    else
        install -Dm755 "$gui_binary" "$package_bin_dir/mksync-gui"
        package_binaries+=("$package_bin_dir/mksync-gui")
    fi
fi
install -Dm644 "$project_dir/LICENSE" "$package_doc_dir/copyright"
install -Dm644 "$project_dir/README.md" "$package_doc_dir/README.md"
install -Dm644 "$project_dir/README_zh.md" "$package_doc_dir/README_zh.md"
install -Dm644 "$script_dir/shlibdeps-control" "$work_dir/debian/control"
install -d -m755 "$package_root/DEBIAN"

qt_runtime_dir="$private_lib_dir/qt6"
qt_plugin_runtime_dir="$qt_runtime_dir/plugins"
qt_qml_runtime_dir="$qt_runtime_dir/qml"

copy_directory_files() {
    local source_dir="$1"
    local destination_dir="$2"
    local entry

    [[ -d "$source_dir" ]] || return 1
    install -d -m755 "$destination_dir"
    while IFS= read -r -d '' entry; do
        cp -a -- "$entry" "$destination_dir/"
    done < <(find "$source_dir" -mindepth 1 -maxdepth 1 \( -type f -o -type l \) -print0)
}

copy_directory_tree() {
    local source_dir="$1"
    local destination_dir="$2"

    [[ -d "$source_dir" ]] || return 1
    install -d -m755 "$(dirname -- "$destination_dir")"
    cp -a -- "$source_dir" "$destination_dir"
}

copy_qml_module_files() {
    local relative_dir="$1"
    copy_directory_files "$qt_qml_dir/$relative_dir" "$qt_qml_runtime_dir/$relative_dir"
}

copy_qml_module_tree() {
    local relative_dir="$1"
    copy_directory_tree "$qt_qml_dir/$relative_dir" "$qt_qml_runtime_dir/$relative_dir"
}

copy_qt_plugin() {
    local relative_file="$1"
    local source_file="$qt_plugin_dir/$relative_file"
    [[ -f "$source_file" ]] || return 1
    install -Dm755 "$source_file" "$qt_plugin_runtime_dir/$relative_file"
}

copy_qt_plugin_group() {
    local relative_dir="$1"
    local source_dir="$qt_plugin_dir/$relative_dir"
    local plugin

    [[ -d "$source_dir" ]] || return 0
    while IFS= read -r -d '' plugin; do
        install -Dm755 "$plugin" "$qt_plugin_runtime_dir/$relative_dir/$(basename -- "$plugin")"
    done < <(find "$source_dir" -maxdepth 1 -type f -name '*.so' -print0)
}

if [[ "$include_gui" == yes && "$bundle_qt" == yes ]]; then
    # Bundle only the imports used by mksync. Basic is fixed as the fallback Controls style so the
    # other style engines and designer metadata do not inflate the package.
    for qml_parent in QtCore QtQml QtQuick QtQuick/Controls QtQuick/Dialogs; do
        copy_qml_module_files "$qml_parent"
    done
    for qml_module in \
        QtQml/Models \
        QtQml/WorkerScript \
        QtQuick/Controls/Basic \
        QtQuick/Controls/impl \
        QtQuick/Dialogs/quickimpl \
        QtQuick/Layouts \
        QtQuick/Shapes \
        QtQuick/Templates \
        QtQuick/Window \
        Qt/labs/folderlistmodel; do
        copy_qml_module_tree "$qml_module"
    done
    find "$qt_qml_runtime_dir/QtQuick/Dialogs/quickimpl" \
        -type d \( \
        -name '+Fusion' -o -name '+Imagine' -o -name '+Material' -o -name '+Universal' \
        \) \
        -prune -exec rm -rf -- {} +

    copy_qt_plugin platforms/libqxcb.so
    copy_qt_plugin platforms/libqwayland-egl.so
    copy_qt_plugin platforms/libqwayland-generic.so
    for plugin_group in \
        wayland-decoration-client \
        wayland-graphics-integration-client \
        wayland-shell-integration \
        xcbglintegrations; do
        copy_qt_plugin_group "$plugin_group"
    done
fi

binary_machine="$(
    readelf -h "$package_bin_dir/mksync" \
        | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p'
)"

is_host_runtime_library() {
    case "$1" in
        ld-linux*.so.* | linux-vdso.so.* | libc.so.* | libanl.so.* | libdl.so.* | libm.so.* | \
            libnss_*.so.* | libpthread.so.* | libresolv.so.* | librt.so.* | libthread_db.so.* | \
            libutil.so.*)
            return 0
            ;;
        # Keep the host graphics dispatch/driver boundary. These libraries must match the
        # installed Mesa/NVIDIA stack.
        libEGL.so.* | libGL.so.* | libGLX.so.* | libGLdispatch.so.* | libOpenGL.so.* | \
            libdrm.so.* | libgbm.so.* | libvulkan.so.*)
            return 0
            ;;
        # Protocol clients and desktop service ABIs belong to the target system. Copying them from
        # the build image can conflict with the target compositor, X server, portal, or system bus.
        libX*.so.* | libxcb*.so.* | libxkbcommon*.so.* | libwayland-*.so.* | \
            libdecor-*.so.* | libinput.so.* | libei.so.* | liboeffis.so.* | libportal.so.* | \
            libglib-2.0.so.* | libgio-2.0.so.* | libgmodule-2.0.so.* | libgobject-2.0.so.* | \
            libdbus-1.so.* | libsystemd.so.* | libudev.so.*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

find_bundled_library() {
    local library_name="$1"
    local candidate
    local candidate_machine
    local candidate_mtime
    local newest_mtime=-1
    local selected=""

    if is_host_runtime_library "$library_name"; then
        return 1
    fi

    for search_dir in "${private_library_dirs[@]+"${private_library_dirs[@]}"}"; do
        while IFS= read -r -d '' candidate; do
            candidate="$(readlink -f -- "$candidate")"
            [[ -f "$candidate" ]] || continue
            candidate_machine="$(
                readelf -h "$candidate" 2>/dev/null \
                    | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p'
            )"
            [[ "$candidate_machine" == "$binary_machine" ]] || continue
            candidate_mtime="$(stat -c '%Y' "$candidate")"
            if ((candidate_mtime > newest_mtime)); then
                newest_mtime=$candidate_mtime
                selected="$candidate"
            fi
        done < <(find "$search_dir" \( -type f -o -type l \) -name "$library_name" -print0)
    done

    # The self-contained Qt variant carries Qt itself, not every system library Qt was linked
    # against. Keep those ABI-stable dependencies under the distribution package manager.
    if [[ -z "$selected" && "$bundle_qt" == yes && "$library_name" == libQt6*.so.* ]]; then
        while IFS= read -r -d '' candidate; do
            candidate="$(readlink -f -- "$candidate")"
            [[ -f "$candidate" ]] || continue
            candidate_machine="$(
                readelf -h "$candidate" 2>/dev/null \
                    | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p'
            )"
            [[ "$candidate_machine" == "$binary_machine" ]] || continue
            candidate_mtime="$(stat -c '%Y' "$candidate")"
            if ((candidate_mtime > newest_mtime)); then
                newest_mtime=$candidate_mtime
                selected="$candidate"
            fi
        done < <(
            find "$qt_library_dir" -maxdepth 1 \( -type f -o -type l \) \
                -name "$library_name" -print0
        )
    fi

    # Explicit bundle directories remain an escape hatch for non-distribution private libraries.
    if [[ -z "$selected" ]]; then
        for search_dir in "${bundle_library_dirs[@]+"${bundle_library_dirs[@]}"}"; do
            while IFS= read -r -d '' candidate; do
                candidate="$(readlink -f -- "$candidate")"
                [[ -f "$candidate" ]] || continue
                candidate_machine="$(
                    readelf -h "$candidate" 2>/dev/null \
                        | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p'
                )"
                [[ "$candidate_machine" == "$binary_machine" ]] || continue
                candidate_mtime="$(stat -c '%Y' "$candidate")"
                if ((candidate_mtime > newest_mtime)); then
                    newest_mtime=$candidate_mtime
                    selected="$candidate"
                fi
            done < <(
                find "$search_dir" \( -type f -o -type l \) -name "$library_name" -print0
            )
        done
    fi

    [[ -n "$selected" ]] || return 1
    printf '%s\n' "$selected"
}

copied_libraries=()
declare -A bundled_source_packages=()
copy_bundled_library() {
    local requested_name="$1"
    local source_path
    local real_name
    local soname
    local destination

    if ! source_path="$(find_bundled_library "$requested_name")"; then
        return 1
    fi
    real_name="$(basename -- "$source_path")"
    destination="$private_lib_dir/$real_name"
    if [[ ! -f "$destination" ]]; then
        install -Dm755 "$source_path" "$destination"
        copied_libraries+=("$destination")
        printf 'Bundled runtime library %s from %s\n' "$requested_name" "$source_path"
        while IFS=: read -r owning_package _; do
            [[ -n "$owning_package" ]] && bundled_source_packages[$owning_package]=yes
        done < <(dpkg-query -S "$source_path" 2>/dev/null || true)
    fi

    soname="$(
        readelf -d "$destination" \
            | sed -n 's/.*(SONAME).*\[\(.*\)\].*/\1/p' \
            | head -n 1
    )"
    if [[ -z "$soname" ]]; then
        soname="$requested_name"
    fi
    if [[ "$soname" != "$real_name" && ! -e "$private_lib_dir/$soname" ]]; then
        ln -s "$real_name" "$private_lib_dir/$soname"
    fi
    if [[ "$requested_name" != "$real_name" && ! -e "$private_lib_dir/$requested_name" ]]; then
        ln -s "$real_name" "$private_lib_dir/$requested_name"
    fi
}

staged_runtime_elf_files=()
if [[ -d "$qt_runtime_dir" ]]; then
    while IFS= read -r -d '' runtime_file; do
        if readelf -h "$runtime_file" >/dev/null 2>&1; then
            staged_runtime_elf_files+=("$runtime_file")
        fi
    done < <(find "$qt_runtime_dir" -type f -print0)
fi

elf_files=("${package_binaries[@]}")
elf_files+=("${staged_runtime_elf_files[@]+"${staged_runtime_elf_files[@]}"}")
declare -A inspected_libraries=()
for ((elf_index = 0; elf_index < ${#elf_files[@]}; ++elf_index)); do
    elf_file="${elf_files[$elf_index]}"
    while IFS= read -r needed_library; do
        [[ -n "$needed_library" ]] || continue
        if [[ -z "${inspected_libraries[$needed_library]:-}" ]]; then
            inspected_libraries[$needed_library]=yes
            copied_count=${#copied_libraries[@]}
            if copy_bundled_library "$needed_library" \
                && ((${#copied_libraries[@]} > copied_count)); then
                elf_files+=("${copied_libraries[-1]}")
            fi
        fi
    done < <(readelf -d "$elf_file" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p')
done

if ((${#copied_libraries[@]} > 0 || ${#staged_runtime_elf_files[@]} > 0)); then
    require_command patchelf
    for binary in "${package_binaries[@]}"; do
        if [[ "$binary" == "$package_bin_dir/"* ]]; then
            patchelf --set-rpath "\$ORIGIN/../lib/$multiarch/mksync" "$binary"
        else
            patchelf --set-rpath '$ORIGIN' "$binary"
        fi
    done
    for library in "${copied_libraries[@]}"; do
        patchelf --set-rpath '$ORIGIN' "$library"
    done
    for runtime_file in "${staged_runtime_elf_files[@]+"${staged_runtime_elf_files[@]}"}"; do
        patchelf --set-rpath "\$ORIGIN:/usr/lib/$multiarch/mksync" "$runtime_file"
    done
fi

elf_files=("${package_binaries[@]}")
elf_files+=("${staged_runtime_elf_files[@]+"${staged_runtime_elf_files[@]}"}")
elf_files+=("${copied_libraries[@]+"${copied_libraries[@]}"}")
for elf_file in "${elf_files[@]}"; do
    ldd_output="$(LD_LIBRARY_PATH="$private_lib_dir" ldd "$elf_file" 2>&1)"
    if grep -q 'not found' <<<"$ldd_output"; then
        printf 'The staged ELF file still has unresolved libraries: %s\n' "$elf_file" >&2
        printf '%s\n' "$ldd_output" >&2
        exit 1
    fi
done

shlibdeps_args=()
for elf_file in "${elf_files[@]}"; do
    shlibdeps_args+=("-e${elf_file#"$work_dir/"}")
done
for bundled_package in "${!bundled_source_packages[@]}"; do
    shlibdeps_args+=("-x$bundled_package")
done
shlibdeps_output="$(
    cd -- "$work_dir"
    dpkg-shlibdeps -O --ignore-missing-info --package=mksync \
        "-Sdebian/mksync" "-l$private_lib_dir" "${shlibdeps_args[@]}"
)"
auto_dependencies="$(sed -n 's/^shlibs:Depends=//p' <<<"$shlibdeps_output")"
if [[ -z "$auto_dependencies" ]]; then
    printf 'dpkg-shlibdeps did not produce runtime dependencies.\n' >&2
    exit 1
fi

declare -A dependency_names=()
dependencies=()
append_dependency() {
    local dependency="$1"
    local name="${dependency%% *}"
    name="${name%%(*}"
    if [[ -z "${dependency_names[$name]:-}" ]]; then
        dependency_names[$name]=yes
        dependencies+=("$dependency")
    fi
}

IFS=',' read -r -a detected_dependencies <<<"$auto_dependencies"
for dependency in "${detected_dependencies[@]}"; do
    dependency="${dependency#${dependency%%[![:space:]]*}}"
    dependency="${dependency%${dependency##*[![:space:]]}}"
    [[ -n "$dependency" ]] && append_dependency "$dependency"
done
if [[ "$include_gui" == yes && "$bundle_qt" == no ]]; then
    # QML imports and QPA plugins are runtime-loaded rather than ELF DT_NEEDED entries, so
    # dpkg-shlibdeps cannot discover them. Their packages pull in matching transitive modules and
    # keep the Qt ABI version under the distribution package manager's control.
    append_dependency qml6-module-qtquick
    append_dependency qml6-module-qtquick-controls
    append_dependency qml6-module-qtquick-dialogs
    append_dependency qml6-module-qtquick-layouts
    append_dependency qt6-qpa-plugins
    append_dependency qt6-wayland
fi

dependency_text=""
for dependency in "${dependencies[@]}"; do
    if [[ -n "$dependency_text" ]]; then
        dependency_text+=", "
    fi
    dependency_text+="$dependency"
done

installed_size="$(du -sk "$package_root/usr" | awk '{print $1}')"
{
    printf 'Package: mksync\n'
    printf 'Version: %s\n' "$version"
    printf 'Section: utils\n'
    printf 'Priority: optional\n'
    printf 'Architecture: %s\n' "$architecture"
    printf 'Maintainer: Btk-Project <btk-project@users.noreply.github.com>\n'
    printf 'Installed-Size: %s\n' "$installed_size"
    printf 'Depends: %s\n' "$dependency_text"
    printf 'Homepage: https://github.com/Btk-Project/mksync\n'
    printf 'Description: Mouse and keyboard synchronization tool\n'
    printf ' mksync shares mouse and keyboard input between computers.\n'
} >"$package_root/DEBIAN/control"

if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
    SOURCE_DATE_EPOCH="$(git -C "$project_dir" log -1 --format=%ct 2>/dev/null || date +%s)"
fi
export SOURCE_DATE_EPOCH
while IFS= read -r -d '' staged_path; do
    touch --no-dereference --date="@$SOURCE_DATE_EPOCH" "$staged_path"
done < <(find "$package_root" -print0)

mkdir -p -- "$output_dir"
package_path="$output_dir/mksync_${version}_${architecture}.deb"
dpkg-deb --root-owner-group --build "$package_root" "$package_path"
dpkg-deb --info "$package_path" >/dev/null

printf 'Created %s\n' "$package_path"
