#!/usr/bin/env bash
set -euo pipefail

BIN_DEST="${HOME}/.local/bin"
SHARE_DEST="${HOME}/.local/share/zwift-ble"
SERVICE_DEST="${HOME}/.config/systemd/user"
ZWIFT_CONFIG="${HOME}/.config/zwift/config"

echo "=== Uninstalling zwift-linux-ble ==="

echo "[+] Stopping and disabling systemd service..."
systemctl --user stop zwift-ble-daemon.service 2>/dev/null || true
systemctl --user disable zwift-ble-daemon.service 2>/dev/null || true
rm -f "${SERVICE_DEST}/zwift-ble-daemon.service"
systemctl --user daemon-reload

echo "[+] Removing binaries..."
rm -f "${BIN_DEST}/zwift-ble-daemon"
rm -rf "${SHARE_DEST}"

if [[ -f "${ZWIFT_CONFIG}" ]]; then
    echo "[+] Cleaning netbrain Zwift config..."
    sed -i '/CONTAINER_EXTRA_ARGS=/d' "${ZWIFT_CONFIG}"
fi

echo "=== Uninstalled Successfully ==="
