#include "achievement_sound.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

#include <rex/logging.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

// stb_vorbis is compiled into the SDK's decoders library (C linkage).
extern "C" int stb_vorbis_decode_memory(const unsigned char* mem, int len, int* channels, int* sample_rate,
                                        short** output);

namespace wwe2k14 {
namespace {

void Put16(std::vector<uint8_t>& out, uint16_t v) {
  out.push_back(uint8_t(v));
  out.push_back(uint8_t(v >> 8));
}

void Put32(std::vector<uint8_t>& out, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    out.push_back(uint8_t(v >> (i * 8)));
}

}  // namespace

std::vector<uint8_t> LoadOggAsWav(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    return {};
  const std::vector<unsigned char> ogg((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  int channels = 0, rate = 0;
  short* samples = nullptr;
  const int frames = stb_vorbis_decode_memory(ogg.data(), int(ogg.size()), &channels, &rate, &samples);
  if (frames <= 0 || !samples || channels <= 0) {
    std::free(samples);
    REXLOG_WARN("[wwe2k14] achievement sound {} could not be decoded", path.string());
    return {};
  }
  const uint32_t data_bytes = uint32_t(frames) * uint32_t(channels) * 2;
  std::vector<uint8_t> wav;
  wav.reserve(44 + data_bytes);
  wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
  Put32(wav, 36 + data_bytes);
  wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  Put32(wav, 16);                                // fmt chunk size
  Put16(wav, 1);                                 // PCM
  Put16(wav, uint16_t(channels));
  Put32(wav, uint32_t(rate));
  Put32(wav, uint32_t(rate) * uint32_t(channels) * 2);  // byte rate
  Put16(wav, uint16_t(channels * 2));            // block align
  Put16(wav, 16);                                // bits per sample
  wav.insert(wav.end(), {'d', 'a', 't', 'a'});
  Put32(wav, data_bytes);
  const auto* pcm = reinterpret_cast<const uint8_t*>(samples);
  wav.insert(wav.end(), pcm, pcm + data_bytes);  // stb_vorbis returns little-endian 16-bit samples
  std::free(samples);
  REXLOG_INFO("[wwe2k14] achievement sound loaded: {:.2f} s, {} Hz, {} channel(s)", double(frames) / rate, rate,
              channels);
  return wav;
}

void PlayWavAsync(const std::vector<uint8_t>& wav) {
#ifdef _WIN32
  if (!wav.empty())
    PlaySoundW(reinterpret_cast<LPCWSTR>(wav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#else
  (void)wav;
#endif
}

}  // namespace wwe2k14
