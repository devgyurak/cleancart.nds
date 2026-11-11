// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Headless runner on the melonDS 1.1 core (the same core as melonDS.app):
// software renderer, FreeBIOS, generated firmware, no JIT. It loads a ROM the
// way the Qt frontend does (save from a .sav file, direct boot), feeds
// scripted input and writes screenshots, so QA can look at melonDS output
// without screen recording.
//
//   mdsrun ROM FRAMES OUTPREFIX [EVENTS] [SHOT_FRAMES]
//
// EVENTS: comma list of FRAME:KEY[:HOLD]; KEY = A B X Y L R START SELECT UP
// DOWN LEFT RIGHT, LID (closed while held) or Tx/y (touch). HOLD defaults to 4.
// "@path" reads the list from a file (commas or newlines).
// SHOT_FRAMES: comma list; writes OUTPREFIX_NNNNN.png, OUTPREFIX_final.png always.
//
// Environment:
//   SAV=path     load the cart save from path (if it exists) and write it back at the end
//   POKE=A:V,..  write u32 V (decimal) to hex address A every 30 frames
//   SHOT_EVERY=N also write a screenshot every N frames
//   TRACE=A:N,.. print N bytes at hex address A every TRACE_EVERY (default 60) frames,
//                after the power management register 0 (pm=, backlight bits 2/3)
//   PEEK=A:N,..  print N bytes at hex address A after the run
//   WAV=path     write the sound output (48000 Hz stereo s16) to path
//   RTC=YYYY-MM-DD-HH-MM-SS   start time of the RTC (default: host UTC)
//   PM0=hex      power management register 0 at boot. Direct boot leaves it 0;
//                0d is what the firmware leaves (backlights and amp on)
//
// Prints "RESULT w0..w15" (demo result block at 0x0237F000), "SAVE type len"
// (melonDS save memory type and size), "PC9 x PC7 y" and the ARM9 registers
// ("CPU9", "REG9") for crash triage. Nothing in here is built into a ROM.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "NDS.h"
#include "NDSCart.h"
#include "Platform.h"
#include "GPU.h"
#include "SPU.h"
#include "SPI.h"

using namespace melonDS;

static std::string g_savpath;
static std::vector<u8> g_save;

// ---------------------------------------------------------------- platform

namespace melonDS::Platform
{

void SignalStop(StopReason, void*) {}

static bool exists(const std::string& p)
{
    FILE* f = fopen(p.c_str(), "rb");
    if (f) fclose(f);
    return f != nullptr;
}

FileHandle* OpenFile(const std::string& path, FileMode mode)
{
    bool ex = exists(path);
    if ((mode & FileMode::NoCreate) && !ex) return nullptr;
    std::string m;
    if (mode & FileMode::Append) m = "a";
    else if (!(mode & FileMode::Write)) m = "r";
    else if ((mode & FileMode::Preserve) && ex) m = "r+";
    else m = "w";
    if ((mode & FileMode::Read) && (mode & FileMode::Write) && m != "r+") m += "+";
    if (!(mode & FileMode::Text)) m += "b";
    return reinterpret_cast<FileHandle*>(fopen(path.c_str(), m.c_str()));
}

std::string GetLocalFilePath(const std::string& f) { return f; }
FileHandle* OpenLocalFile(const std::string& p, FileMode m) { return OpenFile(p, m); }
bool FileExists(const std::string& n) { return exists(n); }
bool LocalFileExists(const std::string& n) { return exists(n); }
bool CheckFileWritable(const std::string&) { return true; }
bool CheckLocalFileWritable(const std::string&) { return true; }
bool CloseFile(FileHandle* f) { return fclose(reinterpret_cast<FILE*>(f)) == 0; }
bool IsEndOfFile(FileHandle* f) { return feof(reinterpret_cast<FILE*>(f)) != 0; }
bool FileReadLine(char* s, int n, FileHandle* f) { return fgets(s, n, reinterpret_cast<FILE*>(f)) != nullptr; }
u64 FilePosition(FileHandle* f) { return ftell(reinterpret_cast<FILE*>(f)); }

bool FileSeek(FileHandle* f, s64 off, FileSeekOrigin o)
{
    int w = o == FileSeekOrigin::Start ? SEEK_SET : o == FileSeekOrigin::Current ? SEEK_CUR : SEEK_END;
    return fseek(reinterpret_cast<FILE*>(f), off, w) == 0;
}

void FileRewind(FileHandle* f) { rewind(reinterpret_cast<FILE*>(f)); }
u64 FileRead(void* d, u64 s, u64 c, FileHandle* f) { return fread(d, s, c, reinterpret_cast<FILE*>(f)); }
bool FileFlush(FileHandle* f) { return fflush(reinterpret_cast<FILE*>(f)) == 0; }
u64 FileWrite(const void* d, u64 s, u64 c, FileHandle* f) { return fwrite(d, s, c, reinterpret_cast<FILE*>(f)); }

u64 FileWriteFormatted(FileHandle* f, const char* fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    int r = vfprintf(reinterpret_cast<FILE*>(f), fmt, a);
    va_end(a);
    return r < 0 ? 0 : r;
}

u64 FileLength(FileHandle* f)
{
    FILE* fp = reinterpret_cast<FILE*>(f);
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, pos, SEEK_SET);
    return len;
}

