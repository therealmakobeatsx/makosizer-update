#!/usr/bin/env bash
set -euo pipefail

version="${1:?usage: package-macos.sh VERSION VST3_BUNDLE AU_BUNDLE OUTPUT_DIR}"
vst3_bundle="${2:?missing VST3 bundle path}"
au_bundle="${3:?missing AU bundle path}"
output_dir="${4:?missing output directory}"
version="${version#v}"

[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+([-+][0-9A-Za-z.-]+)?$ ]] || {
    echo "Invalid package version: $version" >&2
    exit 1
}
[[ -d "$vst3_bundle" ]] || { echo "VST3 bundle not found: $vst3_bundle" >&2; exit 1; }
[[ -d "$au_bundle" ]] || { echo "AU component not found: $au_bundle" >&2; exit 1; }

mkdir -p "$output_dir"
stage="$(mktemp -d "${TMPDIR:-/tmp}/makosizerx-package.XXXXXX")"
trap 'rm -rf -- "$stage"' EXIT

mkdir -p \
    "$stage/Library/Audio/Plug-Ins/VST3" \
    "$stage/Library/Audio/Plug-Ins/Components"
ditto "$vst3_bundle" "$stage/Library/Audio/Plug-Ins/VST3/MakosizerX.vst3"
ditto "$au_bundle" "$stage/Library/Audio/Plug-Ins/Components/MakosizerX.component"

pkgbuild \
    --root "$stage" \
    --identifier com.makosizerx.plugininstaller \
    --version "$version" \
    --install-location / \
    "$output_dir/MakosizerX.pkg"

ditto -c -k --sequesterRsrc --keepParent \
    "$vst3_bundle" "$output_dir/MakosizerX-macOS-VST3.zip"
ditto -c -k --sequesterRsrc --keepParent \
    "$au_bundle" "$output_dir/MakosizerX-macOS-AU.zip"

test -s "$output_dir/MakosizerX.pkg"
test -s "$output_dir/MakosizerX-macOS-VST3.zip"
test -s "$output_dir/MakosizerX-macOS-AU.zip"
