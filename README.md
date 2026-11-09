# zwift-linux-ble

Native Bluetooth Low Energy (BLE) peripheral support for **Zwift on Linux** via [`netbrain/zwift`](https://github.com/netbrain/zwift).

Allows devices like **Zwift Play controllers** and standard **Bluetooth Heart Rate Monitors (e.g. Garmin HRM-Dual)** to connect and stream telemetry directly through Zwift's native Windows 10 BLE interface (`BleWin10Lib.dll`) without requiring a companion smartphone bridge app or secondary ANT+ dongles.

---

## Zero to Zwifting on Linux: Complete Guide

If you are setting up Zwift on Linux from scratch, follow these steps:

### Step 1: Install `netbrain/zwift` (The Zwift Container for Linux)

[`netbrain/zwift`](https://github.com/netbrain/zwift) is the standard, battle-tested way to run Zwift under Wine containerized on modern Linux distributions (Arch, Ubuntu, Fedora, Debian, etc.).

1. Ensure **Docker** or **Podman** is installed and running on your system.
2. Install `netbrain/zwift` by following their official quickstart:
   ```bash
   bash -c "$(curl -fsSL https://raw.githubusercontent.com/netbrain/zwift/master/bin/install.sh)"
   ```
3. Run `zwift` once to let it download Zwift and verify that the game launcher loads and runs.

---

### Step 2: Install `zwift-linux-ble` (This Project)

By default, Wine cannot pass through Windows 10 Bluetooth LE GATT characteristics and notifications to Linux BlueZ. This project bridges that gap natively.

1. Ensure Linux Bluetooth (**BlueZ**) is active:
   ```bash
   systemctl status bluetooth
   ```
2. Clone this repository and run the installer:
   ```bash
   git clone https://github.com/davidpfa/zwift-linux-ble.git
   cd zwift-linux-ble
   ./scripts/install.sh
   ```

The installer will automatically:
- Build and place the background proxy daemon (`zwift-ble-daemon`) into `~/.local/bin/`
- Build and place the replacement Windows BLE driver (`BleWin10Lib.dll`) into `~/.local/share/zwift-ble/`
- Register and start a systemd user service (`zwift-ble-daemon.service`) so it runs seamlessly in the background on login
- Update your `~/.config/zwift/config` using netbrain's official `CONTAINER_EXTRA_ARGS` to mount the driver read-only into the container

---

### Step 3: Ride!

1. Wake up your Bluetooth devices (press buttons on your **Zwift Play** controllers / put on your **HRM**).
2. Launch Zwift directly from your desktop app menu or run `zwift` in your terminal.
3. On the **Paired Devices** screen, click **Controls** or **Heart Rate**.
4. Your devices will appear and pair directly over Bluetooth!

---

## How It Works Under the Hood

```
+-------------------------------------------------------------+
|                      Host Linux OS                          |
|                                                             |
|  [BlueZ Stack] <--- D-Bus ---> [zwift-ble-daemon (Rust)]    |
|                                         ^                   |
|                                         | TCP 127.0.0.1:35199
|                                         v                   |
|  +-------------------------------------------------------+  |
|  |             netbrain/zwift Container (Wine)           |  |
|  |                                                       |  |
|  |   [ZwiftApp.exe] <--- C ABI ---> [BleWin10Lib.dll]    |  |
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```

1. **`zwift-ble-daemon`**: Communicates with the host Linux BlueZ Bluetooth stack via D-Bus (`bluer`). It proxies BLE scan, connect, GATT read/write, and notification events across a local TCP socket (`127.0.0.1:35199`).
2. **`BleWin10Lib.dll`**: A clean-room replacement Windows DLL compiled with LLVM/Clang that replicates Zwift's exact internal `BleWin10Lib.dll` ABI, forwarding commands and notifications directly to/from the host daemon.
3. **Future-Proof Persistence**: Injected into the container via standard `CONTAINER_EXTRA_ARGS` volume mounts in `~/.config/zwift/config`. It persists automatically across `netbrain/zwift` script and container updates!

---

## Troubleshooting & Status

Check the daemon logs in real-time to see live connection and telemetry events:
```bash
journalctl --user -u zwift-ble-daemon.service -f
```

Restart the daemon:
```bash
systemctl --user restart zwift-ble-daemon.service
```

---

## Uninstall

To cleanly remove the daemon, systemd unit, and config entries:
```bash
./scripts/uninstall.sh
```

---

## Acknowledgments & Credits

- [netbrain/zwift](https://github.com/netbrain/zwift) for the Docker container and Linux runner setup.
- The Linux Bluetooth team and the authors of `bluer`.

---

## License

MIT License. See [LICENSE](LICENSE) for details.
