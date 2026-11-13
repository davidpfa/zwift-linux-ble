#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DEST="${HOME}/.local/bin"
SHARE_DEST="${HOME}/.local/share/zwift-ble"
DOCS_DEST="${HOME}/.local/share/zwift/Documents"
SERVICE_DEST="${HOME}/.config/systemd/user"
ZWIFT_CONFIG="${HOME}/.config/zwift/config"

echo "=== Installing zwift-linux-ble ==="

# 0. Check Build Dependencies
MISSING_DEPS=()
for cmd in cargo clang lld-link llvm-dlltool; do
    if ! command -v "$cmd" &>/dev/null; then
        MISSING_DEPS+=("$cmd")
    fi
done

if [[ ${#MISSING_DEPS[@]} -gt 0 ]]; then
    echo "[-] Missing required build tools: ${MISSING_DEPS[*]}"
    echo ""
    echo "Please install them using your distribution's package manager:"
    echo "  • Arch / CachyOS / EndeavourOS:"
    echo "      sudo pacman -S --needed base-devel rust clang lld llvm"
    echo ""
    echo "  • Ubuntu / Debian:"
    echo "      sudo apt update && sudo apt install -y build-essential rustc cargo clang lld llvm libdbus-1-dev"
    echo ""
    echo "  • Fedora:"
    echo "      sudo dnf install -y rust cargo clang lld llvm dbus-devel"
    echo ""
    exit 1
fi

mkdir -p "${BIN_DEST}" "${SHARE_DEST}" "${DOCS_DEST}" "${SERVICE_DEST}"

# Ensure ~/.local/bin is in PATH for future shell sessions
if [[ ":$PATH:" != *":$BIN_DEST:"* ]]; then
    echo "[!] Notice: ${BIN_DEST} is not currently in your PATH."
    # Configure fish shell if installed
    if command -v fish &>/dev/null; then
        fish -c "fish_add_path -U ${BIN_DEST}" 2>/dev/null || true
    fi
    # Configure bash / zsh
    if [[ -f "${HOME}/.bashrc" ]] && ! grep -q '\.local/bin' "${HOME}/.bashrc"; then
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> "${HOME}/.bashrc"
    fi
    if [[ -f "${HOME}/.zshrc" ]] && ! grep -q '\.local/bin' "${HOME}/.zshrc"; then
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> "${HOME}/.zshrc"
    fi
    echo "[+] Added ${BIN_DEST} to your shell configuration."
fi

# 1. Install or compile binaries
if [[ -f "${SCRIPT_DIR}/bin/zwift-ble-daemon" && -f "${SCRIPT_DIR}/bin/BleWin10Lib.dll" ]]; then
    echo "[+] Using pre-built release artifacts from bin/..."
    install -m 755 "${SCRIPT_DIR}/bin/zwift-ble-daemon" "${BIN_DEST}/zwift-ble-daemon"
    install -m 644 "${SCRIPT_DIR}/bin/BleWin10Lib.dll" "${SHARE_DEST}/BleWin10Lib.dll"
else
    echo "[+] Building zwift-ble-daemon from source..."
    (cd "${SCRIPT_DIR}/daemon" && cargo build --release)
    install -m 755 "${SCRIPT_DIR}/daemon/target/release/zwift-ble-daemon" "${BIN_DEST}/zwift-ble-daemon"

    echo "[+] Building BleWin10Lib.dll..."
    (cd "${SCRIPT_DIR}/dll" && ./build.sh)
    install -m 644 "${SCRIPT_DIR}/dll/BleWin10Lib.dll" "${SHARE_DEST}/BleWin10Lib.dll"
fi
chmod +x "${BIN_DEST}/zwift-ble-daemon"

# 2. Install systemd user service
echo "[+] Configuring systemd user service..."
cp "${SCRIPT_DIR}/systemd/zwift-ble-daemon.service" "${SERVICE_DEST}/"
systemctl --user daemon-reload
systemctl --user enable zwift-ble-daemon.service
systemctl --user restart zwift-ble-daemon.service

# 3. Configure ~/.config/zwift/config
echo "[+] Updating netbrain Zwift config..."
mkdir -p "$(dirname "${ZWIFT_CONFIG}")"
touch "${ZWIFT_CONFIG}"

# Ensure NETWORKING="host"
if grep -q '^NETWORKING=' "${ZWIFT_CONFIG}"; then
    sed -i 's/^NETWORKING=.*/NETWORKING="host"/' "${ZWIFT_CONFIG}"
else
    echo 'NETWORKING="host"' >> "${ZWIFT_CONFIG}"
fi

# Ensure DISABLE_BLUETOOTH="0"
if grep -q '^DISABLE_BLUETOOTH=' "${ZWIFT_CONFIG}"; then
    sed -i 's/^DISABLE_BLUETOOTH=.*/DISABLE_BLUETOOTH="0"/' "${ZWIFT_CONFIG}"
else
    echo 'DISABLE_BLUETOOTH="0"' >> "${ZWIFT_CONFIG}"
fi

# Ensure CONTAINER_EXTRA_ARGS mounts:
#  1. BleWin10Lib.dll read-only (native Bluetooth bridge)
#  2. Documents/Zwift on host (persists sensor pairings, knowndevices.xml, and prefs.xml)
if grep -q 'CONTAINER_EXTRA_ARGS=' "${ZWIFT_CONFIG}"; then
    sed -i '/CONTAINER_EXTRA_ARGS=/d' "${ZWIFT_CONFIG}"
fi

DLL_MOUNT="-v \"${SHARE_DEST}/BleWin10Lib.dll:/home/user/.wine/drive_c/Program Files (x86)/Zwift/BleWin10Lib.dll:ro\""
DOCS_MOUNT="-v \"${DOCS_DEST}:/home/user/.wine/drive_c/users/user/Documents/Zwift\""

echo "CONTAINER_EXTRA_ARGS=(\"-v\" \"${SHARE_DEST}/BleWin10Lib.dll:/home/user/.wine/drive_c/Program Files (x86)/Zwift/BleWin10Lib.dll:ro\" \"-v\" \"${DOCS_DEST}:/home/user/.wine/drive_c/users/user/Documents/Zwift\")" >> "${ZWIFT_CONFIG}"

echo "=== Installation Complete! ==="
echo "You can now launch Zwift directly from your desktop app menu or terminal ('zwift')."
if [[ ":$PATH:" != *":$BIN_DEST:"* ]]; then
    echo "NOTE: Restart your terminal session or run 'export PATH=\"\$HOME/.local/bin:\$PATH\"' (or 'fish_add_path ~/.local/bin') to use 'zwift' directly in this terminal."
fi
echo "Check daemon status at any time with: systemctl --user status zwift-ble-daemon"
