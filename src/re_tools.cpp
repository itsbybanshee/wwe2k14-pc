// Reverse-engineering aid (WWE2K14_FIND_TEXT="Xbox Live"): finds a piece of menu text in guest memory and logs the
// code that reads it. Several texts can be given separated by '|'; a leading '^' / trailing '$' require a NUL before /
// after the text (a whole string, not part of a sentence). Writes to the watched text are not reported.
//   1. Every 3 s (from 20 s after start) guest memory is scanned for the text (ASCII, UTF-16 BE/LE); matches are
//      logged as guest addresses.
//   2. The pages holding the first matches are made inaccessible. A read inside the text logs the host call chain
//      ([re-tools] hit ... rva:...): recompiled functions are named after their guest address, so the linker map
//      names them (tools/cp_profile_report.py style). Accesses to the rest of a page are single-stepped through.
// Off unless the variable is set.

#if defined(_WIN32)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <rex/logging.h>
#include <rex/runtime.h>

namespace {

struct Watch {
  uint8_t* page;
  uint8_t* lo;
  uint8_t* hi;
  DWORD old_protect;
  int hits;
  bool writes = false;  // report writes (page read-only) instead of reads (page inaccessible)
};

std::mutex g_mutex;
std::vector<Watch> g_watches;
thread_local Watch* t_stepping = nullptr;
uint64_t g_image = 0;
uint8_t* g_membase = nullptr;
constexpr int kMaxHitsPerWatch = 8;

std::string Chain(CONTEXT context) {
  std::string s;
  for (int depth = 0; depth < 14 && context.Rip; ++depth) {
    DWORD64 base = 0;
    PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &base, nullptr);
    const uint64_t pc = function && base == g_image ? g_image + function->BeginAddress : context.Rip;
    s += pc >= g_image && pc - g_image < 0x10000000 ? fmt::format(" {:X}", pc - g_image) : std::string(" ?");
    if (!function) {
      context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
      context.Rsp += 8;
    } else {
      void* handler_data = nullptr;
      DWORD64 establisher = 0;
      RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, context.Rip, function, &context, &handler_data, &establisher, nullptr);
    }
  }
  return s;
}

LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
  const DWORD code = info->ExceptionRecord->ExceptionCode;
  if (code == EXCEPTION_SINGLE_STEP && t_stepping) {
    DWORD old;
    VirtualProtect(t_stepping->page, 4096, t_stepping->writes ? PAGE_READONLY : PAGE_NOACCESS, &old);
    t_stepping = nullptr;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  if (code != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
  uint8_t* address = reinterpret_cast<uint8_t*>(info->ExceptionRecord->ExceptionInformation[1]);
  std::lock_guard<std::mutex> lock(g_mutex);
  for (Watch& w : g_watches) {
    if (address < w.page || address >= w.page + 4096 || w.hits >= kMaxHitsPerWatch) continue;
    DWORD old;
    VirtualProtect(w.page, 4096, w.old_protect, &old);
    const bool is_write = info->ExceptionRecord->ExceptionInformation[0] == 1;
    if (address >= w.lo && address < w.hi && is_write == w.writes) {
      ++w.hits;
      REXLOG_INFO("[re-tools] hit guest {:08X} ({}) rva:{}", uint32_t(address - g_membase),
                  info->ExceptionRecord->ExceptionInformation[0] ? "write" : "read", Chain(*info->ContextRecord));
      if (w.hits >= kMaxHitsPerWatch) return EXCEPTION_CONTINUE_EXECUTION;  // stays unprotected
    }
    // Let this one access through, then protect the page again.
    t_stepping = &w;
    info->ContextRecord->EFlags |= 0x100;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

void Scan(std::string text) {
  bool need_nul_before = !text.empty() && text.front() == '^';
  bool need_nul_after = !text.empty() && text.back() == '$';
  if (need_nul_before) text.erase(0, 1);
  if (need_nul_after) text.pop_back();
  std::vector<std::string> patterns = {text};
  std::string be, le;
  for (char c : text) {
    be += '\0', be += c;
    le += c, le += '\0';
  }
  patterns.push_back(be);
  patterns.push_back(le);
  std::vector<uint32_t> found;
  for (uint64_t lo : {0x40000000ull, 0xA0000000ull}) {
    uint8_t* p = g_membase + lo;
    uint8_t* end = g_membase + lo + 0x20000000ull + (lo == 0x40000000ull ? 0x20000000ull : 0);
    while (p < end && found.size() < 16) {
      MEMORY_BASIC_INFORMATION mbi;
      if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
      uint8_t* region_end = std::min(static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize, end);
      if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY)) &&
          !(mbi.Protect & PAGE_GUARD)) {
        for (const std::string& pat : patterns) {
          for (uint8_t* q = p; q + pat.size() <= region_end;) {
            q = std::search(q, region_end, pat.begin(), pat.end(),
                            [](uint8_t a, char b) { return a == uint8_t(b); });
            if (q + pat.size() > region_end) break;
            const size_t unit = &pat == &patterns[0] ? 1 : 2;
            if ((need_nul_before && (q - unit < p || q[-1] != 0 || (unit == 2 && q[-2] != 0))) ||
                (need_nul_after && (q + pat.size() + unit > region_end || q[pat.size()] != 0 ||
                                    (unit == 2 && q[pat.size() + 1] != 0)))) {
              q += pat.size();
              continue;
            }
            found.push_back(uint32_t(q - g_membase));
            REXLOG_INFO("[re-tools] \"{}\" ({}) at guest {:08X}", text,
                        &pat == &patterns[0] ? "ascii" : &pat == &patterns[1] ? "utf16be" : "utf16le",
                        uint32_t(q - g_membase));
            if (found.size() >= 16) break;
            q += pat.size();
          }
        }
      }
      p = region_end;
    }
  }
  if (found.empty()) return;
  std::lock_guard<std::mutex> lock(g_mutex);
  for (uint32_t guest : found) {
    if (g_watches.size() >= 12) break;
    uint8_t* host = g_membase + guest;
    uint8_t* page = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(host) & ~uintptr_t(4095));
    if (std::any_of(g_watches.begin(), g_watches.end(), [&](const Watch& w) { return w.page == page; })) continue;
    Watch w{page, host, host + text.size() * 2, 0, 0};
    if (VirtualProtect(page, 4096, PAGE_NOACCESS, &w.old_protect)) {
      g_watches.push_back(w);
      REXLOG_INFO("[re-tools] watching guest {:08X}", guest);
    }
  }
}

