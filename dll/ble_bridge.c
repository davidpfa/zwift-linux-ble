#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * Minimal Win32 / Winsock2 Declarations (x86_64-pc-windows-msvc)
 * ============================================================================ */

typedef void* HANDLE;
typedef uint32_t DWORD;
typedef int32_t BOOL;
typedef uintptr_t SOCKET;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_SOCKET       ((SOCKET)(~0ULL))
#define SOCKET_ERROR         (-1)
#define AF_INET              2
#define SOCK_STREAM          1
#define IPPROTO_TCP          6
#define SOL_SOCKET           0xffff
#define IPPROTO_TCP_OPT      6
#define TCP_NODELAY          0x0001

#define GENERIC_WRITE        0x40000000UL
#define FILE_SHARE_READ      0x00000001UL
#define FILE_SHARE_WRITE     0x00000002UL
#define OPEN_ALWAYS          4
#define FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define FILE_APPEND_DATA     0x00000004UL

#define DLL_PROCESS_ATTACH   1
#define DLL_PROCESS_DETACH   0

typedef struct {
    void* DebugInfo;
    int32_t LockCount;
    int32_t RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    uintptr_t SpinCount;
} CRITICAL_SECTION;

typedef struct {
    int16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char sin_zero[8];
} sockaddr_in;

typedef struct {
    uint16_t wVersion;
    uint16_t wHighVersion;
    uint16_t iMaxSockets;
    uint16_t iMaxUdpDg;
    char* lpVendorInfo;
    char szDescription[257];
    char szSystemStatus[129];
} WSADATA;

__declspec(dllimport) void __stdcall InitializeCriticalSection(CRITICAL_SECTION* lpCriticalSection);
__declspec(dllimport) void __stdcall DeleteCriticalSection(CRITICAL_SECTION* lpCriticalSection);
__declspec(dllimport) void __stdcall EnterCriticalSection(CRITICAL_SECTION* lpCriticalSection);
__declspec(dllimport) void __stdcall LeaveCriticalSection(CRITICAL_SECTION* lpCriticalSection);
__declspec(dllimport) HANDLE __stdcall CreateThread(void* lpThreadAttributes, size_t dwStackSize, DWORD (__stdcall *lpStartAddress)(void*), void* lpParameter, DWORD dwCreationFlags, DWORD* lpThreadId);
__declspec(dllimport) DWORD __stdcall WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE hObject);
__declspec(dllimport) void __stdcall Sleep(DWORD dwMilliseconds);
__declspec(dllimport) HANDLE __stdcall GetProcessHeap(void);
__declspec(dllimport) void* __stdcall HeapAlloc(HANDLE hHeap, DWORD dwFlags, size_t dwBytes);
__declspec(dllimport) BOOL __stdcall HeapFree(HANDLE hHeap, DWORD dwFlags, void* lpMem);
__declspec(dllimport) HANDLE __stdcall CreateFileA(const char* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, void* lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, void* lpOverlapped);
__declspec(dllimport) BOOL __stdcall FlushFileBuffers(HANDLE hFile);
__declspec(dllimport) uint64_t __stdcall GetTickCount64(void);
__declspec(dllimport) BOOL __stdcall DisableThreadLibraryCalls(HANDLE hLibModule);

__declspec(dllimport) int __stdcall WSAStartup(uint16_t wVersionRequested, WSADATA* lpWSAData);
__declspec(dllimport) int __stdcall WSACleanup(void);
__declspec(dllimport) SOCKET __stdcall socket(int af, int type, int protocol);
__declspec(dllimport) int __stdcall connect(SOCKET s, const void* name, int namelen);
__declspec(dllimport) int __stdcall send(SOCKET s, const char* buf, int len, int flags);
__declspec(dllimport) int __stdcall recv(SOCKET s, char* buf, int len, int flags);
__declspec(dllimport) int __stdcall closesocket(SOCKET s);
__declspec(dllimport) uint16_t __stdcall htons(uint16_t hostshort);
__declspec(dllimport) uint32_t __stdcall inet_addr(const char* cp);
__declspec(dllimport) int __stdcall setsockopt(SOCKET s, int level, int optname, const char* optval, int optlen);

/* ============================================================================
 * Compiler Intrinsic / Freestanding Memory Helpers
 * ============================================================================ */

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* memset(void* dst, int c, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}

