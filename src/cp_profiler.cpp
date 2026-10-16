// Diagnostic (WWE2K14_CP_PROFILE=1): a sampling profiler for the GPU command processor thread ("GPU Commands").
// About 1000 times a second the thread is suspended, its stack walked with the x64 unwind tables, and the functions
// on it counted. Every 10 s the log gets the busiest functions as image offsets ([cp-profile] lines); map them to
// names with the linker map (wwe2k14.map / wwe2k14_tu.map next to the exe).
//   self  = samples with the function at the top of the stack (time spent in its own code)
//   total = samples with the function anywhere on the stack (time spent in it and what it calls)

#if defined(_WIN32)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <rex/logging.h>

namespace {

HANDLE FindThreadByName(const wchar_t* name) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return nullptr;
  THREADENTRY32 entry{sizeof(entry)};
  HANDLE found = nullptr;
  const DWORD pid = GetCurrentProcessId();
  for (BOOL ok = Thread32First(snapshot, &entry); ok && !found; ok = Thread32Next(snapshot, &entry)) {
    if (entry.th32OwnerProcessID != pid) continue;
    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                               entry.th32ThreadID);
    if (!thread) continue;
    PWSTR description = nullptr;
    // XThread names carry a handle suffix: "GPU Commands (F8000034)".
    if (SUCCEEDED(GetThreadDescription(thread, &description)) && description &&
        wcsncmp(description, name, wcslen(name)) == 0) {
      found = thread;
    } else {
      CloseHandle(thread);
    }
    if (description) LocalFree(description);
  }
  CloseHandle(snapshot);
  return found;
}

void Run() {
  HANDLE thread = nullptr;
  while (!(thread = FindThreadByName(L"GPU Commands"))) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  const uint64_t image = reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
  REXLOG_INFO("[cp-profile] sampling \"GPU Commands\", image base {:X}", image);

  constexpr int kMaxDepth = 24;
  std::unordered_map<uint32_t, uint32_t> self, total;
  uint32_t samples = 0;
  auto window_start = std::chrono::steady_clock::now();
  for (;;) {
    uint64_t frames[kMaxDepth];
    int depth = 0;
    if (SuspendThread(thread) != DWORD(-1)) {
      CONTEXT context{};
      context.ContextFlags = CONTEXT_FULL;
      if (GetThreadContext(thread, &context)) {
        // No allocation while the thread is suspended (it may hold the heap lock).
        while (depth < kMaxDepth && context.Rip) {
          DWORD64 base = 0;
          PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &base, nullptr);
          frames[depth++] = function && base == image ? image + function->BeginAddress : context.Rip;
          if (!function) {
            context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
            context.Rsp += 8;
          } else {
            void* handler_data = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, context.Rip, function, &context, &handler_data, &establisher,
                             nullptr);
          }
        }
      }
      ResumeThread(thread);
    }
    if (depth) {
      ++samples;
      auto offset = [&](uint64_t pc) { return pc >= image ? uint32_t(std::min<uint64_t>(pc - image, UINT32_MAX)) : 0u; };
      ++self[offset(frames[0])];
      for (int i = 0; i < depth; ++i) {
        const uint32_t o = offset(frames[i]);
        bool seen = false;
        for (int j = 0; j < i && !seen; ++j) seen = offset(frames[j]) == o;
        if (!seen) ++total[o];
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    const auto now = std::chrono::steady_clock::now();
    if (now - window_start >= std::chrono::seconds(10)) {
      auto top = [](const std::unordered_map<uint32_t, uint32_t>& counts, size_t n) {
        std::vector<std::pair<uint32_t, uint32_t>> v(counts.begin(), counts.end());
        std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; });
        if (v.size() > n) v.resize(n);
        std::string s;
        for (auto& [o, c] : v) s += fmt::format(" {:X}:{}", o, c);
        return s;
      };
      REXLOG_INFO("[cp-profile] samples={} self:{}", samples, top(self, 25));
      REXLOG_INFO("[cp-profile] samples={} total:{}", samples, top(total, 40));
      self.clear();
      total.clear();
      samples = 0;
      window_start = now;
    }
  }
}

struct Starter {
  Starter() {
    const char* v = std::getenv("WWE2K14_CP_PROFILE");
    if (v && v[0] == '1') std::thread(Run).detach();
  }
} starter;

}  // namespace

#endif  // _WIN32
