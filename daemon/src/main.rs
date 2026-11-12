use anyhow::Result;
use bluer::gatt::remote::{Characteristic, CharacteristicWriteRequest};
use bluer::gatt::WriteOp;
use bluer::{Adapter, AdapterEvent, Address, Device, DiscoveryFilter, DiscoveryTransport, Session};
use futures::StreamExt;
use std::collections::{HashMap, HashSet};
use std::sync::Arc;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use tokio::sync::{mpsc, Mutex};
use tokio::time::{sleep, Duration};
use uuid::Uuid;

const CMD_START_SCAN: u8 = 0x01;
const CMD_STOP_SCAN: u8 = 0x02;
const CMD_PAIR_DEVICE: u8 = 0x03;
const CMD_UNPAIR_DEVICE: u8 = 0x04;
const CMD_PURGE_DEVICES: u8 = 0x05;
const CMD_WRITE_CHAR: u8 = 0x06;

const EVT_LOG: u8 = 0x81;
const EVT_ADVERTISEMENT: u8 = 0x82;
const EVT_DEVICE_UPDATE: u8 = 0x83;

const BASE_BLE_SUFFIX: &str = "-0000-1000-8000-00805f9b34fb";

fn addr_to_u64(addr: Address) -> u64 {
    let b = addr.0;
    ((b[0] as u64) << 40)
        | ((b[1] as u64) << 32)
        | ((b[2] as u64) << 24)
        | ((b[3] as u64) << 16)
        | ((b[4] as u64) << 8)
        | (b[5] as u64)
}

fn u64_to_addr(val: u64) -> Address {
    Address([
        ((val >> 40) & 0xFF) as u8,
        ((val >> 32) & 0xFF) as u8,
        ((val >> 24) & 0xFF) as u8,
        ((val >> 16) & 0xFF) as u8,
        ((val >> 8) & 0xFF) as u8,
        (val & 0xFF) as u8,
    ])
}

/// Normalize any UUID string (e.g. "{180D}", "0x180D", "180d", "0000180d-0000-1000-8000-00805f9b34fb")
/// to a canonical 128-bit lowercase UUID string ("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx").
fn normalize_uuid(raw: &str) -> Option<Uuid> {
    let s = raw
        .trim()
        .trim_matches(|c| c == '{' || c == '}')
        .trim_start_matches("0x")
        .trim_start_matches("0X")
        .to_lowercase();
    if let Ok(u) = Uuid::parse_str(&s) {
        return Some(u);
    }
    if s.len() <= 4 && s.chars().all(|c| c.is_ascii_hexdigit()) {
        let padded = format!("0000{:0>4}{}", s, BASE_BLE_SUFFIX);
        return Uuid::parse_str(&padded).ok();
    }
    if s.len() <= 8 && s.chars().all(|c| c.is_ascii_hexdigit()) {
        let padded = format!("{:0>8}{}", s, BASE_BLE_SUFFIX);
        return Uuid::parse_str(&padded).ok();
    }
    None
}

#[derive(Clone, Default)]
struct UuidMapper {
    /// Maps canonical 128-bit Uuid -> exact string passed by ZwiftApp.exe
    zwift_strings: HashMap<Uuid, String>,
    /// Set of service UUIDs requested by ZwiftApp.exe in BLEStartScanning
    filter_services: HashMap<Uuid, String>,
    /// Default formatting learned from ZwiftApp.exe (e.g. braces, uppercase, 16-bit vs 128-bit)
    use_braces: bool,
    use_uppercase: bool,
    use_short_16bit: bool,
}

impl UuidMapper {
    fn new_with_zwift_defaults() -> Self {
        let mut m = Self {
            zwift_strings: HashMap::new(),
            filter_services: HashMap::new(),
            use_braces: false,
            use_uppercase: true,
            use_short_16bit: true,
        };
        for (svc, chars) in [
            ("0x180D", &["2A37"][..]),
            ("0x180F", &["2A19"][..]),
            ("0x180A", &["2A23", "2A24", "2A25", "2A26", "2A27", "2A28", "2A29"][..]),
            (
                "0xFC82",
                &[
                    "00000002-19CA-4651-86E5-FA29DCDD09D1",
                    "00000003-19CA-4651-86E5-FA29DCDD09D1",
                    "00000004-19CA-4651-86E5-FA29DCDD09D1",
                    "00000006-19CA-4651-86E5-FA29DCDD09D1",
                ][..],
            ),
            (
                "00000001-19CA-4651-86E5-FA29DCDD09D1",
                &[
                    "00000002-19CA-4651-86E5-FA29DCDD09D1",
                    "00000003-19CA-4651-86E5-FA29DCDD09D1",
                    "00000004-19CA-4651-86E5-FA29DCDD09D1",
                    "00000006-19CA-4651-86E5-FA29DCDD09D1",
                ][..],
            ),
            ("0x1818", &["2A63", "A026E005-0A7D-4AB3-97FA-F1500F9FEB8B"][..]),
            ("0x1826", &["2ACC", "2AD2", "2AD9", "2ADA", "2ACD"][..]),
            ("0x1816", &["2A5B"][..]),
        ] {
            m.record_zwift_uuid(svc, true);
            for c in chars {
                m.record_zwift_uuid(c, false);
            }
        }
        m
    }