void Log(LogLevel level, const char* fmt, ...)
{
    if (level < LogLevel::Warn && !getenv("MDS_DEBUG")) return;
    va_list a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    va_end(a);
}

struct Thread { std::thread t; };
Thread* Thread_Create(std::function<void()> fn) { return new Thread{std::thread(fn)}; }
void Thread_Free(Thread* t) { if (t->t.joinable()) t->t.detach(); delete t; }
void Thread_Wait(Thread* t) { if (t->t.joinable()) t->t.join(); }

struct Semaphore { std::mutex m; std::condition_variable cv; int n = 0; };
Semaphore* Semaphore_Create() { return new Semaphore; }
void Semaphore_Free(Semaphore* s) { delete s; }
void Semaphore_Reset(Semaphore* s) { std::lock_guard<std::mutex> l(s->m); s->n = 0; }

void Semaphore_Wait(Semaphore* s)
{
    std::unique_lock<std::mutex> l(s->m);
    s->cv.wait(l, [s] { return s->n > 0; });
    s->n--;
}

bool Semaphore_TryWait(Semaphore* s, int ms)
{
    std::unique_lock<std::mutex> l(s->m);
    if (!s->cv.wait_for(l, std::chrono::milliseconds(ms), [s] { return s->n > 0; })) return false;
    s->n--;
    return true;
}

void Semaphore_Post(Semaphore* s, int c)
{
    { std::lock_guard<std::mutex> l(s->m); s->n += c; }
    s->cv.notify_all();
}

struct Mutex { std::mutex m; };
Mutex* Mutex_Create() { return new Mutex; }
void Mutex_Free(Mutex* m) { delete m; }
void Mutex_Lock(Mutex* m) { m->m.lock(); }
void Mutex_Unlock(Mutex* m) { m->m.unlock(); }
bool Mutex_TryLock(Mutex* m) { return m->m.try_lock(); }

void Sleep(u64 us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }

u64 GetMSCount()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

u64 GetUSCount()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

void WriteNDSSave(const u8* data, u32 len, u32, u32, void*)
{
    g_save.assign(data, data + len);
}

void WriteGBASave(const u8*, u32, u32, u32, void*) {}
void WriteFirmware(const Firmware&, u32, u32, void*) {}
void WriteDateTime(int, int, int, int, int, int, void*) {}

void MP_Begin(void*) {}
void MP_End(void*) {}
int MP_SendPacket(u8*, int, u64, void*) { return 0; }
int MP_RecvPacket(u8*, u64*, void*) { return 0; }
int MP_SendCmd(u8*, int, u64, void*) { return 0; }
int MP_SendReply(u8*, int, u64, u16, void*) { return 0; }
int MP_SendAck(u8*, int, u64, void*) { return 0; }
int MP_RecvHostPacket(u8*, u64*, void*) { return 0; }
u16 MP_RecvReplies(u8*, u64, u16, void*) { return 0; }
int Net_SendPacket(u8*, int, void*) { return 0; }
int Net_RecvPacket(u8*, void*) { return 0; }

void Camera_Start(int, void*) {}
void Camera_Stop(int, void*) {}
void Camera_CaptureFrame(int, u32*, int, int, bool, void*) {}
void Mic_Start(void*) {}
void Mic_Stop(void*) {}
int Mic_ReadInput(s16*, int, void*) { return 0; }

struct AACDecoder {};
AACDecoder* AAC_Init() { return nullptr; }
void AAC_DeInit(AACDecoder*) {}
bool AAC_Configure(AACDecoder*, int, int) { return false; }
bool AAC_DecodeFrame(AACDecoder*, const void*, int, void*, int) { return false; }

