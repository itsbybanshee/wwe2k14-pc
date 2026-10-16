# WWE 2K14 PC (Beta) [By Banshee]

WWE 2K14 running natively on Windows. It's not an emulator: the Xbox 360 game is rebuilt to run on PC (static
recompilation), with a launcher that adds modern graphics options, mods, custom logos, your own music and more.

- No game data included. You need your own legally acquired WWE 2K14 (Xbox 360) files: a disc image (`.iso`) or the
  extracted game folder.

## Features

- **Resolution**: 720p, 1080p, 1440p or 4K
- **AMD FidelityFX Super Resolution (FSR)** with edge-preserving sharpening (RCAS) for high framerates at high
  resolutions
- **NVIDIA FXAA 3.11 Quality** anti-aliasing (FXAA / Max)
- Smooth frame pacing, with **60 FPS entrances and victory scenes**
- Windowed, borderless or fullscreen; anisotropic filtering (4x, 16x), shadows toggle, FPS counter
- **Controller or keyboard**, plus the **mouse in menus**: point-and-click menus, wrestler select, clickable button
  prompts, option sliders, Yes/No popups, wheel scrolling
- **Runs straight from your `.iso`**, or from an extracted game folder
- **Mods** (wrestlers, arenas, menus, videos, sounds) with an on/off switch each, also with an `.iso`
- **DLC**, achievements with unlock pop-ups, screenshots with F12
- **Your own music** in the game's soundtrack (MP3, WAV, FLAC, OGG)
- **Custom logos**, including import from a PS3 save, with a **logo library** to keep more than 20 and choose which
  ones the game uses

## Installation

1. Unzip the download into a folder of its own (e.g. `C:\Games\WWE 2K14 PC`). Don't run it from inside the zip, and
   avoid `Program Files`.
2. Run `WWE 2K14 PC.exe`. If Windows says "Windows protected your PC", click **More info** → **Run anyway**.
3. Under **Game files**, click **Choose ISO** and pick your WWE 2K14 `.iso` (or **Choose folder** for an extracted
   copy).
4. Pick your settings and press **Play**.

**Requirements:** Windows 10/11 64-bit, a Vulkan graphics card with an up-to-date driver, controller or keyboard,
and the [Microsoft Edge WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) (already on
Windows 11 and most Windows 10 PCs).

## The launcher

| Section | What it does |
|---|---|
| **Game files** | Choose your `.iso` or game folder. **Install update** adds the official title update if you have its files; **Updated** then plays with it. |
| **Resolution** | 720p is the original Xbox 360 look; 1080p and up are sharper and need a stronger graphics card. |
| **Graphics** | V-Sync, 60 FPS cinematics, shadows, FPS counter, window mode, anisotropic filtering, anti-aliasing (FXAA / Max), upscaling (FSR). |
| **Achievements** | Your progress. Start the game once to load the list. |
| **DLC** | Add official DLC packages you own. |
| **Mods** | Add a mod as a `.zip`, a folder or a single file; each has its own on/off switch. Close the game before changing mods. |
| **Music** | Add your own songs, then choose **User playlist** in the game's soundtrack options. |
| **Logos** | **Add logos** (PNG, JPG, BMP, GIF), **Import PS3 save** (`LOGOS.DAT` or a `.zip` / `.rar` of the save), and the **Library** to choose which 20 the game uses. Save one logo in the game's Paint Tool first. |
| **Controls** | The full controller and keyboard layout. |

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
