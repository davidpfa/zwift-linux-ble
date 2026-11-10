#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Building BleWin10Lib.dll for Windows x86_64..."
cd "${SCRIPT_DIR}"

llvm-dlltool -m i386:x86-64 -d kernel32.def -l kernel32.lib
llvm-dlltool -m i386:x86-64 -d ws2_32.def -l ws2_32.lib

clang --target=x86_64-pc-windows-msvc \
    -O2 -fno-builtin -ffreestanding \
    -c ble_bridge.c -o ble_bridge.obj

lld-link /DLL /NOENTRY /ENTRY:_DllMainCRTStartup \
    /DEF:BleWin10Lib.def \
    /OUT:BleWin10Lib.dll \
    ble_bridge.obj kernel32.lib ws2_32.lib

echo "Successfully built BleWin10Lib.dll"