    fn record_zwift_uuid(&mut self, raw: &str, is_service: bool) {
        if raw.starts_with('{') {
            self.use_braces = true;
        }
        if raw.chars().any(|c| c.is_ascii_uppercase()) {
            self.use_uppercase = true;
        }
        let trimmed = raw.trim_matches(|c| c == '{' || c == '}');
        if trimmed.len() <= 6 {
            self.use_short_16bit = true;
        }
        if let Some(u) = normalize_uuid(raw) {
            self.zwift_strings.insert(u, raw.to_string());
            if is_service {
                self.filter_services.insert(u, raw.to_string());
            }
        }
    }

    fn format_uuid(&self, u: &Uuid) -> String {
        if let Some(exact) = self.zwift_strings.get(u) {
            return exact.clone();
        }
        let mut s = u.hyphenated().to_string();
        if self.use_short_16bit && s.starts_with("0000") && s.ends_with(BASE_BLE_SUFFIX) {
            s = s[4..8].to_string();
        }
        if self.use_uppercase {
            s = s.to_uppercase();
        }
        if self.use_braces {
            format!("{{{}}}", s)
        } else {
            s
        }
    }
}

/// Last Zwift (0x094A) manufacturer device type seen per address, e.g. 14 = Play Right, 15 = Play Left.
fn zwift_type_cache() -> &'static std::sync::Mutex<HashMap<u64, u8>> {
    static CACHE: std::sync::OnceLock<std::sync::Mutex<HashMap<u64, u8>>> = std::sync::OnceLock::new();
    CACHE.get_or_init(|| std::sync::Mutex::new(HashMap::new()))
}

struct SharedState {
    scanning: bool,
    mapper: UuidMapper,
    /// Active characteristics for connected devices: (addr_u64, canonical_char_uuid) -> (Characteristic, bool /* write_without_response */)
    char_map: HashMap<(u64, Uuid), (Characteristic, bool)>,
    /// Keep Device and Service handles alive while connected
    connected_devices: HashMap<u64, (Device, Vec<bluer::gatt::remote::Service>)>,
    /// Abort handles for active connected device tasks
    connected_tasks: HashMap<u64, Vec<tokio::task::AbortHandle>>,
    /// Devices currently executing handle_pair_device
    pairing_in_progress: HashSet<u64>,
}

fn build_frame(msg_type: u8, payload: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(5 + payload.len());
    out.push(msg_type);
    out.extend_from_slice(&(payload.len() as u32).to_le_bytes());
    out.extend_from_slice(payload);
    out
}

fn build_evt_log(msg: &str, is_err: bool) -> Vec<u8> {
    let bytes = msg.as_bytes();
    let len = bytes.len().min(65535) as u16;
    let mut p = Vec::with_capacity(3 + len as usize);
    p.push(if is_err { 1 } else { 0 });
    p.extend_from_slice(&len.to_le_bytes());
    p.extend_from_slice(&bytes[..len as usize]);
    build_frame(EVT_LOG, &p)
}

#[allow(clippy::too_many_arguments)]
fn build_evt_advertisement(
    addr_u64: u64,
    rssi: i16,
    name: &str,
    sections: &[(u8, Vec<u8>)],
    matched_services: &[String],
) -> Vec<u8> {
    let mut p = Vec::new();
    p.extend_from_slice(&addr_u64.to_le_bytes());
    p.extend_from_slice(&rssi.to_le_bytes());
    let name_bytes = name.as_bytes();
    let name_len = name_bytes.len().min(65535) as u16;
    p.extend_from_slice(&name_len.to_le_bytes());
    p.extend_from_slice(&name_bytes[..name_len as usize]);

    let num_secs = sections.len().min(65535) as u16;
    p.extend_from_slice(&num_secs.to_le_bytes());
    for (dtype, data) in sections.iter().take(num_secs as usize) {
        p.push(*dtype);
        let dlen = data.len().min(65535) as u16;
        p.extend_from_slice(&dlen.to_le_bytes());
        p.extend_from_slice(&data[..dlen as usize]);
    }

    let num_svcs = matched_services.len().min(65535) as u16;
    p.extend_from_slice(&num_svcs.to_le_bytes());
    for s in matched_services.iter().take(num_svcs as usize) {
        let sb = s.as_bytes();
        let slen = sb.len().min(65535) as u16;
        p.extend_from_slice(&slen.to_le_bytes());
        p.extend_from_slice(&sb[..slen as usize]);
    }
    build_frame(EVT_ADVERTISEMENT, &p)
}

fn build_evt_device_update(
    status: i32,
    addr_u64: u64,
    rssi: i16,
    name: &str,
    services: &[(String, Vec<(String, Vec<u8>)>)],
) -> Vec<u8> {
    let mut p = Vec::new();
    p.extend_from_slice(&status.to_le_bytes());
    p.extend_from_slice(&addr_u64.to_le_bytes());
    p.extend_from_slice(&rssi.to_le_bytes());
    let name_bytes = name.as_bytes();
    let name_len = name_bytes.len().min(65535) as u16;
    p.extend_from_slice(&name_len.to_le_bytes());
    p.extend_from_slice(&name_bytes[..name_len as usize]);

    let num_svcs = services.len().min(65535) as u16;
    p.extend_from_slice(&num_svcs.to_le_bytes());
    for (svc_uuid, chars) in services.iter().take(num_svcs as usize) {
        let sb = svc_uuid.as_bytes();
        let slen = sb.len().min(65535) as u16;
        p.extend_from_slice(&slen.to_le_bytes());
        p.extend_from_slice(&sb[..slen as usize]);

        let num_chars = chars.len().min(65535) as u16;
        p.extend_from_slice(&num_chars.to_le_bytes());
        for (char_uuid, data) in chars.iter().take(num_chars as usize) {
            let cb = char_uuid.as_bytes();
            let clen = cb.len().min(65535) as u16;
            p.extend_from_slice(&clen.to_le_bytes());
            p.extend_from_slice(&cb[..clen as usize]);

            let dlen = data.len() as u32;
            p.extend_from_slice(&dlen.to_le_bytes());
            p.extend_from_slice(data);
        }
    }
    build_frame(EVT_DEVICE_UPDATE, &p)
}

