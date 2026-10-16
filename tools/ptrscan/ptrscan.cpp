// Pointer scan over a memory dump (src/re_tools.cpp, logs\memdump-<n>.bin): finds chains of pointers from game globals
// (the image, 0x82000000-0x83FFFFFF) to a target guest address, e.g. a UI object found by other means.
//   ptrscan <dump> <target hex> [max depth=5] [max offset hex=0x800]
// Prints chains as: global +off -> ... -> target, i.e. [[[global]+o1]+o2]+o3 == target.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

struct Region { uint32_t addr; std::vector<uint8_t> data; };
static uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
static bool image(uint32_t a) { return a >= 0x82000000u && a < 0x84000000u; }
static bool heap(uint32_t a) { return (a >= 0x40000000u && a < 0x80000000u) || (a >= 0xA0000000u && a < 0xC0000000u); }

int main(int argc, char** argv) {
  if (argc < 3) { std::puts("ptrscan <dump> <target> [depth] [maxoff]"); return 1; }
  const uint32_t target = std::strtoul(argv[2], nullptr, 16);
  const int depth = argc > 3 ? std::atoi(argv[3]) : 5;
  const uint32_t maxoff = argc > 4 ? std::strtoul(argv[4], nullptr, 16) : 0x800;
  FILE* f = std::fopen(argv[1], "rb");
  if (!f) return 1;
  std::vector<std::pair<uint32_t, uint32_t>> ptrs;  // (value, address) for words that look like heap pointers
  uint32_t h[2];
  while (std::fread(h, 4, 2, f) == 2) {
    std::vector<uint8_t> d(h[1]);
    if (std::fread(d.data(), 1, h[1], f) != h[1]) break;
    for (uint32_t i = 0; i + 4 <= h[1]; i += 4) {
      const uint32_t v = be32(&d[i]);
      if (heap(v)) ptrs.push_back({v, h[0] + i});
    }
  }
  std::fclose(f);
  std::sort(ptrs.begin(), ptrs.end());
  std::fprintf(stderr, "%zu pointers\n", ptrs.size());
  // Breadth-first from the target: level n holds addresses from which the target is n dereferences away.
  std::map<uint32_t, std::pair<uint32_t, uint32_t>> parent;  // address -> (next address, offset)
  std::vector<uint32_t> level = {target};
  int printed = 0;
  for (int d = 0; d < depth && !level.empty() && printed < 40; ++d) {
    std::vector<uint32_t> next;
    for (uint32_t t : level) {
      auto it = std::lower_bound(ptrs.begin(), ptrs.end(), std::pair<uint32_t, uint32_t>{t > maxoff ? t - maxoff : 0, 0});
      for (; it != ptrs.end() && it->first <= t; ++it) {
        const uint32_t holder = it->second;
        if (parent.count(holder)) continue;
        parent[holder] = {t, t - it->first};
        if (image(holder)) {
          std::printf("%08X", holder);
          for (uint32_t a = holder; a != target;) {
            auto [n, off] = parent[a];
            std::printf(" +%X -> %08X", off, n);
            a = n;
          }
          std::printf("\n");
          if (++printed >= 40) break;
        } else if (next.size() < 200000) {
          next.push_back(holder);
        }
      }
      if (printed >= 40) break;
    }
    std::fprintf(stderr, "depth %d: %zu holders\n", d + 1, next.size());
    level.swap(next);
  }
  return 0;
}