bool Addon_KeyDown(KeyType, void*) { return false; }
void Addon_RumbleStart(u32, void*) {}
void Addon_RumbleStop(void*) {}
float Addon_MotionQuery(MotionQueryType, void*) { return 0; }

DynamicLibrary* DynamicLibrary_Load(const char*) { return nullptr; }
void DynamicLibrary_Unload(DynamicLibrary*) {}
void* DynamicLibrary_LoadFunction(DynamicLibrary*, const char*) { return nullptr; }

}

// ---------------------------------------------------------------- output

static u32 crc_table[256];

static u32 crc32(u32 c, const u8* p, size_t n)
{
    if (!crc_table[1])
        for (u32 i = 0; i < 256; i++) {
            u32 v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320 ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    c = ~c;
    while (n--) c = crc_table[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return ~c;
}

static void put32(std::vector<u8>& v, u32 x)
{
    for (int s = 24; s >= 0; s -= 8) v.push_back(x >> s);
}

static void chunk(FILE* f, const char* type, const std::vector<u8>& data)
{
    std::vector<u8> c;
    put32(c, data.size());
    c.insert(c.end(), type, type + 4);
    c.insert(c.end(), data.begin(), data.end());
    put32(c, crc32(0, c.data() + 4, c.size() - 4));
    fwrite(c.data(), 1, c.size(), f);
}

// 256x384 RGB PNG (top screen over bottom) with stored (uncompressed) deflate blocks.
static void write_png(const std::string& path, const u32* top, const u32* bot)
{
    const int w = 256, h = 384;
    std::vector<u8> raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        const u32* row = y < 192 ? top + y * w : bot + (y - 192) * w;
        for (int x = 0; x < w; x++) {
            u32 p = row[x];  // melonDS framebuffer: 0xFFRRGGBB, 8 bits per channel
            raw.push_back(p >> 16);
            raw.push_back(p >> 8);
            raw.push_back(p);
        }
    }
    std::vector<u8> z = {0x78, 0x01};
    u32 a = 1, b = 0;
    for (u8 c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    for (size_t i = 0; i < raw.size(); i += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - i);
        z.push_back(i + n == raw.size());
        z.push_back(n & 0xFF); z.push_back(n >> 8);
        z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
    }
    put32(z, (b << 16) | a);
    std::vector<u8> ihdr;
    put32(ihdr, w); put32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { perror(path.c_str()); return; }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    chunk(f, "IHDR", ihdr);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    fclose(f);
}

static void write_wav(const std::string& path, const std::vector<s16>& s)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { perror(path.c_str()); return; }
    u32 bytes = s.size() * 2, rate = 48000;  // NDSArgs::OutputSampleRate default
    auto le32 = [f](u32 x) { fwrite(&x, 4, 1, f); };
    auto le16 = [f](u16 x) { fwrite(&x, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); le32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    le32(16); le16(1); le16(2); le32(rate); le32(rate * 4); le16(4); le16(16);
    fwrite("data", 1, 4, f); le32(bytes);
    fwrite(s.data(), 2, s.size(), f);
    fclose(f);
}

// ---------------------------------------------------------------- main

struct Event { int frame; std::string key; int hold; };

// Power management register 0 (bit 2/3 = lower/upper backlight, bit 0 = sound
// amp) is private in melonDS; the explicit-instantiation access rule lets us
// read it without patching the core.
template <typename Tag, typename Tag::type M> struct Rob
{
    friend typename Tag::type get(Tag) { return M; }
};
struct PMRegs
{
    typedef u8 (PowerMan::*type)[8];
    friend type get(PMRegs);
};
template struct Rob<PMRegs, &PowerMan::Registers>;

static u8& powerman_reg0(NDS& nds)
{
    return (nds.SPI.GetPowerMan()->*get(PMRegs()))[0];
}

// ARM9 view of memory for PEEK/TRACE; the bus can't see DTCM (stacks live there).
static u8 peek8(NDS& nds, u32 x)
{
    if ((x & nds.ARM9.DTCMMask) == nds.ARM9.DTCMBase)
        return nds.ARM9.DTCM[x & 0x3FFF];
    return nds.ARM9Read8(x);
}

static const std::map<std::string, int> KEYBIT = {
    {"A", 0}, {"B", 1}, {"SELECT", 2}, {"START", 3}, {"RIGHT", 4}, {"LEFT", 5},
    {"UP", 6}, {"DOWN", 7}, {"R", 8}, {"L", 9}, {"X", 10}, {"Y", 11},
};

static std::vector<std::string> split(const std::string& s, char c)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i <= s.size()) {
        size_t j = s.find(c, i);
        if (j == std::string::npos) j = s.size();
        if (j > i) out.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return out;
}

