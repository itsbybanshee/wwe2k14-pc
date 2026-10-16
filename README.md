# WWE 2K14 PC Recomp (By Banshee)

WWE 2K14 PC recompilation with a launcher that adds modern graphics options, mods, custom logos, your own music and more.

- No game data included. You need your own legally acquired WWE 2K14 (Xbox 360) files: a disc image (`.iso`) or the
  extracted game folder.

## Features

- **Resolution**: 720p, 1080p, 1440p or 4K
- **AMD FidelityFX Super Resolution (FSR)**
- **NVIDIA FXAA 3.11 Quality** anti-aliasing (FXAA / Max)
- **60 FPS entrances and victory scenes**
- **Keyboard gameplay**
- **Mouse support**: point-and-click menus, wrestler select, clickable button prompts
- **Runs straight from your `.iso`**, or from an extracted game folder
- **Mods** (wrestlers, arenas, menus, videos, sounds) with an on/off switch each
- **Total conversion** one-click install for projects like "Wrestling Legends" 
- **DLC support**
- **Achievements** with unlock notifications and progress tracker
- **Custom music** in the game's soundtrack or entrances (MP3, WAV, FLAC, OGG)
- **Custom logos**, including import from a PS3 save, with a **logo library** to keep more than 20 and choose imports

## Installation

1. Unzip the download into a folder of its own (e.g. `C:\Games\WWE 2K14 PC`).
2. Run `WWE 2K14 PC.exe`.
3. Under **Game files**, click **Choose ISO** and pick your WWE 2K14 `.iso` (or **Choose folder** for an extracted
   copy).
4. Pick your settings and press **Play**.

**Requirements:** Windows 10/11 64-bit, a Vulkan graphics card with an up-to-date driver, controller or keyboard,
and the [Microsoft Edge WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) (already on
Windows 11 and most Windows 10 PCs).

Saves, settings, logos, music and DLC are kept in `userdata` next to the launcher, mods in `mods`, logs in `logs`.
To update, unzip the new version over the old one: `userdata` and `mods` are kept.

## Controls

An Xbox controller (or any controller Windows sees as one) works like on the Xbox 360. Keyboard works too; the full
layout is under **Controls** in the launcher.

Mouse in menus: point to select, click to choose, wheel to scroll, click on-screen button prompts (like **B Back**),
middle click = Start.

## Troubleshooting

- **Launcher doesn't open**: unzip everything first, and check your antivirus didn't block `WWE 2K14 PC.exe`.
- **Launcher is blank / black**: install the WebView2 Runtime (link above).
- **"Not an Xbox 360 disc image"**: the `.iso` has to be the Xbox 360 version; PS3 isos don't work.
- **Game doesn't start or closes right away**: update your graphics driver, check your antivirus didn't remove
  anything from the `bin` folder, and try the game folder instead of the `.iso` (or the other way round).
- **Low framerate**: lower the resolution, turn on FSR, turn off shadows, or set anti-aliasing to Off.
- **A mod crashes the game**: turn it off in the Mods tab. Some mods made for other versions of the game won't work.
- **Logos tab says there is no logo save**: in the game, open Creation Suite → Paint Tool and save any logo once.
- When reporting a problem, include what happened, your graphics card and Windows version, and the newest file from
  `logs`.

Online features of the original game are not available. This is an early release, so expect some bugs.

## Credits

- **ReXGlue SDK** by Tom Clay (BSD 3-Clause)
- The libraries listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); the SDK patches are under the SDK's
  licence ([patches/sdk/LICENSE](patches/sdk/LICENSE))
- **Banshee** ([@itsbybanshee](https://x.com/itsbybanshee)): WWE 2K14 PC port and launcher

Fan project, not affiliated with WWE, 2K or Yuke's.