void Run(std::string texts) {
  std::this_thread::sleep_for(std::chrono::seconds(20));
  g_image = reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
  AddVectoredExceptionHandler(1, OnException);
  for (;;) {
    auto* runtime = rex::Runtime::instance();
    g_membase = runtime ? runtime->virtual_membase() : nullptr;
    if (g_membase) {
      bool any;
      {
        std::lock_guard<std::mutex> lock(g_mutex);
        any = !g_watches.empty();
      }
      if (!any) {
        size_t start = 0;
        for (size_t bar; (bar = texts.find('|', start)) != std::string::npos; start = bar + 1) {
          Scan(texts.substr(start, bar - start));
        }
        Scan(texts.substr(start));
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }
}

// Memory dumps (WWE2K14_MEM_DUMP=1): F9 while the game window is in front writes the committed guest memory
// (0x40000000-0x7FFFFFFF, 0xA0000000-0xBFFFFFFF and the image at 0x82000000-0x83FFFFFF) to logs\memdump-<n>.bin as records of
// {u32 guest address, u32 size, bytes} (little-endian header, guest bytes as-is), for offline comparison.
void DumpMemory(int n) {
  char name[64];
  std::snprintf(name, sizeof(name), "logs/memdump-%d.bin", n);
  FILE* f = std::fopen(name, "wb");
  if (!f) return;
  uint64_t total = 0;
  // Heap, physical memory and the game image (its globals).
  for (auto [lo, size] : {std::pair{0x40000000ull, 0x40000000ull}, std::pair{0xA0000000ull, 0x20000000ull},
                          std::pair{0x82000000ull, 0x02000000ull}}) {
    uint8_t* p = g_membase + lo;
    uint8_t* end = g_membase + lo + size;
    while (p < end) {
      MEMORY_BASIC_INFORMATION mbi;
      if (!VirtualQuery(p, &mbi, sizeof(mbi))) break;
      uint8_t* region_end = std::min(static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize, end);
      if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY)) &&
          !(mbi.Protect & PAGE_GUARD)) {
        for (uint8_t* q = p; q < region_end;) {
          const uint32_t size = uint32_t(std::min<uint64_t>(region_end - q, 0x1000000));
          const uint32_t header[2] = {uint32_t(q - g_membase), size};
          std::fwrite(header, 4, 2, f);
          std::fwrite(q, 1, size, f);
          total += size;
          q += size;
        }
      }
      p = region_end;
    }
  }
  std::fclose(f);
  REXLOG_INFO("[re-tools] wrote {} ({} MB)", name, total >> 20);
}

// logs/watch.req holding a guest address (hex) starts a write watch on its 16 bytes: the code writing there is logged
// like a read hit ([re-tools] hit ... (write) rva:...).
void WatchWrites() {
  FILE* f = std::fopen("logs/watch.req", "rb");
  if (!f) return;
  char text[32] = {};
  std::fread(text, 1, sizeof(text) - 1, f);
  std::fclose(f);
  DeleteFileW(L"logs/watch.req");
  const uint32_t guest = uint32_t(std::strtoul(text, nullptr, 16));
  if (!guest || !g_membase) return;
  static std::once_flag once;
  std::call_once(once, [] {
    g_image = reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
    AddVectoredExceptionHandler(1, OnException);
  });
  uint8_t* host = g_membase + guest;
  uint8_t* page = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(host) & ~uintptr_t(4095));
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_watches.size() >= 12) return;
  g_watches.reserve(12);
  Watch w{page, host, host + 16, 0, 0, true};
  if (VirtualProtect(page, 4096, PAGE_READONLY, &w.old_protect)) {
    g_watches.push_back(w);
    REXLOG_INFO("[re-tools] watching writes to guest {:08X}", guest);
  }
}

void RunDumps() {
  int n = 0;
  bool was_down = false;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    const bool front = w && GetWindowThreadProcessId(w, &pid) && pid == GetCurrentProcessId();
    const bool down = front && (GetAsyncKeyState(VK_F9) & 0x8000);
    // A test script can also ask for a dump by creating logs/memdump.req (deleted when taken).
    const bool requested = DeleteFileW(L"logs/memdump.req") != 0;
    if ((down && !was_down) || requested) {
      auto* runtime = rex::Runtime::instance();
      g_membase = runtime ? runtime->virtual_membase() : nullptr;
      if (g_membase) DumpMemory(++n);
    }
    if (!g_membase) {
      auto* runtime = rex::Runtime::instance();
      g_membase = runtime ? runtime->virtual_membase() : nullptr;
    }
    WatchWrites();
    was_down = down;
  }
}

struct Starter {
  Starter() {
    if (const char* v = std::getenv("WWE2K14_FIND_TEXT"); v && *v) std::thread(Run, std::string(v)).detach();
    if (const char* v = std::getenv("WWE2K14_MEM_DUMP"); v && *v == '1') std::thread(RunDumps).detach();
  }
} starter;

}  // namespace

#endif  // _WIN32
