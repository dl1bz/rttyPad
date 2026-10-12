#!/usr/bin/env bash
# rttyPad: standalone Windows x64 distribution (MSYS2 UCRT64).
set -euo pipefail

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    echo 'ERROR: Run this script in the MSYS2 UCRT64 shell.' >&2
    exit 1
fi

for cmd in cmake ninja windeployqt6 qtpaths6 objdump cygpath powershell.exe; do
    command -v "$cmd" >/dev/null || { echo "ERROR: Missing $cmd" >&2; exit 1; }
done

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build-release"
DIST="$ROOT/dist/rttyPad"
ZIP="$ROOT/dist/rttyPad-windows-x64.zip"

cd "$ROOT"
cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --parallel 4

# Always start from a clean bundle: no obsolete plugins or DLLs.
rm -rf "$DIST"
mkdir -p "$DIST"
cp "$BUILD/rttyPad.exe" "$DIST/"
windeployqt6 --release --compiler-runtime "$DIST/rttyPad.exe"

# Keep the Windows Schannel TLS backend. It is needed by QtNetwork SSL.
QT_PLUGINS="$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || true)"
if [[ ! -f "$DIST/tls/qschannelbackend.dll" ]]; then
    if [[ -n "$QT_PLUGINS" && -f "$QT_PLUGINS/tls/qschannelbackend.dll" ]]; then
        mkdir -p "$DIST/tls"
        cp "$QT_PLUGINS/tls/qschannelbackend.dll" "$DIST/tls/"
    else
        echo 'ERROR: Qt Schannel TLS plugin not found.' >&2
        exit 1
    fi
fi

# qglib depends on the complete GLib/GIO stack; Windows has its native
# network-list-manager plugin, so do not ship qglib.
rm -f "$DIST/networkinformation/qglib.dll"

# Resolve the *transitive* DLL imports of every executable, Qt DLL and plugin.
# Copy only UCRT64 DLLs; Windows system DLLs are deliberately not bundled.
declare -A scanned=()
queue=()
while IFS= read -r -d '' file; do queue+=("$file"); done < <(
    find "$DIST" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0
)

index=0
while (( index < ${#queue[@]} )); do
    file="${queue[index]}"
    ((index+=1))
    key="${file,,}"
    [[ -n "${scanned[$key]:-}" ]] && continue
    scanned[$key]=1

    while IFS= read -r name; do
        [[ -n "$name" ]] || continue
        # Case-insensitive DLL lookup, as on Windows.
        if find "$DIST" -maxdepth 1 -type f -iname "$name" -print -quit | grep -q .; then
            continue
        fi
        source="$(find /ucrt64/bin -maxdepth 1 -type f -iname "$name" -print -quit)"
        if [[ -n "$source" ]]; then
            dest="$DIST/$(basename "$source")"
            cp "$source" "$dest"
            echo "DLL: $(basename "$source")"
            queue+=("$dest")
        fi
    done < <(objdump -p "$file" 2>/dev/null | sed -n 's/^[[:space:]]*DLL Name: //p' | sort -fu)
done

# Check all imports once more. Imports not in the bundle and not found under
# /ucrt64/bin are assumed to be Windows system DLLs; list them for inspection.
echo '--- DLLs in bundle ---'
find "$DIST" -type f -iname '*.dll' | wc -l

echo '--- Bundle size ---'
du -sh "$DIST"

# Compress the entire directory, including Qt plugin subdirectories.
mkdir -p "$(dirname "$ZIP")"
DIST_WIN="$(cygpath -w "$DIST")"
ZIP_WIN="$(cygpath -w "$ZIP")"
powershell.exe -NoProfile -NonInteractive -Command \
    "Compress-Archive -LiteralPath '$DIST_WIN' -DestinationPath '$ZIP_WIN' -Force" \
    </dev/null

echo "READY: $ZIP"