async fn emit_device_advertisement(
    adapter: &Adapter,
    addr: Address,
    state: &Arc<Mutex<SharedState>>,
    tx: &mpsc::UnboundedSender<Vec<u8>>,
) -> Result<()> {
    let addr_u64 = addr_to_u64(addr);
    {
        let st = state.lock().await;
        // CRITICAL: Never emit BLE advertisements for devices that are already paired/connected
        // or currently pairing in this session! On Windows 10, connected BLE peripherals stop advertising.
        // If ZwiftApp.exe receives an advertisement for an already-connected Zwift Play controller,
        // 0x140515100 resets its encryption counter and re-sends "RideOn", breaking the session!
        if st.connected_tasks.contains_key(&addr_u64)
            || st.connected_devices.contains_key(&addr_u64)
            || st.pairing_in_progress.contains(&addr_u64)
        {
            return Ok(());
        }
    }

    let dev = adapter.device(addr)?;
    let is_connected = dev.is_connected().await.unwrap_or(false);
    let rssi_opt = dev.rssi().await.ok().flatten();
    if rssi_opt.is_none() && !is_connected {
        return Ok(());
    }
    let rssi = rssi_opt.unwrap_or(-55);
    let name = dev
        .name()
        .await
        .ok()
        .flatten()
        .unwrap_or_default();

    let mfg_data = dev.manufacturer_data().await.ok().flatten().unwrap_or_default();
    let svc_data = dev.service_data().await.ok().flatten().unwrap_or_default();
    let mut uuids = dev.uuids().await.ok().flatten().unwrap_or_default();

    if name.starts_with("Zwift Play") {
        if let Ok(fc82) = Uuid::parse_str("0000fc82-0000-1000-8000-00805f9b34fb") {
            uuids.insert(fc82);
        }
    }

    if name.is_empty() && mfg_data.is_empty() && svc_data.is_empty() && uuids.is_empty() {
        return Ok(());
    }

    // 4. Match against ZwiftApp.exe's filterServices FIRST so we don't spam Zwift with unrelated devices
    let mut matched_services = Vec::new();
    {
        let st = state.lock().await;
        for u in &uuids {
            if let Some(z_svc) = st.mapper.filter_services.get(u) {
                if !matched_services.contains(z_svc) {
                    matched_services.push(z_svc.clone());
                }
            }
        }
        for u in svc_data.keys() {
            if let Some(z_svc) = st.mapper.filter_services.get(u) {
                if !matched_services.contains(z_svc) {
                    matched_services.push(z_svc.clone());
                }
            }
        }
    }

    // Skip devices that don't match any Zwift filter service and aren't Zwift/HRM/fitness devices
    if matched_services.is_empty() && !name.starts_with("Zwift") && !name.contains("HRM") && !name.contains("KICKR") {
        return Ok(());
    }

    let mut sections: Vec<(u8, Vec<u8>)> = Vec::new();

    // 1. Manufacturer Data (0xFF): 2-byte little-endian company ID + payload
    let mut has_zwift_mfg = false;
    for (company_id, payload) in &mfg_data {
        let mut sec = Vec::with_capacity(2 + payload.len());
        sec.extend_from_slice(&company_id.to_le_bytes());
        if *company_id == 0x094a && !payload.is_empty() {
            has_zwift_mfg = true;
            zwift_type_cache().lock().unwrap().insert(addr_u64, payload[0]);
            let mut modified = payload.clone();
            // Newer Zwift Play firmware advertises CP device type 14 (0x0E = Right) and 15 (0x0F = Left),
            // whereas ZwiftApp.exe's jump table at 0x140522dc1 maps 2 -> RC1-RIGHT (Component 21) and 3 -> RC1-LEFT (Component 20).
            // Set ZWIFT_PLAY_REWRITE=0 to pass the native device type through unchanged.
            let rewrite = std::env::var("ZWIFT_PLAY_REWRITE").map(|v| v != "0").unwrap_or(true);
            if rewrite && modified[0] == 14 {
                modified[0] = 2; // RC1-RIGHT ("-R")
            } else if rewrite && modified[0] == 15 {
                modified[0] = 3; // RC1-LEFT ("-L")
            }
            sec.extend_from_slice(&modified);
        } else {
            sec.extend_from_slice(payload);
        }
        sections.push((0xFF, sec));
    }

    // If a connected Zwift Play controller has no cached manufacturer_data in BlueZ, synthesize 0x094A
    // using the device type we last saw it advertise (Left/Right cannot be guessed from the address).
    if !has_zwift_mfg && name.starts_with("Zwift Play") {
        let cached = zwift_type_cache().lock().unwrap().get(&addr_u64).copied();
        let Some(raw_type) = cached else {
            // Wait for a real advertisement on a later scan pass.
            return Ok(());
        };
        let rewrite = std::env::var("ZWIFT_PLAY_REWRITE").map(|v| v != "0").unwrap_or(true);
        let cp_type = match raw_type {
            14 if rewrite => 2,
            15 if rewrite => 3,
            t => t,
        };
        let sec = vec![0x4a, 0x09, cp_type, addr.0[4], addr.0[5]];
        sections.push((0xFF, sec));
    }

    // 2. Service Data (0x16 for 16-bit UUID, 0x21 for 128-bit UUID)
    for (u, payload) in &svc_data {
        let s = u.hyphenated().to_string();
        if s.starts_with("0000") && s.ends_with(BASE_BLE_SUFFIX) {
            if let Ok(short16) = u16::from_str_radix(&s[4..8], 16) {
                let mut sec = Vec::with_capacity(2 + payload.len());
                sec.extend_from_slice(&short16.to_le_bytes());
                sec.extend_from_slice(payload);
                sections.push((0x16, sec));
            }
        } else {
            let mut uuid_le = u.as_bytes().to_vec();
            uuid_le.reverse();
            uuid_le.extend_from_slice(payload);
            sections.push((0x21, uuid_le));
        }
    }

    // 3. Advertised Service UUIDs (0x03 for 16-bit, 0x07 for 128-bit)
    let mut short_uuids = Vec::new();
    let mut long_uuids = Vec::new();
    for u in &uuids {
        let s = u.hyphenated().to_string();
        if s.starts_with("0000") && s.ends_with(BASE_BLE_SUFFIX) {
            if let Ok(short16) = u16::from_str_radix(&s[4..8], 16) {
                short_uuids.extend_from_slice(&short16.to_le_bytes());
            }
        } else {
            let mut b = u.as_bytes().to_vec();
            b.reverse();
            long_uuids.extend_from_slice(&b);
        }
    }
    if !short_uuids.is_empty() {
        sections.push((0x03, short_uuids));
    }
    if !long_uuids.is_empty() {
        sections.push((0x07, long_uuids));
    }

    let frame = build_evt_advertisement(addr_u64, rssi, &name, &sections, &matched_services);
    let _ = tx.send(frame);
    Ok(())
}

