# zwift-linux-ble

Native Bluetooth Low Energy (BLE) peripheral support for **Zwift on Linux** via [`netbrain/zwift`](https://github.com/netbrain/zwift).

Allows devices like **Zwift Play controllers** and standard **Bluetooth Heart Rate Monitors (e.g. Garmin HRM-Dual)** to connect and stream telemetry directly through Zwift's native Windows 10 BLE interface (`BleWin10Lib.dll`) without requiring a companion smartphone bridge app or secondary ANT+ dongles.

---

## How It Works

1. **`zwift-ble-daemon` (Host Linux)**: A lightweight Rust daemon talking to the Linux host BlueZ Bluetooth stack via D-Bus (`bluer`). It proxies BLE scan, connect, GATT read/write, and notification events across a local TCP socket (`127.0.0.1:35199`).
2. **`BleWin10Lib.dll` (Wine / Container)**: A clean-room replacement Windows DLL that replicates Zwift's exact internal `BleWin10Lib.dll` ABI, forwarding commands and notifications directly to/from the host daemon.
3. **Seamless netbrain Integration**: Injected into the container via standard `CONTAINER_EXTRA_ARGS` volume mounts in `~/.config/zwift/config`. It persists across updates of `netbrain/zwift` automatically!

---

## Requirements

- Linux with **BlueZ** installed and running (`bluetoothd`)
- User access to Bluetooth (usually membership in `lp` or `bluetooth` group)
- Docker / Podman running `netbrain/zwift`

---

## Installation

### One-line Quick Install
```bash
git clone https://github.com/davidpfa/zwift-linux-ble.git
cd zwift-linux-ble
./scripts/install.sh
```

The script will:
- Build and install `zwift-ble-daemon` to `~/.local/bin/`
- Build and place `BleWin10Lib.dll` in `~/.local/share/zwift-ble/`
- Enable and start a systemd user service (`zwift-ble-daemon.service`) so it runs automatically in the background
- Configure `~/.config/zwift/config` to mount the bridge DLL into the container read-only

---

## Usage

Simply launch Zwift as you normally would (e.g., from your desktop application launcher or running `zwift` in the terminal)!

1. Turn on your Bluetooth devices (wake up your Zwift Play controllers / put on your HRM).
2. Open Zwift's **Paired Devices** screen.
3. Your BLE devices will appear and pair directly over Bluetooth!

---

## Troubleshooting & Status

Check the daemon logs in real-time:
```bash
journalctl --user -u zwift-ble-daemon.service -f
```

Restart the daemon:
```bash
systemctl --user restart zwift-ble-daemon.service
```

---

## Uninstall

```bash
./scripts/uninstall.sh
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.
