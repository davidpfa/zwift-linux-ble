#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DEST="${HOME}/.local/bin"
SHARE_DEST="${HOME}/.local/share/zwift-ble"
SERVICE_DEST="${HOME}/.config/systemd/user"
ZWIFT_CONFIG="${HOME}/.config/zwift/config"

echo "=== Installing zwift-linux-ble ==="

mkdir -p "${BIN_DEST}" "${SHARE_DEST}" "${SERVICE_DEST}"

# 1. Install or compile binaries
if [[ -f "${SCRIPT_DIR}/bin/zwift-ble-daemon" && -f "${SCRIPT_DIR}/bin/BleWin10Lib.dll" ]]; then
    echo "[+] Using pre-built release artifacts from bin/..."
    cp "${SCRIPT_DIR}/bin/zwift-ble-daemon" "${BIN_DEST}/"
    cp "${SCRIPT_DIR}/bin/BleWin10Lib.dll" "${SHARE_DEST}/"
else
    echo "[+] Building zwift-ble-daemon from source..."
    (cd "${SCRIPT_DIR}/daemon" && cargo build --release)
    cp "${SCRIPT_DIR}/daemon/target/release/zwift-ble-daemon" "${BIN_DEST}/"

    echo "[+] Building BleWin10Lib.dll..."
    (cd "${SCRIPT_DIR}/dll" && ./build.sh)
    cp "${SCRIPT_DIR}/dll/BleWin10Lib.dll" "${SHARE_DEST}/"
fi
chmod +x "${BIN_DEST}/zwift-ble-daemon"

# 2. Install systemd user service
echo "[+] Configuring systemd user service..."
cp "${SCRIPT_DIR}/systemd/zwift-ble-daemon.service" "${SERVICE_DEST}/"
systemctl --user daemon-reload
systemctl --user enable --now zwift-ble-daemon.service

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

# Ensure CONTAINER_EXTRA_ARGS mounts BleWin10Lib.dll read-only
MOUNT_ARG="-v ${SHARE_DEST}/BleWin10Lib.dll:/home/user/.wine/drive_c/Program Files (x86)/Zwift/BleWin10Lib.dll:ro"
if grep -q 'CONTAINER_EXTRA_ARGS=' "${ZWIFT_CONFIG}"; then
    # Remove existing BleWin10Lib mounts to prevent duplicates
    sed -i '/CONTAINER_EXTRA_ARGS=/d' "${ZWIFT_CONFIG}"
fi
echo "CONTAINER_EXTRA_ARGS=(\"${MOUNT_ARG}\")" >> "${ZWIFT_CONFIG}"

echo "=== Installation Complete! ==="
echo "You can now launch Zwift directly from your desktop app menu or terminal ('zwift')."
echo "Check daemon status at any time with: systemctl --user status zwift-ble-daemon"