size_t my_strlen(const char* s) {
    if (!s) return 0;
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void* my_alloc(size_t sz) {
    return HeapAlloc(GetProcessHeap(), 0x00000008 /* HEAP_ZERO_MEMORY */, sz ? sz : 1);
}

static void my_free(void* p) {
    if (p) HeapFree(GetProcessHeap(), 0, p);
}

static void u64_to_dec(uint64_t val, char* out, size_t* out_len) {
    char tmp[32];
    int idx = 0;
    if (val == 0) {
        out[0] = '0';
        out[1] = '\0';
        if (out_len) *out_len = 1;
        return;
    }
    while (val > 0) {
        tmp[idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = 0; i < idx; i++) {
        out[i] = tmp[idx - 1 - i];
    }
    out[idx] = '\0';
    if (out_len) *out_len = (size_t)idx;
}

static void u64_to_hex(uint64_t val, char* out) {
    const char* hex = "0123456789abcdef";
    out[0] = '0';
    out[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        out[2 + (15 - i)] = hex[(val >> (i * 4)) & 0xF];
    }
    out[18] = '\0';
}

/* ============================================================================
 * Exact 64-bit MSVC C++ ABI Struct Layouts Used by ZwiftApp.exe
 * ============================================================================ */

#pragma pack(push, 8)

typedef struct {
    union {
        char buf[16];
        char* ptr;
    } u;
    uint64_t size;
    uint64_t cap;
} msvc_string; /* 32 bytes (0x20) */

typedef struct {
    uint8_t* begin;
    uint8_t* end;
    uint8_t* cap;
} msvc_vec_u8; /* 24 bytes (0x18) */

typedef struct {
    uint8_t dataType;
    uint8_t _pad[7];
    msvc_vec_u8 data;
} BLEAdvSection; /* 32 bytes (0x20) */

typedef struct {
    BLEAdvSection* begin;
    BLEAdvSection* end;
    BLEAdvSection* cap;
} msvc_vec_adv_section; /* 24 bytes (0x18) */

typedef struct {
    int16_t rssi;
    uint8_t _pad[6];
    msvc_string addressDecimal;
    msvc_string name;
    msvc_vec_adv_section sections;
} BLEAdvertisement; /* 96 bytes (0x60) */

typedef struct {
    msvc_string charUuid;
    const uint8_t* dataPtr;
    uint32_t dataLen;
    uint32_t _pad;
} BLECharacteristic; /* 48 bytes (0x30) */

typedef struct {
    BLECharacteristic* begin;
    BLECharacteristic* end;
    BLECharacteristic* cap;
} msvc_vec_char; /* 24 bytes (0x18) */

typedef struct {
    msvc_string serviceUuid;
    msvc_vec_char characteristics;
} BLEService; /* 56 bytes (0x38) */

typedef struct {
    BLEService* begin;
    BLEService* end;
    BLEService* cap;
} msvc_vec_service; /* 24 bytes (0x18) */

typedef struct {
    int32_t status;
    int32_t _pad0;
    msvc_string addressDecimal;
    msvc_string name;
    int16_t rssi;
    uint8_t _pad1[6];
    msvc_vec_service services;
} BLEDevice; /* 112 bytes (0x70) */

#pragma pack(pop)

static const char* msvc_string_cstr(const msvc_string* s, uint64_t* len_out) {
    if (!s) {
        if (len_out) *len_out = 0;
        return "";
    }
    if (len_out) *len_out = s->size;
    return (s->cap >= 16) ? s->u.ptr : s->u.buf;
}

static void msvc_string_init(msvc_string* s, const char* data, size_t len) {
    memset(s, 0, sizeof(*s));
    s->size = len;
    if (len <= 15) {
        s->cap = 15;
        if (len > 0 && data) memcpy(s->u.buf, data, len);
        s->u.buf[len] = '\0';
    } else {
        size_t alloc_cap = (len | 15);
        char* p = (char*)my_alloc(alloc_cap + 1);
        if (len > 0 && data) memcpy(p, data, len);
        p[len] = '\0';
        s->u.ptr = p;
        s->cap = alloc_cap;
    }
}

static void msvc_string_free(msvc_string* s) {
    if (s && s->cap >= 16 && s->u.ptr) {
        my_free(s->u.ptr);
        s->u.ptr = NULL;
    }
}

/* ============================================================================
 * Callback Pointers & State
 * ============================================================================ */

typedef void (*AssertFunc_t)(const char*, const char*);
typedef void (*BLEErrorFunc_t)(const BLEDevice*, int32_t);
typedef void (*BondStateFunc_t)(void*);
typedef void (*ConnectionStatusCBFunc_t)(const BLEDevice*);
typedef void (*EnableBLEIsAvailableFunc_t)(bool);
typedef void (*EnableSupportsBLEFunc_t)(bool);
typedef void (*LogFunc_t)(const char*, bool);
typedef void (*OnPairCB_t)(const BLEDevice*);
typedef void (*PeripheralDiscoveryFunc_t)(const BLEAdvertisement*);
typedef void (*ProcessBLEResponse_t)(const BLEDevice*);

static AssertFunc_t               g_AssertFunc = NULL;
static BLEErrorFunc_t             g_BLEErrorFunc = NULL;
static BondStateFunc_t            g_BondStateFunc = NULL;
static ConnectionStatusCBFunc_t   g_ConnectionStatusCBFunc = NULL;
static EnableBLEIsAvailableFunc_t g_EnableBLEIsAvailableFunc = NULL;
static EnableSupportsBLEFunc_t    g_EnableSupportsBLEFunc = NULL;
static LogFunc_t                  g_LogFunc = NULL;
static OnPairCB_t                 g_OnPairCB = NULL;
static PeripheralDiscoveryFunc_t  g_PeripheralDiscoveryFunc = NULL;
static ProcessBLEResponse_t       g_ProcessBLEResponse = NULL;

static bool             g_cs_initialized = false;
static CRITICAL_SECTION g_lock;
static CRITICAL_SECTION g_send_lock;
static HANDLE           g_log_file = INVALID_HANDLE_VALUE;
static HANDLE           g_worker_thread = NULL;
static volatile bool    g_running = false;
static volatile bool    g_initialized = false;
static volatile bool    g_scanning = false;
static volatile SOCKET  g_sock = INVALID_SOCKET;

/* Cached filter services passed to BLEStartScanning so we can re-send on reconnect */
static uint8_t* g_cached_scan_payload = NULL;
static uint32_t g_cached_scan_payload_len = 0;

static void ensure_init_locks(void) {
    if (!g_cs_initialized) {
        InitializeCriticalSection(&g_lock);
        InitializeCriticalSection(&g_send_lock);
        g_log_file = CreateFileA(
            "ble_bridge.log",
            FILE_APPEND_DATA | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        g_cs_initialized = true;
    }
}

static void bridge_log(const char* msg, bool is_err) {
    ensure_init_locks();
    if (g_log_file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        const char* prefix = is_err ? "[BLEBridge ERR] " : "[BLEBridge] ";
        WriteFile(g_log_file, prefix, (DWORD)my_strlen(prefix), &written, NULL);
        WriteFile(g_log_file, msg, (DWORD)my_strlen(msg), &written, NULL);
        WriteFile(g_log_file, "\r\n", 2, &written, NULL);
        FlushFileBuffers(g_log_file);
    }
    LogFunc_t cb = g_LogFunc;
    if (cb) {
        cb(msg, is_err);
    }
}

/* ============================================================================
 * Wire Protocol between BleWin10Lib.dll and Host zwift-ble-daemon (127.0.0.1:35199)
 * ============================================================================
 * Frame Header (5 bytes, little-endian):
 *   uint8_t  type
 *   uint32_t payload_len
 *
 * Commands (DLL -> Daemon):
 *   0x01 CMD_START_SCAN:
 *        uint32_t num_services
 *        For each service:
 *          uint16_t svc_uuid_len, char svc_uuid[svc_uuid_len]
 *          uint32_t num_chars
 *          For each char:
 *            uint16_t char_uuid_len, char char_uuid[char_uuid_len]
 *   0x02 CMD_STOP_SCAN: (empty)
 *   0x03 CMD_PAIR_DEVICE:
 *        uint64_t address
 *   0x04 CMD_UNPAIR_DEVICE:
 *        uint64_t address
 *   0x05 CMD_PURGE_DEVICES: (empty)
 *   0x06 CMD_WRITE_CHAR:
 *        uint64_t address
 *        uint16_t char_uuid_len, char char_uuid[char_uuid_len]
 *        uint32_t data_len, uint8_t data[data_len]
 *
 * Events (Daemon -> DLL):
 *   0x81 EVT_LOG:
 *        uint8_t is_error
 *        uint16_t msg_len, char msg[msg_len]
 *   0x82 EVT_ADVERTISEMENT:
 *        uint64_t address
 *        int16_t  rssi
 *        uint16_t name_len, char name[name_len]
 *        uint16_t num_sections
 *        For each section:
 *          uint8_t  data_type
 *          uint16_t data_len, uint8_t data[data_len]
 *        uint16_t num_matched_services
 *        For each matched service:
 *          uint16_t svc_uuid_len, char svc_uuid[svc_uuid_len]
 *   0x83 EVT_DEVICE_UPDATE:
 *        int32_t  status (1=Connecting, 2=CharValueNotification, 3=Connected/Paired, 4=Disconnected, 5=AdvServices)
 *        uint64_t address
 *        int16_t  rssi
 *        uint16_t name_len, char name[name_len]
 *        uint16_t num_services
 *        For each service:
 *          uint16_t svc_uuid_len, char svc_uuid[svc_uuid_len]
 *          uint16_t num_chars
 *          For each char:
 *            uint16_t char_uuid_len, char char_uuid[char_uuid_len]
 *            uint32_t data_len, uint8_t data[data_len]
 */

#define CMD_START_SCAN     0x01
#define CMD_STOP_SCAN      0x02
#define CMD_PAIR_DEVICE    0x03
#define CMD_UNPAIR_DEVICE  0x04
#define CMD_PURGE_DEVICES  0x05
#define CMD_WRITE_CHAR     0x06

#define EVT_LOG            0x81
#define EVT_ADVERTISEMENT  0x82
#define EVT_DEVICE_UPDATE  0x83

static bool send_all(SOCKET s, const uint8_t* buf, uint32_t len) {
    uint32_t total = 0;
    while (total < len) {
        int n = send(s, (const char*)(buf + total), (int)(len - total), 0);
        if (n <= 0) return false;
        total += (uint32_t)n;
    }
    return true;
}

static bool recv_all(SOCKET s, uint8_t* buf, uint32_t len) {
    uint32_t total = 0;
    while (total < len) {
        int n = recv(s, (char*)(buf + total), (int)(len - total), 0);
        if (n <= 0) return false;
        total += (uint32_t)n;
    }
    return true;
}

static void send_frame(uint8_t type, const uint8_t* payload, uint32_t len) {
    ensure_init_locks();
    EnterCriticalSection(&g_send_lock);
    SOCKET s = g_sock;
    if (s != INVALID_SOCKET) {
        uint8_t hdr[5];
        hdr[0] = type;
        hdr[1] = (uint8_t)(len & 0xFF);
        hdr[2] = (uint8_t)((len >> 8) & 0xFF);
        hdr[3] = (uint8_t)((len >> 16) & 0xFF);
        hdr[4] = (uint8_t)((len >> 24) & 0xFF);
        if (!send_all(s, hdr, 5) || (len > 0 && !send_all(s, payload, len))) {
            closesocket(s);
            g_sock = INVALID_SOCKET;
        }
    }
    LeaveCriticalSection(&g_send_lock);
}

/* Reader helpers */
typedef struct {
    const uint8_t* buf;
    uint32_t len;
    uint32_t pos;
} Reader;

static bool rd_u8(Reader* r, uint8_t* out) {
    if (r->pos + 1 > r->len) return false;
    *out = r->buf[r->pos++];
    return true;
}

static bool rd_u16(Reader* r, uint16_t* out) {
    if (r->pos + 2 > r->len) return false;
    *out = (uint16_t)r->buf[r->pos] | ((uint16_t)r->buf[r->pos + 1] << 8);
    r->pos += 2;
    return true;
}

static bool rd_i16(Reader* r, int16_t* out) {
    uint16_t v = 0;
    if (!rd_u16(r, &v)) return false;
    *out = (int16_t)v;
    return true;
}

static bool rd_u32(Reader* r, uint32_t* out) {
    if (r->pos + 4 > r->len) return false;
    *out = (uint32_t)r->buf[r->pos] |
           ((uint32_t)r->buf[r->pos + 1] << 8) |
           ((uint32_t)r->buf[r->pos + 2] << 16) |
           ((uint32_t)r->buf[r->pos + 3] << 24);
    r->pos += 4;
    return true;
}

static bool rd_i32(Reader* r, int32_t* out) {
    uint32_t v = 0;
    if (!rd_u32(r, &v)) return false;
    *out = (int32_t)v;
    return true;
}

static bool rd_u64(Reader* r, uint64_t* out) {
    uint32_t lo = 0, hi = 0;
    if (!rd_u32(r, &lo) || !rd_u32(r, &hi)) return false;
    *out = ((uint64_t)hi << 32) | lo;
    return true;
}

static bool rd_bytes(Reader* r, const uint8_t** out_ptr, uint32_t n) {
    if (r->pos + n > r->len) return false;
    *out_ptr = r->buf + r->pos;
    r->pos += n;
    return true;
}

static void handle_evt_log(Reader* r) {
    uint8_t is_err = 0;
    uint16_t msg_len = 0;
    const uint8_t* msg_bytes = NULL;
    if (!rd_u8(r, &is_err) || !rd_u16(r, &msg_len) || !rd_bytes(r, &msg_bytes, msg_len)) return;
    char* msg = (char*)my_alloc((size_t)msg_len + 1);
    if (msg_len > 0) memcpy(msg, msg_bytes, msg_len);
    msg[msg_len] = '\0';
    bridge_log(msg, is_err != 0);
    my_free(msg);
}

static void handle_evt_advertisement(Reader* r) {
    uint64_t address = 0;
    int16_t rssi = 0;
    uint16_t name_len = 0;
    const uint8_t* name_bytes = NULL;
    uint16_t num_sections = 0;

    if (!rd_u64(r, &address) || !rd_i16(r, &rssi) || !rd_u16(r, &name_len) || !rd_bytes(r, &name_bytes, name_len) || !rd_u16(r, &num_sections)) {
        return;
    }

    char addr_dec[32];
    size_t addr_dec_len = 0;
    u64_to_dec(address, addr_dec, &addr_dec_len);

    BLEAdvertisement adv;
    memset(&adv, 0, sizeof(adv));
    adv.rssi = rssi;
    msvc_string_init(&adv.addressDecimal, addr_dec, addr_dec_len);
    msvc_string_init(&adv.name, (const char*)name_bytes, name_len);

    BLEAdvSection* secs = NULL;
    if (num_sections > 0) {
        secs = (BLEAdvSection*)my_alloc(sizeof(BLEAdvSection) * num_sections);
        for (uint16_t i = 0; i < num_sections; i++) {
            uint8_t dtype = 0;
            uint16_t dlen = 0;
            const uint8_t* dbytes = NULL;
            if (!rd_u8(r, &dtype) || !rd_u16(r, &dlen) || !rd_bytes(r, &dbytes, dlen)) {
                num_sections = i;
                break;
            }
            secs[i].dataType = dtype;
            if (dlen > 0) {
                uint8_t* copy = (uint8_t*)my_alloc(dlen);
                memcpy(copy, dbytes, dlen);
                secs[i].data.begin = copy;
                secs[i].data.end = copy + dlen;
                secs[i].data.cap = copy + dlen;
            }
        }
        adv.sections.begin = secs;
        adv.sections.end = secs + num_sections;
        adv.sections.cap = secs + num_sections;
    }

    uint16_t num_matched_svcs = 0;
    rd_u16(r, &num_matched_svcs);

    BLEService* svcs = NULL;
    if (num_matched_svcs > 0) {
        svcs = (BLEService*)my_alloc(sizeof(BLEService) * num_matched_svcs);
        for (uint16_t i = 0; i < num_matched_svcs; i++) {
            uint16_t slen = 0;
            const uint8_t* sbytes = NULL;
            if (!rd_u16(r, &slen) || !rd_bytes(r, &sbytes, slen)) {
                num_matched_svcs = i;
                break;
            }
            msvc_string_init(&svcs[i].serviceUuid, (const char*)sbytes, slen);
        }
    }

    EnterCriticalSection(&g_lock);
    if (g_PeripheralDiscoveryFunc) {
        g_PeripheralDiscoveryFunc(&adv);
    }
    if (num_matched_svcs > 0 && name_len > 0 && g_ProcessBLEResponse) {
        BLEDevice dev;
        memset(&dev, 0, sizeof(dev));
        dev.status = 5; /* Service Advertisement discovered */
        msvc_string_init(&dev.addressDecimal, addr_dec, addr_dec_len);
        msvc_string_init(&dev.name, (const char*)name_bytes, name_len);
        dev.rssi = rssi;
        dev.services.begin = svcs;
        dev.services.end = svcs + num_matched_svcs;
        dev.services.cap = svcs + num_matched_svcs;
        g_ProcessBLEResponse(&dev);
        msvc_string_free(&dev.addressDecimal);
        msvc_string_free(&dev.name);
    }
    LeaveCriticalSection(&g_lock);

    for (uint16_t i = 0; i < num_matched_svcs; i++) {
        msvc_string_free(&svcs[i].serviceUuid);
    }
    my_free(svcs);

    for (uint16_t i = 0; i < num_sections; i++) {
        my_free(secs[i].data.begin);
    }
    my_free(secs);
    msvc_string_free(&adv.addressDecimal);
    msvc_string_free(&adv.name);
}

static void handle_evt_device_update(Reader* r) {
    int32_t status = 0;
    uint64_t address = 0;
    int16_t rssi = 0;
    uint16_t name_len = 0;
    const uint8_t* name_bytes = NULL;
    uint16_t num_services = 0;

    if (!rd_i32(r, &status) || !rd_u64(r, &address) || !rd_i16(r, &rssi) ||
        !rd_u16(r, &name_len) || !rd_bytes(r, &name_bytes, name_len) || !rd_u16(r, &num_services)) {
        return;
    }

    char addr_dec[32];
    size_t addr_dec_len = 0;
    u64_to_dec(address, addr_dec, &addr_dec_len);

    BLEDevice dev;
    memset(&dev, 0, sizeof(dev));
    dev.status = status;
    msvc_string_init(&dev.addressDecimal, addr_dec, addr_dec_len);
    msvc_string_init(&dev.name, (const char*)name_bytes, name_len);
    dev.rssi = rssi;

    BLEService* svcs = NULL;
    if (num_services > 0) {
        svcs = (BLEService*)my_alloc(sizeof(BLEService) * num_services);
        for (uint16_t i = 0; i < num_services; i++) {
            uint16_t slen = 0;
            const uint8_t* sbytes = NULL;
            uint16_t num_chars = 0;
            if (!rd_u16(r, &slen) || !rd_bytes(r, &sbytes, slen) || !rd_u16(r, &num_chars)) {
                num_services = i;
                break;
            }
            msvc_string_init(&svcs[i].serviceUuid, (const char*)sbytes, slen);
            if (num_chars > 0) {
                BLECharacteristic* chars = (BLECharacteristic*)my_alloc(sizeof(BLECharacteristic) * num_chars);
                for (uint16_t j = 0; j < num_chars; j++) {
                    uint16_t clen = 0;
                    const uint8_t* cbytes = NULL;
                    uint32_t dlen = 0;
                    const uint8_t* dbytes = NULL;
                    if (!rd_u16(r, &clen) || !rd_bytes(r, &cbytes, clen) || !rd_u32(r, &dlen) || !rd_bytes(r, &dbytes, dlen)) {
                        num_chars = j;
                        break;
                    }
                    msvc_string_init(&chars[j].charUuid, (const char*)cbytes, clen);
                    chars[j].dataPtr = (dlen > 0) ? dbytes : NULL;
                    chars[j].dataLen = dlen;
                }
                svcs[i].characteristics.begin = chars;
                svcs[i].characteristics.end = chars + num_chars;
                svcs[i].characteristics.cap = chars + num_chars;
            }
        }
        dev.services.begin = svcs;
        dev.services.end = svcs + num_services;
        dev.services.cap = svcs + num_services;
    }

    EnterCriticalSection(&g_lock);
    if (status == 3) {
        /* Connected / Paired: ZwiftApp.exe sets g_OnPairCB to a no-op stub (0x1401f5bf0)
           and uses g_ConnectionStatusCBFunc (0x14049ec30) to handle status == 3! */
        if (g_ConnectionStatusCBFunc) {
            g_ConnectionStatusCBFunc(&dev);
        }
        if (g_OnPairCB) {
            g_OnPairCB(&dev);
        }
    } else if (status == 2 || status == 5) {
        /* Characteristic value notification (2) or Service Advertisement (5) */
        if (g_ProcessBLEResponse) {
            g_ProcessBLEResponse(&dev);
        }
    } else if (status == 4 || status == 1) {
        /* Disconnected (4) or Connecting (1) */
        if (g_ConnectionStatusCBFunc) {
            g_ConnectionStatusCBFunc(&dev);
        }
    }
    LeaveCriticalSection(&g_lock);

    for (uint16_t i = 0; i < num_services; i++) {
        BLECharacteristic* c_begin = svcs[i].characteristics.begin;
        BLECharacteristic* c_end = svcs[i].characteristics.end;
        for (BLECharacteristic* c = c_begin; c < c_end; c++) {
            msvc_string_free(&c->charUuid);
        }
        my_free(c_begin);
        msvc_string_free(&svcs[i].serviceUuid);
    }
    my_free(svcs);
    msvc_string_free(&dev.addressDecimal);
    msvc_string_free(&dev.name);
}

static DWORD __stdcall worker_thread_proc(void* param) {
    (void)param;
    WSADATA wsa;
    WSAStartup(0x0202, &wsa);
    bridge_log("Worker thread started, connecting to 127.0.0.1:35199...", false);

    while (g_running) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            Sleep(1000);
            continue;
        }
        int one = 1;
        setsockopt(s, IPPROTO_TCP_OPT, TCP_NODELAY, (const char*)&one, sizeof(one));

        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(35199);
        addr.sin_addr = inet_addr("127.0.0.1");

        if (connect(s, &addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(s);
            Sleep(1000);
            continue;
        }

        EnterCriticalSection(&g_send_lock);
        g_sock = s;
        LeaveCriticalSection(&g_send_lock);

        bridge_log("Connected to host zwift-ble-daemon on 127.0.0.1:35199", false);

        /* Re-assert BLE availability */
        if (g_EnableSupportsBLEFunc) g_EnableSupportsBLEFunc(true);
        if (g_EnableBLEIsAvailableFunc) g_EnableBLEIsAvailableFunc(true);

        /* If scanning was already requested before socket connected, send cached scan command */
        EnterCriticalSection(&g_lock);
        if (g_scanning && g_cached_scan_payload) {
            send_frame(CMD_START_SCAN, g_cached_scan_payload, g_cached_scan_payload_len);
        }
        LeaveCriticalSection(&g_lock);

        /* Receive loop */
        while (g_running) {
            uint8_t hdr[5];
            if (!recv_all(s, hdr, 5)) {
                break;
            }
            uint8_t type = hdr[0];
            uint32_t plen = (uint32_t)hdr[1] |
                            ((uint32_t)hdr[2] << 8) |
                            ((uint32_t)hdr[3] << 16) |
                            ((uint32_t)hdr[4] << 24);
            if (plen > 16 * 1024 * 1024) {
                break;
            }
            uint8_t* payload = (plen > 0) ? (uint8_t*)my_alloc(plen) : NULL;
            if (plen > 0 && !recv_all(s, payload, plen)) {
                my_free(payload);
                break;
            }
            Reader r = { payload, plen, 0 };
            if (type == EVT_LOG) {
                handle_evt_log(&r);
            } else if (type == EVT_ADVERTISEMENT) {
                handle_evt_advertisement(&r);
            } else if (type == EVT_DEVICE_UPDATE) {
                handle_evt_device_update(&r);
            }
            my_free(payload);
        }

        EnterCriticalSection(&g_send_lock);
        if (g_sock == s) {
            closesocket(s);
            g_sock = INVALID_SOCKET;
        }
        LeaveCriticalSection(&g_send_lock);
        bridge_log("Disconnected from host zwift-ble-daemon, retrying in 1s...", true);
        Sleep(1000);
    }

    WSACleanup();
    return 0;
}

/* ============================================================================
 * 19 Exported Functions of BleWin10Lib.dll
 * ============================================================================ */

__declspec(dllexport) void __cdecl BLEInitBLEFlags(void) {
    ensure_init_locks();
    bridge_log("BLEInitBLEFlags() called", false);
    g_initialized = true;
    if (g_EnableSupportsBLEFunc) g_EnableSupportsBLEFunc(true);
    if (g_EnableBLEIsAvailableFunc) g_EnableBLEIsAvailableFunc(true);

    EnterCriticalSection(&g_lock);
    if (!g_running) {
        g_running = true;
        DWORD tid = 0;
        g_worker_thread = CreateThread(NULL, 0, worker_thread_proc, NULL, 0, &tid);
    }
    LeaveCriticalSection(&g_lock);
}

__declspec(dllexport) bool __cdecl BLEPairToDevice(uint64_t address) {
    char msg[64] = "BLEPairToDevice(";
    u64_to_hex(address, msg + 16);
    msg[34] = ')';
    msg[35] = '\0';
    bridge_log(msg, false);

    uint8_t payload[8];
    for (int i = 0; i < 8; i++) payload[i] = (uint8_t)((address >> (i * 8)) & 0xFF);
    send_frame(CMD_PAIR_DEVICE, payload, 8);
    return true;
}

__declspec(dllexport) void __cdecl BLEPurgeDeviceList(void) {
    bridge_log("BLEPurgeDeviceList() called", false);
    send_frame(CMD_PURGE_DEVICES, NULL, 0);
}

__declspec(dllexport) void __cdecl BLERequestBondState(const char* addressStr) {
    (void)addressStr;
    bridge_log("BLERequestBondState() called", false);
}

__declspec(dllexport) void __cdecl BLESetAssertFunc(AssertFunc_t cb) {
    ensure_init_locks();
    g_AssertFunc = cb;
}

__declspec(dllexport) void __cdecl BLESetBLEErrorFunc(BLEErrorFunc_t cb) {
    ensure_init_locks();
    g_BLEErrorFunc = cb;
}

__declspec(dllexport) void __cdecl BLESetBondStateFunc(BondStateFunc_t cb) {
    ensure_init_locks();
    g_BondStateFunc = cb;
}

__declspec(dllexport) void __cdecl BLESetConnectionStatusCBFunc(ConnectionStatusCBFunc_t cb) {
    ensure_init_locks();
    g_ConnectionStatusCBFunc = cb;
}

__declspec(dllexport) void __cdecl BLESetEnableBLEIsAvailableFunc(EnableBLEIsAvailableFunc_t cb) {
    ensure_init_locks();
    g_EnableBLEIsAvailableFunc = cb;
    if (cb) cb(true);
}

__declspec(dllexport) void __cdecl BLESetEnableSupportsBLEFunc(EnableSupportsBLEFunc_t cb) {
    ensure_init_locks();
    g_EnableSupportsBLEFunc = cb;
    if (cb) cb(true);
}

__declspec(dllexport) void __cdecl BLESetLogFunc(LogFunc_t cb) {
    ensure_init_locks();
    g_LogFunc = cb;
    bridge_log("BLESetLogFunc() registered", false);
}

__declspec(dllexport) void __cdecl BLESetOnPairCB(OnPairCB_t cb) {
    ensure_init_locks();
    g_OnPairCB = cb;
}

__declspec(dllexport) void __cdecl BLESetPeripheralDiscoveryFunc(PeripheralDiscoveryFunc_t cb) {
    ensure_init_locks();
    g_PeripheralDiscoveryFunc = cb;
}

__declspec(dllexport) void __cdecl BLESetProcessBLEResponse(ProcessBLEResponse_t cb) {
    ensure_init_locks();
    g_ProcessBLEResponse = cb;
}

__declspec(dllexport) void __cdecl BLEShutdown(void) {
    bridge_log("BLEShutdown() called", false);
    g_running = false;
    g_scanning = false;
    EnterCriticalSection(&g_send_lock);
    if (g_sock != INVALID_SOCKET) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }
    LeaveCriticalSection(&g_send_lock);
    if (g_worker_thread) {
        WaitForSingleObject(g_worker_thread, 2000);
        CloseHandle(g_worker_thread);
        g_worker_thread = NULL;
    }
}

__declspec(dllexport) void __cdecl BLEStartScanning(const msvc_vec_service* filterServices) {
    ensure_init_locks();
    bridge_log("BLEStartScanning() called", false);
    if (g_EnableSupportsBLEFunc) g_EnableSupportsBLEFunc(true);
    if (g_EnableBLEIsAvailableFunc) g_EnableBLEIsAvailableFunc(true);

    /* Ensure worker thread is running even if BLEInitBLEFlags wasn't called first */
    if (!g_running) {
        BLEInitBLEFlags();
    }

    uint32_t num_svcs = 0;
    uint32_t total_size = 4; /* num_services */

    if (filterServices && filterServices->begin && filterServices->end >= filterServices->begin) {
        num_svcs = (uint32_t)(filterServices->end - filterServices->begin);
        for (uint32_t i = 0; i < num_svcs; i++) {
            const BLEService* svc = &filterServices->begin[i];
            uint64_t slen = 0;
            msvc_string_cstr(&svc->serviceUuid, &slen);
            total_size += 2 + (uint32_t)slen + 4;
            uint32_t num_chars = 0;
            if (svc->characteristics.begin && svc->characteristics.end >= svc->characteristics.begin) {
                num_chars = (uint32_t)(svc->characteristics.end - svc->characteristics.begin);
                for (uint32_t j = 0; j < num_chars; j++) {
                    uint64_t clen = 0;
                    msvc_string_cstr(&svc->characteristics.begin[j].charUuid, &clen);
                    total_size += 2 + (uint32_t)clen;
                }
            }
        }
    }

    uint8_t* buf = (uint8_t*)my_alloc(total_size);
    uint32_t pos = 0;
    buf[pos++] = (uint8_t)(num_svcs & 0xFF);
    buf[pos++] = (uint8_t)((num_svcs >> 8) & 0xFF);
    buf[pos++] = (uint8_t)((num_svcs >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((num_svcs >> 24) & 0xFF);

    for (uint32_t i = 0; i < num_svcs; i++) {
        const BLEService* svc = &filterServices->begin[i];
        uint64_t slen = 0;
        const char* sstr = msvc_string_cstr(&svc->serviceUuid, &slen);
        bridge_log(sstr, false);
        uint16_t u16_slen = (uint16_t)slen;
        buf[pos++] = (uint8_t)(u16_slen & 0xFF);
        buf[pos++] = (uint8_t)((u16_slen >> 8) & 0xFF);
        if (u16_slen > 0) {
            memcpy(buf + pos, sstr, u16_slen);
            pos += u16_slen;
        }
        uint32_t num_chars = 0;
        if (svc->characteristics.begin && svc->characteristics.end >= svc->characteristics.begin) {
            num_chars = (uint32_t)(svc->characteristics.end - svc->characteristics.begin);
        }
        buf[pos++] = (uint8_t)(num_chars & 0xFF);
        buf[pos++] = (uint8_t)((num_chars >> 8) & 0xFF);
        buf[pos++] = (uint8_t)((num_chars >> 16) & 0xFF);
        buf[pos++] = (uint8_t)((num_chars >> 24) & 0xFF);

        for (uint32_t j = 0; j < num_chars; j++) {
            uint64_t clen = 0;
            const char* cstr = msvc_string_cstr(&svc->characteristics.begin[j].charUuid, &clen);
            uint16_t u16_clen = (uint16_t)clen;
            buf[pos++] = (uint8_t)(u16_clen & 0xFF);
            buf[pos++] = (uint8_t)((u16_clen >> 8) & 0xFF);
            if (u16_clen > 0) {
                memcpy(buf + pos, cstr, u16_clen);
                pos += u16_clen;
            }
        }
    }

    EnterCriticalSection(&g_lock);
    if (g_cached_scan_payload) my_free(g_cached_scan_payload);
    g_cached_scan_payload = buf;
    g_cached_scan_payload_len = pos;
    g_scanning = true;
    LeaveCriticalSection(&g_lock);

    send_frame(CMD_START_SCAN, buf, pos);
}

__declspec(dllexport) void __cdecl BLEStopScanning(void) {
    bridge_log("BLEStopScanning() called", false);
    g_scanning = false;
    send_frame(CMD_STOP_SCAN, NULL, 0);
}

__declspec(dllexport) bool __cdecl BLEUnpairFromDevice(uint64_t address) {
    char msg[64] = "BLEUnpairFromDevice(";
    u64_to_hex(address, msg + 20);
    msg[38] = ')';
    msg[39] = '\0';
    bridge_log(msg, false);

    uint8_t payload[8];
    for (int i = 0; i < 8; i++) payload[i] = (uint8_t)((address >> (i * 8)) & 0xFF);
    send_frame(CMD_UNPAIR_DEVICE, payload, 8);
    return true;
}

__declspec(dllexport) bool __cdecl BLEWriteToDevice(uint64_t address, const msvc_string* charUuid, const msvc_string* data) {
    uint64_t uuid_len = 0;
    const char* uuid_str = msvc_string_cstr(charUuid, &uuid_len);
    uint64_t data_len = 0;
    const char* data_bytes = msvc_string_cstr(data, &data_len);

    uint32_t total = 8 + 2 + (uint32_t)uuid_len + 4 + (uint32_t)data_len;
    uint8_t* buf = (uint8_t*)my_alloc(total);
    uint32_t pos = 0;
    for (int i = 0; i < 8; i++) buf[pos++] = (uint8_t)((address >> (i * 8)) & 0xFF);
    uint16_t u16_ulen = (uint16_t)uuid_len;
    buf[pos++] = (uint8_t)(u16_ulen & 0xFF);
    buf[pos++] = (uint8_t)((u16_ulen >> 8) & 0xFF);
    if (u16_ulen > 0) {
        memcpy(buf + pos, uuid_str, u16_ulen);
        pos += u16_ulen;
    }
    uint32_t u32_dlen = (uint32_t)data_len;
    buf[pos++] = (uint8_t)(u32_dlen & 0xFF);
    buf[pos++] = (uint8_t)((u32_dlen >> 8) & 0xFF);
    buf[pos++] = (uint8_t)((u32_dlen >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((u32_dlen >> 24) & 0xFF);
    if (u32_dlen > 0) {
        memcpy(buf + pos, data_bytes, u32_dlen);
        pos += u32_dlen;
    }

    send_frame(CMD_WRITE_CHAR, buf, pos);
    my_free(buf);
    return true;
}

BOOL __stdcall _DllMainCRTStartup(HANDLE hModule, DWORD ul_reason_for_call, void* lpReserved) {
    (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        ensure_init_locks();
        bridge_log("Custom BleWin10Lib.dll attached to ZwiftApp.exe!", false);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        BLEShutdown();
    }
    return 1;
}
