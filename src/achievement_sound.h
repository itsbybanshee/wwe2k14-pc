#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace wwe2k14 {

// Decodes an Ogg Vorbis file (achievement.ogg next to the exe) into an in-memory 16-bit PCM WAV, ready for
// PlaySound. Returns an empty vector when the file is missing or cannot be decoded.
std::vector<uint8_t> LoadOggAsWav(const std::filesystem::path& path);

// Plays a WAV built by LoadOggAsWav without blocking. The buffer must stay alive while it plays.
void PlayWavAsync(const std::vector<uint8_t>& wav);

}  // namespace wwe2k14
