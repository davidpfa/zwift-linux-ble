# zwift-linux-ble

Native Bluetooth Low Energy (BLE) peripheral support for **Zwift on Linux** via [`netbrain/zwift`](https://github.com/netbrain/zwift).

Allows devices like **Zwift Play controllers**, **smart trainers (KICKR, Tacx, etc.)**, and standard **Bluetooth Heart Rate Monitors (Garmin HRM-Dual, Polar, Wahoo)** to connect and stream telemetry directly through Zwift's native Windows 10 BLE interface (`BleWin10Lib.dll`) without requiring a companion smartphone bridge app or secondary ANT+ dongles.

---

## Zero to Zwifting on Linux: Complete Guide

If you are setting up Zwift on Linux from scratch, follow these steps:

### Step 1: Install and Enable Docker / Podman

`netbrain/zwift` requires a container engine. If you don't already have Docker installed and running:

#### 1. Install & Start Docker
- **Arch / CachyOS / EndeavourOS**:
  ```bash
  sudo pacman -S --needed docker
  sudo systemctl enable --now docker
  ```
- **Ubuntu / Debian / Pop!_OS**:
  ```bash
  sudo apt update && sudo apt install -y docker.io
  sudo systemctl enable --now docker
  ```
- **Fedora**:
  ```bash
  sudo dnf install -y docker
  sudo systemctl enable --now docker
  ```

#### 2. Allow Your User to Run Docker Without Sudo
```bash
sudo usermod -aG docker $USER
newgrp docker
```
*(Verify by running `docker ps` — it should output an empty list of containers without any permission errors).*

#### 3. (Nvidia GPU Users Only) Install NVIDIA Container Toolkit
If your system has an Nvidia graphics card, Docker requires the NVIDIA container runtime for 3D acceleration:

- **Arch / CachyOS / EndeavourOS**:
  ```bash
  sudo pacman -S --needed nvidia-container-toolkit
  sudo nvidia-ctk runtime configure --runtime=docker
  sudo systemctl restart docker
  ```
- **Ubuntu / Debian**:
  ```bash
  sudo apt install -y nvidia-container-toolkit
  sudo nvidia-ctk runtime configure --runtime=docker
  sudo systemctl restart docker
  ```
- **Fedora**:
  ```bash
  sudo dnf install -y nvidia-container-toolkit
  sudo nvidia-ctk runtime configure --runtime=docker
  sudo systemctl restart docker
  ```

---

### Step 2: Install `netbrain/zwift` (The Zwift Container for Linux)

[`netbrain/zwift`](https://github.com/netbrain/zwift) runs Zwift under Wine containerized on modern Linux distributions.

1. Run netbrain's installer:
   ```bash
   bash -c "$(curl -fsSL https://raw.githubusercontent.com/netbrain/zwift/master/bin/install.sh)"
   ```
2. Run `zwift` once to verify that the game launcher loads and runs.

---

### Step 3: Install `zwift-linux-ble` (This Project)

By default, Wine cannot pass through Windows 10 Bluetooth LE GATT characteristics and notifications to Linux BlueZ. This project bridges that gap natively and automatically ensures your paired devices are remembered across sessions.

#### 1. Install Build Dependencies
Make sure your distribution has the required toolchains (Rust compiler, Clang/LLVM, and D-Bus headers):

- **Arch / CachyOS / EndeavourOS / Manjaro**:
  ```bash
  sudo pacman -S --needed base-devel rust clang lld llvm
  ```
- **Ubuntu / Debian / Pop!_OS**:
  ```bash
  sudo apt update && sudo apt install -y build-essential rustc cargo clang lld llvm libdbus-1-dev
  ```
- **Fedora / RHEL**:
  ```bash
  sudo dnf install -y rust cargo clang lld llvm dbus-devel
  ```

#### 2. Ensure Linux Bluetooth is Active
```bash
sudo systemctl enable --now bluetooth
```

#### 3. Clone and Run the Installer
```bash
git clone https://github.com/davidpfa/zwift-linux-ble.git
cd zwift-linux-ble
./scripts/install.sh
```

The installer will automatically:
- Check that all build tools are present
- Configure `~/.local/bin` in your shell PATH (supports `fish`, `bash`, and `zsh`)
- Compile and place the background proxy daemon (`zwift-ble-daemon`) into `~/.local/bin/`
- Compile and place the replacement Windows BLE driver (`BleWin10Lib.dll`) into `~/.local/share/zwift-ble/`
- Register and start a systemd user service (`zwift-ble-daemon.service`) so it runs seamlessly in the background on login
- Create `~/.local/share/zwift/Documents` on your host to persist your paired sensors (`knowndevices.xml`), preferences (`prefs.xml`), and activity `.fit` files
- Update your `~/.config/zwift/config` using netbrain's official `CONTAINER_EXTRA_ARGS` to mount both the driver and the persistent documents folder

---

### Step 4: Ride!

1. Wake up your Bluetooth devices (press buttons on your **Zwift Play** controllers / put on your **HRM** / power on your trainer).
2. Launch Zwift directly from your desktop app menu or run `zwift` in your terminal.
3. On the **Paired Devices** screen, pair your devices once.
4. On all future launches, Zwift will remember your devices and auto-connect to them immediately—just like on a pure Windows installation!

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

Check the daemon status:
```bash
systemctl --user status zwift-ble-daemon.service
```

Check real-time daemon logs to see connection and telemetry events:
```bash
journalctl --user -u zwift-ble-daemon.service -f
```

Restart the daemon:
```bash
systemctl --user restart zwift-ble-daemon.service
```

### Devices fail to connect (`le-connection-abort-by-local`, `RSSI=not seen`)

If the logs show repeated `Connect attempt N failed ... le-connection-abort-by-local`, `RSSI=not seen`, or `GATT services did not resolve`, the problem is almost always the Bluetooth radio link, not the daemon:

- **Don't plug the Bluetooth dongle into a USB 3 hub or next to a USB 3 port.** USB 3.0 emits 2.4 GHz interference that breaks BLE connections. Plug the dongle directly into a motherboard USB 2.0 port. If you need to bring it closer to the bike, use a short USB 2.0 extension cable. Low-bandwidth devices like an HRM may still work while Zwift Play controllers fail.
- **Keep the adapter in line of sight of the bike.** Handlebar controllers behind the PC case or trainer often fail at around −85 dBm or weaker. The daemon logs the signal strength (RSSI) on every connect attempt.
- **Charge the controllers.** Zwift Play controllers become unreliable when their battery is low.
- **Test outside Zwift** to rule out the daemon:
  ```bash
  systemctl --user stop zwift-ble-daemon.service
  bluetoothctl   # then: scan le → scan off → connect <MAC>
  systemctl --user start zwift-ble-daemon.service
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