async fn handle_pair_device(
    adapter: Adapter,
    addr_u64: u64,
    state: Arc<Mutex<SharedState>>,
    connect_lock: Arc<Mutex<()>>,
    tx: mpsc::UnboundedSender<Vec<u8>>,
) {
    {
        let mut st = state.lock().await;
        if st.connected_tasks.contains_key(&addr_u64) {
            println!(
                "[Daemon] Device 0x{:012x} is already connected and streaming notifications, skipping duplicate pair",
                addr_u64
            );
            return;
        }
        if !st.pairing_in_progress.insert(addr_u64) {
            println!(
                "[Daemon] Pairing already in progress for 0x{:012x}, skipping duplicate request",
                addr_u64
            );
            return;
        }
    }
    handle_pair_device_inner(adapter, addr_u64, state.clone(), connect_lock, tx).await;
    {
        let mut st = state.lock().await;
        st.pairing_in_progress.remove(&addr_u64);
    }
}

async fn handle_pair_device_inner(
    adapter: Adapter,
    addr_u64: u64,
    state: Arc<Mutex<SharedState>>,
    connect_lock: Arc<Mutex<()>>,
    tx: mpsc::UnboundedSender<Vec<u8>>,
) {
    let addr = u64_to_addr(addr_u64);
    println!("[Daemon] Pairing/Connecting to device {} (0x{:012x})", addr, addr_u64);
    let _ = tx.send(build_evt_log(
        &format!("Connecting to BLE device {} (0x{:012x})", addr, addr_u64),
        false,
    ));

    let dev = match adapter.device(addr) {
        Ok(d) => d,
        Err(e) => {
            eprintln!("[Daemon] Failed to get device {}: {}", addr, e);
            return;
        }
    };

    let name = dev.name().await.ok().flatten().unwrap_or_default();
    let rssi = dev.rssi().await.ok().flatten().unwrap_or(-50);

    // Abort any previous notification tasks for this device
    {
        let mut st = state.lock().await;
        if let Some(handles) = st.connected_tasks.remove(&addr_u64) {
            for h in handles {
                h.abort();
            }
        }
        st.connected_devices.remove(&addr_u64);
    }

    // Serialize physical BLE connect attempts so BlueZ does not abort concurrent connections
    let mut connected = dev.is_connected().await.unwrap_or(false);
    let mut resolved = dev.is_services_resolved().await.unwrap_or(false);
    if !connected || !resolved {
        let _guard = connect_lock.lock().await;
        connected = dev.is_connected().await.unwrap_or(false);
        resolved = dev.is_services_resolved().await.unwrap_or(false);
        if !connected || !resolved {
            for attempt in 1..=4 {
                // Ensure BlueZ has an active LE advertisement for this Random-address BLE device.
                // When StopDiscovery runs, BlueZ purges temporary unbonded BLE devices or forgets their LE bearer,
                // causing Device1.Connect() to hang for 20s on BR/EDR or fail with le-connection-abort-by-local.
                if attempt > 1 || dev.rssi().await.ok().flatten().is_none() {
                    println!(
                        "[Daemon] Refreshing LE advertisement for {} before connect attempt {}...",
                        addr, attempt
                    );
                    let filter = DiscoveryFilter {
                        transport: DiscoveryTransport::Le,
                        duplicate_data: true,
                        ..Default::default()
                    };
                    let _ = adapter.set_discovery_filter(filter).await;
                    if let Ok(mut disc) = adapter.discover_devices().await {
                        let deadline = tokio::time::Instant::now() + Duration::from_millis(1500);
                        while tokio::time::Instant::now() < deadline {
                            if tokio::time::timeout(Duration::from_millis(200), disc.next()).await.is_ok() {
                                if dev.rssi().await.ok().flatten().is_some() {
                                    break;
                                }
                            }
                        }
                    }
                }

                // Wait 400ms after stopping discovery so the HCI controller closes its LE scan window cleanly
                sleep(Duration::from_millis(400)).await;

                let cur_rssi = dev.rssi().await.ok().flatten();
                println!(
                    "[Daemon] Connect attempt {} for {} ({}) RSSI={}...",
                    attempt,
                    addr,
                    name,
                    cur_rssi.map(|r| format!("{} dBm", r)).unwrap_or_else(|| "not seen".into())
                );
                let conn_res = tokio::time::timeout(Duration::from_secs(6), dev.connect()).await;
                match conn_res {
                    Ok(Ok(())) => {
                        connected = true;
                    }
                    Ok(Err(e)) => {
                        eprintln!("[Daemon] Connect attempt {} failed for {}: {}", attempt, addr, e);
                        connected = dev.is_connected().await.unwrap_or(false);
                    }
                    Err(_) => {
                        eprintln!("[Daemon] Connect attempt {} timed out after 6s for {}", attempt, addr);
                        let _ = dev.disconnect().await;
                        connected = false;
                    }
                }

                if connected {
                    // The link is up, so GATT discovery is in progress. Uncached first-time discovery on
                    // Zwift Play can take several seconds; keep waiting while the link stays alive and
                    // bail out immediately if it drops (never tear down a healthy connection ourselves).
                    for _ in 1..=150 {
                        if dev.is_services_resolved().await.unwrap_or(false) {
                            resolved = true;
                            break;
                        }
                        if !dev.is_connected().await.unwrap_or(false) {
                            connected = false;
                            break;
                        }
                        sleep(Duration::from_millis(100)).await;
                    }
                    if resolved {
                        break;
                    }
                    if connected {
                        eprintln!(
                            "[Daemon] Connected to {} but GATT services did not resolve within 15s; resetting connection...",
                            addr
                        );
                    } else {
                        eprintln!(
                            "[Daemon] Link to {} dropped during GATT discovery (radio/link-layer failure)",
                            addr
                        );
                    }
                    let _ = dev.disconnect().await;
                    connected = false;
                }

                sleep(Duration::from_millis(500)).await;
            }
        }
    }

    if !connected || !resolved {
        eprintln!("[Daemon] Could not connect and resolve GATT services for {}", addr);
        let _ = tx.send(build_evt_device_update(4, addr_u64, rssi, &name, &[]));
        return;
    }

    let services = match dev.services().await {
        Ok(s) => s,
        Err(e) => {
            eprintln!("[Daemon] Failed to enumerate services for {}: {}", addr, e);
            let _ = tx.send(build_evt_device_update(4, addr_u64, rssi, &name, &[]));
            return;
        }
    };

    let mut out_services: Vec<(String, Vec<(String, Vec<u8>)>)> = Vec::new();
    let mut notify_chars: Vec<(String, String, Uuid, Characteristic)> = Vec::new();
    let mut retained_services: Vec<bluer::gatt::remote::Service> = Vec::new();

    for svc in services {
        let svc_uuid = match svc.uuid().await {
            Ok(u) => u,
            Err(_) => continue,
        };
        let svc_hyphen = svc_uuid.hyphenated().to_string();
        // Skip Generic Access (1800) and Generic Attribute (1801) - Zwift does not use them,
        // and reading/subscribing to 2A05 causes ATT errors and 20s delays on Garmin HRM-Dual.
        if svc_hyphen.starts_with("00001800-") || svc_hyphen.starts_with("00001801-") {
            continue;
        }

        let svc_uuid_str = {
            let st = state.lock().await;
            st.mapper.format_uuid(&svc_uuid)
        };

        let chars = match svc.characteristics().await {
            Ok(c) => c,
            Err(_) => continue,
        };

        let mut out_chars: Vec<(String, Vec<u8>)> = Vec::new();
        for ch in chars {
            let ch_uuid = match ch.uuid().await {
                Ok(u) => u,
                Err(_) => continue,
            };
            let flags = ch.flags().await.ok();
            let can_read = flags.as_ref().map(|f| f.read).unwrap_or(false);
            let can_notify = flags
                .as_ref()
                .map(|f| f.notify || f.indicate)
                .unwrap_or(false);
            let write_no_resp = flags
                .as_ref()
                .map(|f| f.write_without_response && !f.write)
                .unwrap_or(false);

            let ch_uuid_str = {
                let mut st = state.lock().await;
                st.char_map.insert((addr_u64, ch_uuid), (ch.clone(), write_no_resp));
                st.mapper.format_uuid(&ch_uuid)
            };

            // Do NOT perform slow blocking ch.read() on every Device Information characteristic before status=3,
            // and NEVER read notify characteristics (like 00000004-... which holds stale handshake bytes).
            // Only read Battery Level (2A19) with a short timeout if readable.
            let mut init_val = Vec::new();
            let ch_hyphen = ch_uuid.hyphenated().to_string();
            if can_read && !can_notify && ch_hyphen.starts_with("00002a19-") {
                if let Ok(Ok(v)) = tokio::time::timeout(Duration::from_millis(400), ch.read()).await {
                    init_val = v;
                }
            }

            if can_notify {
                notify_chars.push((svc_uuid_str.clone(), ch_uuid_str.clone(), ch_uuid, ch.clone()));
            }

            println!(
                "[Daemon]   Device {} ({}) Svc {} Char {} (read={}, notify={}, write_no_resp={}, init_len={})",
                addr,
                name,
                svc_uuid_str,
                ch_uuid_str,
                can_read,
                can_notify,
                write_no_resp,
                init_val.len()
            );

            out_chars.push((ch_uuid_str, init_val));
        }

        out_services.push((svc_uuid_str, out_chars));
        retained_services.push(svc);
    }

    // Check if Zwift unpaired this device while we were connecting
    {
        let st = state.lock().await;
        if !st.pairing_in_progress.contains(&addr_u64) {
            println!("[Daemon] Pairing for {} was cancelled while connecting, aborting", addr);
            return;
        }
    }

    // Start notification streams BEFORE sending status=3 (Connected/Paired) to ZwiftApp.exe
    let mut abort_handles = Vec::new();
    for (svc_str, ch_str, ch_uuid, ch) in notify_chars {
        let mut notify_res = ch.notify().await;
        for _ in 0..3 {
            if notify_res.is_ok() {
                break;
            }
            sleep(Duration::from_millis(300)).await;
            notify_res = ch.notify().await;
        }
        match notify_res {
            Ok(stream) => {
                println!(
                    "[Daemon] Subscribed to notifications on {} ({}) char {} ({})",
                    addr, name, ch_str, ch_uuid
                );
                let tx_clone = tx.clone();
                let name_clone = name.clone();
                let jh = tokio::spawn(async move {
                    futures::pin_mut!(stream);
                    let mut count: u64 = 0;
                    let mut last: Vec<u8> = Vec::new();
                    while let Some(val) = stream.next().await {
                        count += 1;
                        let changed = val != last;
                        last = val.clone();
                        let is_hr = ch_str.to_ascii_uppercase().contains("2A37");
                        if count <= 15 || changed || count % 30 == 0 || (is_hr && count % 5 == 0) {
                            println!(
                                "[Daemon] Notify #{} from {} ({}) svc {} char {}: {:02x?}",
                                count, addr, name_clone, svc_str, ch_str, &val
                            );
                        }
                        let svc_payload = vec![(svc_str.clone(), vec![(ch_str.clone(), val)])];
                        let frame = build_evt_device_update(
                            2, // status = 2 (Characteristic Value Notification)
                            addr_u64,
                            rssi,
                            &name_clone,
                            &svc_payload,
                        );
                        if tx_clone.send(frame).is_err() {
                            break;
                        }
                    }
                    println!(
                        "[Daemon] Notification stream ended for {} ({}) char {}",
                        addr, name_clone, ch_str
                    );
                });
                abort_handles.push(jh.abort_handle());
            }
            Err(e) => {
                eprintln!(
                    "[Daemon] Failed to subscribe to notifications on {} char {}: {}",
                    addr, ch_str, e
                );
            }
        }
    }

    {
        let mut st = state.lock().await;
        st.connected_devices.insert(addr_u64, (dev, retained_services));
        st.connected_tasks.insert(addr_u64, abort_handles);
    }

    println!(
        "[Daemon] Device {} ({}) connected with {} services! Sending status=3 (Connected/Paired) to ZwiftApp.exe",
        addr,
        name,
        out_services.len()
    );
    let _ = tx.send(build_evt_device_update(3, addr_u64, rssi, &name, &out_services));
}

