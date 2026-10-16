// Started without the launcher (e.g. double-clicking wwe2k14.exe): use the settings last saved in the launcher.

#pragma once

#include <rex/rex_app.h>

namespace wwe2k14 {

// Reads <root>\userdata\launcher.json (root: the WWE2K14PC folder when run from its build output, else the exe's
// folder, like the launcher) and applies what the launcher would pass: game folder, user data, title update,
// resolution, window size, fullscreen, VSync / 60 FPS / low latency, custom music and a log file. Values given
// on the command line win. If the launcher selects the other version (vanilla / title update), that exe is
// started instead and this process ends.
void ApplyLauncherSettings(rex::PathConfig& paths, bool started_by_launcher);

}  // namespace wwe2k14
