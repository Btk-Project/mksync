#!/usr/bin/env bash

set -euo pipefail

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

for command_name in dpkg dpkg-architecture dpkg-deb dpkg-shlibdeps ldd readelf; do
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
for search_dir in "${private_library_dirs[@]+"${private_library_dirs[@]}"}"; do
    if [[ ! -d "$search_dir" ]]; then
        printf 'Private library search directory does not exist: %s\n' "$search_dir" >&2
        exit 1
    fi
done

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
    install -Dm755 "$gui_binary" "$package_bin_dir/mksync-gui"
    package_binaries+=("$package_bin_dir/mksync-gui")
fi
install -Dm644 "$project_dir/LICENSE" "$package_doc_dir/copyright"
install -Dm644 "$project_dir/README.md" "$package_doc_dir/README.md"
install -Dm644 "$project_dir/README_zh.md" "$package_doc_dir/README_zh.md"
install -Dm644 "$script_dir/shlibdeps-control" "$work_dir/debian/control"
install -d -m755 "$package_root/DEBIAN"

binary_machine="$(
    readelf -h "$package_bin_dir/mksync" \
        | sed -n 's/^[[:space:]]*Machine:[[:space:]]*//p'
)"

find_private_library() {
    local library_name="$1"
    local candidate
    local candidate_machine
    local candidate_mtime
    local newest_mtime=-1
    local selected=""

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

    [[ -n "$selected" ]] || return 1
    printf '%s\n' "$selected"
}

copied_libraries=()
copy_private_library() {
    local requested_name="$1"
    local source_path
    local real_name
    local soname
    local destination

    if ! source_path="$(find_private_library "$requested_name")"; then
        return 1
    fi
    real_name="$(basename -- "$source_path")"
    destination="$private_lib_dir/$real_name"
    if [[ ! -f "$destination" ]]; then
        install -Dm755 "$source_path" "$destination"
        copied_libraries+=("$destination")
        printf 'Bundled private library %s from %s\n' "$requested_name" "$source_path"
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

declare -A inspected_libraries=()
for binary in "${package_binaries[@]}"; do
    while IFS= read -r needed_library; do
        [[ -n "$needed_library" ]] || continue
        if [[ -z "${inspected_libraries[$needed_library]:-}" ]]; then
            inspected_libraries[$needed_library]=yes
            copy_private_library "$needed_library" || true
        fi
    done < <(readelf -d "$binary" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p')
done

if ((${#copied_libraries[@]} > 0)); then
    require_command patchelf
    binary_rpath="\$ORIGIN/../lib/$multiarch/mksync"
    for binary in "${package_binaries[@]}"; do
        patchelf --set-rpath "$binary_rpath" "$binary"
    done
    for library in "${copied_libraries[@]}"; do
        patchelf --set-rpath '$ORIGIN' "$library"
    done
fi

elf_files=("${package_binaries[@]}")
elf_files+=("${copied_libraries[@]+"${copied_libraries[@]}"}")
for ((round = 0; round < 16; ++round)); do
    missing_libraries=()
    while IFS= read -r missing_library; do
        [[ -n "$missing_library" ]] && missing_libraries+=("$missing_library")
    done < <(
        for elf_file in "${elf_files[@]}"; do
            LD_LIBRARY_PATH="$private_lib_dir" ldd "$elf_file" 2>/dev/null \
                | awk '$2 == "=>" && $3 == "not" && $4 == "found" {print $1}'
        done | sort -u
    )
    if ((${#missing_libraries[@]} == 0)); then
        break
    fi
    if ((round == 15)); then
        printf 'Too many rounds while resolving private shared libraries.\n' >&2
        exit 1
    fi
    for missing_library in "${missing_libraries[@]}"; do
        if ! copy_private_library "$missing_library"; then
            printf 'Unresolved shared library: %s\n' "$missing_library" >&2
            printf 'Pass its build directory with --private-library-dir.\n' >&2
            exit 1
        fi
        elf_files=("${package_binaries[@]}")
        elf_files+=("${copied_libraries[@]}")
        patchelf --set-rpath '$ORIGIN' "${copied_libraries[-1]}"
    done
done

for binary in "${package_binaries[@]}"; do
    if LD_LIBRARY_PATH="$private_lib_dir" ldd "$binary" | grep -q 'not found'; then
        printf 'The staged executable still has unresolved libraries: %s\n' "$binary" >&2
        exit 1
    fi
done

shlibdeps_args=()
for elf_file in "${package_binaries[@]}" "${copied_libraries[@]+"${copied_libraries[@]}"}"; do
    shlibdeps_args+=("-e${elf_file#"$work_dir/"}")
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
if [[ "$include_gui" == yes ]]; then
    append_dependency qml6-module-qtquick
    append_dependency qml6-module-qtquick-controls
    append_dependency qml6-module-qtquick-dialogs
    append_dependency qml6-module-qtquick-layouts
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
