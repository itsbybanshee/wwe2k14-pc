#pragma once

namespace rex {
class Runtime;
}

namespace wwe2k14 {
// Saves the game's output image at its internal render resolution to Documents\wwe2k14\Screenshots (PNG).
void TakeScreenshot(rex::Runtime* runtime);
}  // namespace wwe2k14
