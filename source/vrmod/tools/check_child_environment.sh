#!/usr/bin/env bash
# The loader's hook on process creation, without the game (2026-11-12; tests/child_environment_process_check.cpp):
# a folder with the check program as "enshrouded.exe", an EnshroudedVR folder, the loader (dist/dinput8.dll) and a copy
# of cmd.exe; started with the development launch's variables. Usage: tools/check_child_environment.sh <empty folder>
set -u
mod="$(cd "$(dirname "$0")/.." && pwd)"
dir="${1:?an empty folder}"
mkdir -p "$dir/EnshroudedVR"
cp "$mod/build/bin/Release/evr_child_environment_process_check.exe" "$dir/enshrouded.exe"
cp "$mod/dist/dinput8.dll" "$dir/dinput8.dll"
cp /c/Windows/System32/cmd.exe "$dir/owncmd.exe"
win="$(cygpath -w "$dir")"
(cd "$dir" && env EVR_LAYER_OUTPUT="$win\\layer" EVR_SCREEN_XR=1 VK_INSTANCE_LAYERS=VK_LAYER_EVR_diagnostic VK_LAYER_PATH='S:\mod\bin' \
    XR_RUNTIME_JSON='S:\mock_runtime.json' ENABLE_VULKAN_RENDERDOC_CAPTURE=1 KEEP_ME=yes ./enshrouded.exe)
echo "exit $?"
fail=0
for f in inherited block shell; do
    file="$dir/out/$f.txt"
    if [ ! -s "$file" ]; then echo "FAIL $f: no file"; fail=1; continue; fi
    bad=$(grep -iE '^(EVR_|VK_INSTANCE_LAYERS=|VK_LAYER_PATH=|XR_RUNTIME_JSON=|ENABLE_VULKAN_RENDERDOC_CAPTURE=)' "$file")
    if [ -n "$bad" ]; then echo "FAIL $f: the mod's variables reached it:"; echo "$bad"; fail=1; fi
    grep -q '^KEEP_ME=yes' "$file" || { echo "FAIL $f: an ordinary variable was lost"; fail=1; }
    grep -qi '^Path=' "$file" || { echo "FAIL $f: Path was lost"; fail=1; }
    [ $fail = 0 ] && echo "ok $f: $(wc -l < "$file") variables, none of the mod's"
done
own="$dir/out/own.txt"
if grep -q '^EVR_SCREEN_XR=1' "$own" && grep -q '^VK_INSTANCE_LAYERS=VK_LAYER_EVR_diagnostic' "$own"; then echo "ok own: the program in the game's folder kept them"
else echo "FAIL own: the program in the game's folder lost the variables"; fail=1; fi
[ $fail = 0 ] && echo "child environment check: PASS" || echo "child environment check: FAIL"
exit $fail