static bool read_file(const std::string& path, std::vector<u8>& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    out.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    size_t n = fread(out.data(), 1, out.size(), f);
    fclose(f);
    return n == out.size();
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: mdsrun ROM FRAMES OUTPREFIX [EVENTS] [SHOT_FRAMES]\n");
        return 2;
    }
    std::string rompath = argv[1], out = argv[3];
    int frames = atoi(argv[2]);
    std::vector<Event> events;
    std::string evs = argc > 4 ? argv[4] : "";
    if (!evs.empty() && evs[0] == '@') {
        std::vector<u8> f;
        if (!read_file(evs.substr(1), f)) { perror(evs.c_str() + 1); return 1; }
        evs.assign(f.begin(), f.end());
        for (char& c : evs) if (c == '\n' || c == '\r') c = ',';
    }
    if (!evs.empty())
        for (auto& item : split(evs, ',')) {
            auto p = split(item, ':');
            if (p.size() < 2) {
                fprintf(stderr, "bad event %s (want FRAME:KEY[:HOLD])\n", item.c_str());
                return 2;
            }
            events.push_back({atoi(p[0].c_str()), p[1], p.size() > 2 ? atoi(p[2].c_str()) : 4});
        }
    std::vector<int> shots;
    if (argc > 5)
        for (auto& s : split(argv[5], ',')) shots.push_back(atoi(s.c_str()));

    std::vector<u8> rom;
    if (!read_file(rompath, rom)) { perror(rompath.c_str()); return 1; }

    NDSCart::NDSCartArgs cargs {};
    if (const char* sp = getenv("SAV")) {
        g_savpath = sp;
        std::vector<u8> sav;
        if (read_file(g_savpath, sav) && !sav.empty()) {
            cargs.SRAM = std::make_unique<u8[]>(sav.size());
            memcpy(cargs.SRAM.get(), sav.data(), sav.size());
            cargs.SRAMLength = sav.size();
        }
    }
    auto cart = NDSCart::ParseROM(rom.data(), rom.size(), nullptr, std::move(cargs));
    if (!cart) { fprintf(stderr, "ParseROM failed\n"); return 1; }

    NDSArgs args {};
    args.JIT = std::nullopt;
    auto ndsp = std::make_unique<NDS>(std::move(args));  // too big for the stack
    NDS& nds = *ndsp;
    nds.SetNDSCart(std::move(cart));
    nds.Reset();
    nds.SetupDirectBoot(rompath);

    int y, mo, d, h, mi, s;
    if (const char* r = getenv("RTC"); r && sscanf(r, "%d-%d-%d-%d-%d-%d", &y, &mo, &d, &h, &mi, &s) == 6) {
        nds.RTC.SetDateTime(y, mo, d, h, mi, s);
    } else {
        time_t t = time(nullptr);
        struct tm tm;
        gmtime_r(&t, &tm);
        nds.RTC.SetDateTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
    if (const char* pm0 = getenv("PM0"))
        powerman_reg0(nds) = strtoul(pm0, nullptr, 16);
    nds.Start();

    const char* poke = getenv("POKE");
    const char* trace = getenv("TRACE");
    int trace_every = getenv("TRACE_EVERY") ? atoi(getenv("TRACE_EVERY")) : 60;
    int shot_every = getenv("SHOT_EVERY") ? atoi(getenv("SHOT_EVERY")) : 0;
    const char* wavpath = getenv("WAV");
    std::vector<s16> audio;
    s16 abuf[2 * 4096];
    u32 keys = 0xFFF;
    std::map<std::string, int> held;  // key -> release frame

    auto shot = [&](const std::string& path) {
        int fb = nds.GPU.FrontBuffer;
        write_png(path, nds.GPU.Framebuffer[fb][0].get(), nds.GPU.Framebuffer[fb][1].get());
    };

    for (int f = 0; f < frames; f++) {
        for (auto& e : events) {
            if (e.frame != f) continue;
            if (e.key[0] == 'T') {
                auto xy = split(e.key.substr(1), '/');
                nds.TouchScreen(atoi(xy[0].c_str()), atoi(xy[1].c_str()));
                held["touch"] = f + e.hold;
            } else if (e.key == "LID") {
                nds.SetLidClosed(true);
                held["LID"] = f + e.hold;
            } else if (KEYBIT.count(e.key)) {
                keys &= ~(1u << KEYBIT.at(e.key));
                held[e.key] = f + e.hold;
            } else {
                fprintf(stderr, "unknown key %s\n", e.key.c_str());
            }
        }
        for (auto it = held.begin(); it != held.end();) {
            if (it->second != f) { ++it; continue; }
            if (it->first == "touch") nds.ReleaseScreen();
            else if (it->first == "LID") nds.SetLidClosed(false);
            else keys |= 1u << KEYBIT.at(it->first);
            it = held.erase(it);
        }
        nds.SetKeyMask(keys);
        if (poke && f % 30 == 0)
            for (auto& item : split(poke, ',')) {
                unsigned a, v;
                if (sscanf(item.c_str(), "%x:%u", &a, &v) == 2) nds.ARM9Write32(a, v);
            }
        nds.RunFrame();
        int n;
        while ((n = nds.SPU.ReadOutput(abuf, 4096)) > 0)
            if (wavpath) audio.insert(audio.end(), abuf, abuf + 2 * n);
        if (trace && f % trace_every == 0) {
            printf("TRACE %d pm=%02x", f, powerman_reg0(nds));
            for (auto& item : split(trace, ',')) {
                auto p = split(item, ':');
                u32 a = strtoul(p[0].c_str(), nullptr, 16);
                int len = p.size() > 1 ? atoi(p[1].c_str()) : 4;
                printf(" ");
                for (int i = 0; i < len; i++) printf("%02x", peek8(nds, a + i));
            }
            printf("\n");
            fflush(stdout);
        }
        if (shot_every && f % shot_every == 0 && f) {
            char name[32];
            snprintf(name, sizeof name, "_%05d.png", f);
            shot(out + name);
        }
        for (int sf : shots)
            if (sf == f) {
                char name[32];
                snprintf(name, sizeof name, "_%05d.png", f);
                shot(out + name);
            }
    }
    shot(out + "_final.png");

    printf("RESULT");
    for (int i = 0; i < 16; i++) printf(" %08x", nds.ARM9Read32(0x0237F000 + 4 * i));
    printf("\n");
    if (const char* pk = getenv("PEEK"))
        for (auto& item : split(pk, ',')) {
            auto p = split(item, ':');
            u32 a = strtoul(p[0].c_str(), nullptr, 16);
            int n = p.size() > 1 ? atoi(p[1].c_str()) : 4;
            printf("PEEK %08x", a);
            for (int i = 0; i < n; i++) printf(" %02x", peek8(nds, a + i));
            printf("\n");
        }
    printf("SAVE %u %zu\n", nds.GetNDSCart() ? nds.GetNDSCart()->GetROMParams().SaveMemType : 0, g_save.size());
    printf("PC9 %x PC7 %x\n", nds.ARM9.R[15], nds.ARM7.R[15]);
    // After an exception the ARM9 sits in FreeBIOS (mode 0x17 = abort);
    // R14 then points 8 bytes past the faulting instruction.
    printf("CPU9 cpsr %x\n", nds.ARM9.CPSR);
    printf("REG9");
    for (int i = 0; i < 16; i++) printf(" %x", nds.ARM9.R[i]);
    // melonDS swaps banked registers on a mode switch: in abort mode R_ABT
    // holds the interrupted (system mode) SP and LR.
    printf(" sys_sp %x sys_lr %x\n", nds.ARM9.R_ABT[0], nds.ARM9.R_ABT[1]);
    if (getenv("MPU")) {
        printf("MPU ctrl %08x dataRW %08x codeRW %08x dcache %02x dwrite %02x icache %02x\n",
               nds.ARM9.CP15Control, nds.ARM9.PU_DataRW, nds.ARM9.PU_CodeRW,
               nds.ARM9.PU_DataCacheable, nds.ARM9.PU_DataCacheWrite, nds.ARM9.PU_CodeCacheable);
        for (int i = 0; i < 8; i++) printf("MPU region %d %08x\n", i, nds.ARM9.PU_Region[i]);
    }
    if (wavpath) write_wav(wavpath, audio);
    if (!g_savpath.empty() && !g_save.empty()) {
        FILE* f = fopen(g_savpath.c_str(), "wb");
        if (f) { fwrite(g_save.data(), 1, g_save.size(), f); fclose(f); }
    }
    return 0;
}
