#!/usr/bin/env bash

set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tools_dir="${root_dir}/.appimage-tools"
build_dir="${root_dir}/build-appimage"
app_dir="${root_dir}/AppDir"
output="${root_dir}/Hexboard-x86_64.AppImage"

mkdir -p "${tools_dir}"

download_tool() {
    local path="$1"
    local url="$2"
    local checksum="$3"
    if [[ ! -f "${path}" ]]; then
        curl --fail --location --output "${path}.download" "${url}"
        printf '%s  %s\n' "${checksum}" "${path}.download" | sha256sum --check
        mv "${path}.download" "${path}"
    elif ! printf '%s  %s\n' "${checksum}" "${path}" | sha256sum --check; then
        printf 'Refusing to execute an AppImage tool with an unexpected checksum.\n' >&2
        exit 1
    fi
    chmod +x "${path}"
}

download_tool \
    "${tools_dir}/linuxdeploy" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
    "421ca71d5c69ea97c6309276232990d43df1dcece0edfaa26bbf926ff96ed12e"
download_tool \
    "${tools_dir}/linuxdeploy-plugin-qt" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
    "cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617"
download_tool \
    "${tools_dir}/appimagetool" \
    "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" \
    "a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0"

cmake -S "${root_dir}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${build_dir}" --parallel
cmake -E rm -rf "${app_dir}"
DESTDIR="${app_dir}" cmake --install "${build_dir}" --prefix /usr

PATH="${tools_dir}:${PATH}" \
QMAKE="${QMAKE:-qmake6}" \
ARCH=x86_64 \
NO_STRIP=1 \
"${tools_dir}/linuxdeploy" \
    --appimage-extract-and-run \
    --appdir "${app_dir}" \
    --plugin qt

# linuxdeploy's bundled patchelf cannot safely rewrite Fedora's DT_RELR
# libraries. Restore pristine system copies and use environment-based lookup.
for library in "${app_dir}"/usr/lib/*; do
    name="$(basename "${library}")"
    source_path=""
    for candidate in \
        "/usr/lib64/${name}" \
        "/lib64/${name}" \
        "/usr/lib64/libproxy/${name}"; do
        if [[ -e "${candidate}" ]]; then
            source_path="${candidate}"
            break
        fi
    done
    if [[ -z "${source_path}" ]]; then
        printf 'Could not locate original library: %s\n' "${name}" >&2
        exit 1
    fi
    cp --dereference --preserve=mode,timestamps \
        --remove-destination "${source_path}" "${library}"
done

while IFS= read -r -d '' plugin; do
    relative_path="${plugin#"${app_dir}/usr/plugins/"}"
    source_path="/usr/lib64/qt6/plugins/${relative_path}"
    if [[ ! -e "${source_path}" ]]; then
        printf 'Could not locate original Qt plugin: %s\n' "${relative_path}" >&2
        exit 1
    fi
    cp --dereference --preserve=mode,timestamps \
        --remove-destination "${source_path}" "${plugin}"
done < <(find "${app_dir}/usr/plugins" -type f -print0)

cp "${build_dir}/hexboard" "${app_dir}/usr/bin/hexboard"
cmake -E rm -f "${app_dir}/AppRun"
printf '%s\n' \
    '#!/bin/sh' \
    'app_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"' \
    'export LD_LIBRARY_PATH="${app_dir}/usr/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"' \
    'export QT_PLUGIN_PATH="${app_dir}/usr/plugins"' \
    'exec "${app_dir}/usr/bin/hexboard" "$@"' \
    > "${app_dir}/AppRun"
chmod +x "${app_dir}/AppRun"

rm -f "${output}"
ARCH=x86_64 \
"${tools_dir}/appimagetool" \
    --appimage-extract-and-run \
    "${app_dir}" \
    "${output}"

printf 'Created %s\n' "${output}"