async fn handle_client(stream: TcpStream, adapter: Adapter) -> Result<()> {
    println!("[Daemon] Zwift BleWin10Lib.dll connected from {:?}", stream.peer_addr());
    let (mut reader, mut writer) = stream.into_split();
    let (tx, mut rx) = mpsc::unbounded_channel::<Vec<u8>>();
    let connect_lock = Arc::new(Mutex::new(()));

    // Sequential GATT write queue so encrypted Zwift Play packets (00, 01, 02...) are never reordered
    let (write_tx, mut write_rx) = mpsc::unbounded_channel::<(u64, String, Characteristic, bool, Vec<u8>)>();
    let write_jh = tokio::spawn(async move {
        while let Some((addr_u64, uuid_str, ch, write_no_resp, data)) = write_rx.recv().await {
            let res = if write_no_resp {
                let req = CharacteristicWriteRequest {
                    op_type: WriteOp::Command,
                    ..Default::default()
                };
                ch.write_ext(&data, &req).await
            } else {
                ch.write(&data).await
            };
            if let Err(e) = res {
                eprintln!(
                    "[Daemon] Write failed on 0x{:012x} char {}: {}",
                    addr_u64, uuid_str, e
                );
            }
        }
    });

    let state = Arc::new(Mutex::new(SharedState {
        scanning: false,
        mapper: UuidMapper::new_with_zwift_defaults(),
        char_map: HashMap::new(),
        connected_devices: HashMap::new(),
        connected_tasks: HashMap::new(),
        pairing_in_progress: HashSet::new(),
    }));

    // Writer task
    let writer_jh = tokio::spawn(async move {
        while let Some(frame) = rx.recv().await {
            if writer.write_all(&frame).await.is_err() {
                break;
            }
        }
    });

    // Periodic scanner & advertisement broadcaster task
    let adapter_scan = adapter.clone();
    let state_scan = state.clone();
    let tx_scan = tx.clone();
    let scan_jh = tokio::spawn(async move {
        let mut discovery_stream = None;
        loop {
            let (is_scanning, is_pairing) = {
                let st = state_scan.lock().await;
                (st.scanning, !st.pairing_in_progress.is_empty())
            };
            // Pause active BlueZ discovery while a connection handshake is in progress
            // to prevent BlueZ "le-connection-abort-by-local" errors!
            if is_scanning && !is_pairing {
                if discovery_stream.is_none() {
                    println!("[Daemon] Starting BlueZ LE adapter discovery...");
                    let filter = DiscoveryFilter {
                        transport: DiscoveryTransport::Le,
                        duplicate_data: true,
                        ..Default::default()
                    };
                    let _ = adapter_scan.set_discovery_filter(filter).await;
                    match adapter_scan.discover_devices_with_changes().await {
                        Ok(s) => discovery_stream = Some(Box::pin(s)),
                        Err(e) => eprintln!("[Daemon] discover_devices error: {}", e),
                    }
                }
                // Actively drain discovery events for up to 300ms so BlueZ updates LE advertisements
                if let Some(stream) = discovery_stream.as_mut() {
                    let deadline = tokio::time::Instant::now() + Duration::from_millis(300);
                    while tokio::time::Instant::now() < deadline {
                        match tokio::time::timeout(Duration::from_millis(50), stream.next()).await {
                            Ok(Some(AdapterEvent::DeviceAdded(addr))) => {
                                let _ = emit_device_advertisement(&adapter_scan, addr, &state_scan, &tx_scan).await;
                            }
                            Ok(Some(_)) => {}
                            _ => break,
                        }
                    }
                }
                if let Ok(addrs) = adapter_scan.device_addresses().await {
                    for addr in addrs {
                        let _ = emit_device_advertisement(&adapter_scan, addr, &state_scan, &tx_scan).await;
                    }
                }
                sleep(Duration::from_millis(250)).await;
            } else {
                if discovery_stream.is_some() {
                    println!("[Daemon] Stopping BlueZ adapter discovery.");
                    discovery_stream = None;
                }
                sleep(Duration::from_millis(100)).await;
            }
        }
    });

    let _ = tx.send(build_evt_log("zwift-ble-daemon ready on host BlueZ", false));

    // Command reader loop
    loop {
        let mut hdr = [0u8; 5];
        if reader.read_exact(&mut hdr).await.is_err() {
            break;
        }
        let cmd = hdr[0];
        let plen = u32::from_le_bytes([hdr[1], hdr[2], hdr[3], hdr[4]]) as usize;
        let mut payload = vec![0u8; plen];
        if plen > 0 && reader.read_exact(&mut payload).await.is_err() {
            break;
        }

        match cmd {
            CMD_START_SCAN => {
                let mut pos = 0usize;
                if payload.len() >= 4 {
                    let num_svcs = u32::from_le_bytes([
                        payload[0], payload[1], payload[2], payload[3],
                    ]) as usize;
                    pos += 4;
                    let mut st = state.lock().await;
                    for _ in 0..num_svcs {
                        if pos + 2 > payload.len() {
                            break;
                        }
                        let slen = u16::from_le_bytes([payload[pos], payload[pos + 1]]) as usize;
                        pos += 2;
                        if pos + slen > payload.len() {
                            break;
                        }
                        let s_str = String::from_utf8_lossy(&payload[pos..pos + slen]).to_string();
                        pos += slen;
                        st.mapper.record_zwift_uuid(&s_str, true);

                        if pos + 4 > payload.len() {
                            break;
                        }
                        let num_chars = u32::from_le_bytes([
                            payload[pos],
                            payload[pos + 1],
                            payload[pos + 2],
                            payload[pos + 3],
                        ]) as usize;
                        pos += 4;
                        let mut char_list = Vec::new();
                        for _ in 0..num_chars {
                            if pos + 2 > payload.len() {
                                break;
                            }
                            let clen =
                                u16::from_le_bytes([payload[pos], payload[pos + 1]]) as usize;
                            pos += 2;
                            if pos + clen > payload.len() {
                                break;
                            }
                            let c_str =
                                String::from_utf8_lossy(&payload[pos..pos + clen]).to_string();
                            pos += clen;
                            st.mapper.record_zwift_uuid(&c_str, false);
                            char_list.push(c_str);
                        }
                    }
                    st.scanning = true;
                    println!(
                        "[Daemon] CMD_START_SCAN: {} filter services (use_braces={}, use_uppercase={}, use_short_16bit={})",
                        num_svcs,
                        st.mapper.use_braces,
                        st.mapper.use_uppercase,
                        st.mapper.use_short_16bit
                    );
                }
            }
            CMD_STOP_SCAN => {
                println!("[Daemon] CMD_STOP_SCAN");
                state.lock().await.scanning = false;
            }
            CMD_PAIR_DEVICE => {
                if payload.len() >= 8 {
                    let addr_u64 = u64::from_le_bytes(payload[0..8].try_into().unwrap());
                    let adapter_c = adapter.clone();
                    let state_c = state.clone();
                    let lock_c = connect_lock.clone();
                    let tx_c = tx.clone();
                    tokio::spawn(async move {
                        handle_pair_device(adapter_c, addr_u64, state_c, lock_c, tx_c).await;
                    });
                }
            }
            CMD_UNPAIR_DEVICE => {
                if payload.len() >= 8 {
                    let addr_u64 = u64::from_le_bytes(payload[0..8].try_into().unwrap());
                    let addr = u64_to_addr(addr_u64);
                    // Zwift sends PAIR -> UNPAIR -> PAIR in quick succession during auto-connect.
                    // Ignore UNPAIR while a pairing is in flight so we don't tear down a half-open connection.
                    if state.lock().await.pairing_in_progress.contains(&addr_u64) {
                        println!(
                            "[Daemon] Ignoring CMD_UNPAIR_DEVICE for {} (pairing in progress)",
                            addr
                        );
                        continue;
                    }
                    let was_paired_or_pairing = {
                        let mut st = state.lock().await;
                        let p = st.pairing_in_progress.remove(&addr_u64);
                        let d = st.connected_devices.remove(&addr_u64).is_some();
                        let t = if let Some(handles) = st.connected_tasks.remove(&addr_u64) {
                            for h in handles {
                                h.abort();
                            }
                            true
                        } else {
                            false
                        };
                        p || d || t
                    };
                    if was_paired_or_pairing {
                        println!("[Daemon] CMD_UNPAIR_DEVICE: {} (0x{:012x})", addr, addr_u64);
                        if let Ok(dev) = adapter.device(addr) {
                            let name = dev.name().await.ok().flatten().unwrap_or_default();
                            let _ = dev.disconnect().await;
                            let _ = tx.send(build_evt_device_update(4, addr_u64, -60, &name, &[]));
                        }
                    }
                }
            }
            CMD_PURGE_DEVICES => {
                println!("[Daemon] CMD_PURGE_DEVICES");
            }
            CMD_WRITE_CHAR => {
                if payload.len() >= 14 {
                    let addr_u64 = u64::from_le_bytes(payload[0..8].try_into().unwrap());
                    let mut pos = 8usize;
                    let ulen = u16::from_le_bytes([payload[pos], payload[pos + 1]]) as usize;
                    pos += 2;
                    if pos + ulen + 4 <= payload.len() {
                        let uuid_str =
                            String::from_utf8_lossy(&payload[pos..pos + ulen]).to_string();
                        pos += ulen;
                        let dlen = u32::from_le_bytes([
                            payload[pos],
                            payload[pos + 1],
                            payload[pos + 2],
                            payload[pos + 3],
                        ]) as usize;
                        pos += 4;
                        if pos + dlen <= payload.len() {
                            let data = payload[pos..pos + dlen].to_vec();
                            if let Some(canon_uuid) = normalize_uuid(&uuid_str) {
                                let ch_opt = {
                                    let st = state.lock().await;
                                    st.char_map.get(&(addr_u64, canon_uuid)).cloned()
                                };
                                if let Some((ch, write_no_resp)) = ch_opt {
                                    println!(
                                        "[Daemon] Writing {} bytes to 0x{:012x} char {} ({:02x?})",
                                        data.len(),
                                        addr_u64,
                                        uuid_str,
                                        &data
                                    );
                                    let _ = write_tx.send((addr_u64, uuid_str, ch, write_no_resp, data));
                                } else {
                                    eprintln!(
                                        "[Daemon] CMD_WRITE_CHAR: char {} ({}) not found on 0x{:012x}",
                                        uuid_str, canon_uuid, addr_u64
                                    );
                                }
                            }
                        }
                    }
                }
            }
            _ => {
                eprintln!("[Daemon] Unknown command 0x{:02x}", cmd);
            }
        }
    }

    println!("[Daemon] Zwift BleWin10Lib.dll disconnected.");
    scan_jh.abort();
    write_jh.abort();
    writer_jh.abort();
    Ok(())
}

#[tokio::main]
async fn main() -> Result<()> {
    println!("============================================================");
    println!(" Zwift Native BLE Linux Daemon (BlueZ <-> BleWin10Lib.dll)");
    println!("============================================================");

    let session = Session::new().await?;
    let adapter = session.default_adapter().await?;
    adapter.set_powered(true).await?;
    println!(
        "[Daemon] Using BlueZ Adapter: {} ({:?})",
        adapter.name(),
        adapter.address().await?
    );

    let listener = TcpListener::bind("127.0.0.1:35199").await?;
    println!("[Daemon] Listening on 127.0.0.1:35199...");

    loop {
        let (stream, _) = listener.accept().await?;
        let adapter_clone = adapter.clone();
        tokio::spawn(async move {
            if let Err(e) = handle_client(stream, adapter_clone).await {
                eprintln!("[Daemon] Client error: {}", e);
            }
        });
    }
}
