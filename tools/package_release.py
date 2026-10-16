#!/usr/bin/env python3
"""Build and package a clean, sanitized, ready-to-distribute release of WWE 2K14 PC.
Strips symbols, removes local developer paths (user name, checkout folder), minifies launcher scripts,
encrypts UI assets into bin/ui.dat, places game binaries in bin/, and packs everything
directly into dist/WWE 2K14 - PC Recomp (By Banshee).zip without a pre-folder.
"""

import json
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIST = ROOT / "dist"
STAGING = DIST / "staging"
ZIP_OUT = DIST / "WWE 2K14 - PC Recomp (By Banshee).zip"
SINGLE_DIR = ROOT / "out" / "publish" / "launcher_single"
# This checkout's folder as it can appear in compiled binaries (both slash styles).
DEV_PATHS = [str(ROOT).encode(), str(ROOT).replace("\\", "/").encode()]


def run(cmd: list[str]) -> None:
    print(f"Running: {' '.join(cmd)}")
    res = subprocess.run(cmd, cwd=str(ROOT))
    if res.returncode != 0:
        raise RuntimeError(f"Command failed with code {res.returncode}: {' '.join(cmd)}")


def build_single_file_launcher() -> None:
    print("Building single-file launcher with embedded assemblies and stripped debug symbols...")
    run([
        "dotnet", "publish", "launcher/WWE2K14Launcher.csproj",
        "-c", "Release",
        "-r", "win-x64",
        "--self-contained", "false",
        "-p:PublishSingleFile=true",
        "-p:PublishReadyToRun=true",
        "-p:IncludeNativeLibrariesForSelfExtract=true",
        "-p:DebugType=none",
        "-p:DebugSymbols=false",
        "-o", str(SINGLE_DIR)
    ])


def sanitize_pe_pdb_path(exe_path: Path, filename_only: bytes) -> None:
    print(f"Sanitizing debug directory in: {exe_path.name}")
    with open(exe_path, "rb") as f:
        data = bytearray(f.read())

    # PDB path in the debug directory: keep only the file name.
    for dev_pattern in DEV_PATHS:
        idx = data.find(dev_pattern)
        while idx != -1:
            end_idx = data.find(b"\x00", idx)
            if end_idx == -1:
                break
            if data[end_idx - 4:end_idx].lower() == b".pdb":
                data[idx:end_idx] = filename_only.ljust(end_idx - idx, b"\x00")
                print(f"  Replaced PDB path at offset {idx}")
            idx = data.find(dev_pattern, idx + 1)

    # Source paths compiled into the binaries (__FILE__ in log messages): replace the checkout folder with a
    # neutral name of the same length so the strings keep their size.
    for dev_pattern in DEV_PATHS:
        neutral = (b"wwe2k14" + b"_" * len(dev_pattern))[:len(dev_pattern)]
        count = data.count(dev_pattern)
        if count:
            data = bytearray(bytes(data).replace(dev_pattern, neutral))
            print(f"  Replaced {count} source paths")

    with open(exe_path, "wb") as f:
        f.write(data)


def minify_js(src: Path, dst: Path) -> None:
    with open(src, "r", encoding="utf-8") as f:
        content = f.read()

    # Strip block comments
    content = re.sub(r"/\*[\s\S]*?\*/", "", content)

    # Strip line comments
    lines = []
    for line in content.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        lines.append(line)

    dst.write_text("\n".join(lines), encoding="utf-8")


def pack_encrypted_ui(launcher_exe: Path, ui_src: Path, output_dat: Path) -> None:
    print(f"Encrypting UI assets into {output_dat.name}...")
    temp_ui = ROOT / "out" / "temp_ui_pack"
    if temp_ui.exists():
        shutil.rmtree(temp_ui)
    temp_ui.mkdir(parents=True, exist_ok=True)

    for item in ui_src.iterdir():
        if item.name == "launcher.js":
            minify_js(item, temp_ui / "launcher.js")
        else:
            shutil.copy2(item, temp_ui / item.name)

    # Invoke launcher in packing mode
    run([str(launcher_exe), "--pack-ui", str(temp_ui), str(output_dat)])
    shutil.rmtree(temp_ui)

    if not output_dat.exists() or output_dat.stat().st_size == 0:
        raise RuntimeError(f"UI encryption failed: {output_dat} does not exist or is empty")
    print(f"UI encrypted bundle created: {output_dat.stat().st_size} bytes")


def main() -> None:
    build_single_file_launcher()

    if STAGING.exists():
        shutil.rmtree(STAGING)
    STAGING.mkdir(parents=True, exist_ok=True)

    bin_dir = STAGING / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)

    print("Copying standalone launcher executable to root...")
    launcher_src = SINGLE_DIR / "WWE 2K14 PC.exe"
    launcher_dst = STAGING / "WWE 2K14 PC.exe"
    shutil.copy2(launcher_src, launcher_dst)

    print("Encrypting UI assets directly into bin/ui.dat...")
    ui_src = ROOT / "launcher" / "ui"
    pack_encrypted_ui(launcher_dst, ui_src, bin_dir / "ui.dat")

    print("Copying native game executables and dependencies to bin/...")
    build_dir = ROOT / "out" / "build" / "win-amd64" / "RelWithDebInfo"
    game_files = [
        "wwe2k14.exe",
        "wwe2k14_tu.exe",
        "avcodec-58.dll",
        "avutil-56.dll",
        "achievement.ogg",
    ]
    for gf in game_files:
        src = build_dir / gf
        if not src.exists():
            src = DIST / "launcher" / gf
        if not src.exists() and gf == "achievement.ogg":
            src = ROOT / "res" / gf
        dst = bin_dir / gf
        print(f"  Copying {gf} from {src}")
        shutil.copy2(src, dst)

    # Sanitize PE debug paths in game executables and libraries inside bin/
    sanitize_pe_pdb_path(bin_dir / "wwe2k14.exe", b"wwe2k14.pdb")
    sanitize_pe_pdb_path(bin_dir / "wwe2k14_tu.exe", b"wwe2k14_tu.pdb")
    sanitize_pe_pdb_path(bin_dir / "avcodec-58.dll", b"avcodec-58.pdb")
    sanitize_pe_pdb_path(bin_dir / "avutil-56.dll", b"avutil-56.pdb")

    # Full updated ReadMe!.txt
    readme_text = """==========================================================================
                         WWE 2K14 - PC EDITION
                      PC recompilation by Banshee
==========================================================================

Thanks for supporting the project!

This is WWE 2K14 running natively on Windows. It is not an emulator: the
Xbox 360 game was rebuilt to run on PC, with a launcher that adds modern
options, mods, custom logos, your own music and more.

IMPORTANT: the game itself is NOT included. You need your own copy of the
Xbox 360 version of WWE 2K14 (a disc image .iso, or the extracted game
folder).


--------------------------------------------------------------------------
 WHAT YOU GET
--------------------------------------------------------------------------

 - Runs on PC at 720p, 1080p, 1440p or 4K
 - AMD FidelityFX Super Resolution (FSR) with edge-preserving sharpening
   (RCAS) for high framerates at high resolutions
 - Enhanced NVIDIA FXAA 3.11 Quality Anti-Aliasing (FXAA & Max)
 - Smooth frame pacing, with 60 FPS entrances and victory scenes
 - Windowed, borderless or fullscreen
 - Anisotropic texture filtering (4x, 16x), shadows and an FPS counter
 - Play with an Xbox controller or keyboard
 - Mouse in the menus: point and click menus, wrestler select, button
   prompts, option sliders and Yes/No popups; scroll with the wheel
 - Achievements, with unlock pop-ups
 - Your own music in the game's soundtrack (MP3, WAV, FLAC, OGG)
 - Custom logos, including importing logos from a PS3 save, with a
   logo library so you can keep more than 20 and choose which ones to use
 - Mods (wrestlers, arenas, menus, videos, sounds), turn each one on/off
 - DLC support
 - Screenshots with F12


--------------------------------------------------------------------------
 WHAT YOU NEED
--------------------------------------------------------------------------

 - Windows 10 or 11, 64-bit
 - A graphics card that supports Vulkan (most NVIDIA, AMD and Intel cards
   from the last 10 years). Keep your graphics driver up to date.
 - Microsoft Edge WebView2 Runtime. It is already on Windows 11 and on
   most Windows 10 PCs. If the launcher opens blank, get it here:
       https://developer.microsoft.com/microsoft-edge/webview2/
 - Your own WWE 2K14 for Xbox 360, as an .iso or an extracted folder


--------------------------------------------------------------------------
 INSTALLING
--------------------------------------------------------------------------

 1. Unzip everything into a folder of its own, for example
        C:\\Games\\WWE 2K14 PC
    Don't run it from inside the zip, and avoid "Program Files" (the
    launcher saves your progress next to itself).

 2. Open "WWE 2K14 PC.exe".

    If Windows shows "Windows protected your PC", click "More info" and
    then "Run anyway". This appears because the program is new and not
    signed; it is normal for small projects.

 3. Under GAME FILES, click "Choose ISO" and pick your WWE 2K14 .iso.
    (Or "Choose folder" if you have the game extracted to a folder.)

 4. Pick your settings and press PLAY.

 Don't touch anything inside the "bin" folder; the game uses it.


--------------------------------------------------------------------------
 THE LAUNCHER
--------------------------------------------------------------------------

 GAME FILES     Choose your .iso or game folder.
                "Install update" adds the official title update, if you
                have its files, and "Updated" then plays with it.

 RESOLUTION     720p is the original Xbox 360 look; 1080p and up are
                sharper and need a stronger graphics card.

 GRAPHICS       V-Sync        stops screen tearing (leave it on)
                60 FPS Cinematics  smoother entrances and victory scenes
                Shadows       character and ring shadows
                FPS Counter   shows your framerate in game
                Window Mode   windowed / borderless / fullscreen
                Anisotropic Filtering  sharper textures at an angle
                Anti-Aliasing  smooths jagged edges (FXAA / Max)
                Upscaling (FSR) AMD FidelityFX Super Resolution: renders
                              at internal resolution and upscales to your
                              display for high framerates at high resolution

 Tabs on the right:

 ACHIEVEMENTS   Your progress. Start the game once to load the list.

 DLC            Add official DLC packages you own.

 MODS           Add a mod as a .zip package, a folder or a single file.
                Each mod has its own on/off switch. Mods work with an .iso
                and with a game folder. Close the game before changing
                mods.

 MUSIC          Add your own songs. In the game, choose "User playlist"
                in the soundtrack options.

 LOGOS          Your custom logos (the ones used in the Paint Tool,
                Creation Suite and arenas).
                 - "Add logos": PNG, JPG, BMP or GIF images
                 - "Import PS3 save": brings in logos from a WWE 2K14 PS3
                   save (its LOGOS.DAT, or a .zip / .rar of the save)
                 - "Library": the game holds only 20 logos, but the
                   library keeps all of yours. Click a logo to put it in
                   the game or take it out.
                Note: save one logo in the game's Paint Tool first, so
                the game creates its logo save.

 At the bottom:
 Saves & Data   opens the folder with your saves
 Screenshots    opens your screenshots (press F12 in game to take one)
 Controls       the full controller and keyboard layout


--------------------------------------------------------------------------
 CONTROLS
--------------------------------------------------------------------------

 Controller: an Xbox controller (or any controller Windows sees as one)
 works like on the Xbox 360.

 Keyboard (main keys; the full list is under "Controls" in the launcher):

   Move ................ W A S D  (or arrow keys)
   Run ................. E + move
   Grapple ............. Space or K
   Strike .............. J
   Irish whip / Pin .... L
   Signature/Finisher .. I
   Reverse / Block ..... F or Ctrl
   Apron / Ring ........ Shift
   Pick up objects ..... Q
   Change hold ......... T G V B, numpad 8 2 4 6, or move the mouse
   Taunts .............. 1 2 3 4
   Pause ............... Enter or Esc

 Mouse in menus: point to select, click to choose, wheel to scroll.
 You can also click the on-screen button prompts (like "B Back" or
 "X Add Manager"). Middle click = Start.


--------------------------------------------------------------------------
 WHERE YOUR STUFF IS
--------------------------------------------------------------------------

 Everything is kept next to the launcher:

   userdata\\     saves, settings, logos, logo library, music, DLC
   mods\\         your mods
   logs\\         log files (useful when reporting a problem)

 To move or back up the game, copy the whole folder. To update to a new
 version, unzip the new version over the old one: your "userdata" and
 "mods" folders are kept.


--------------------------------------------------------------------------
 PROBLEMS?
--------------------------------------------------------------------------

 Launcher does not open
   Make sure you unzipped everything first, and that your antivirus didn't
   block "WWE 2K14 PC.exe".

 Launcher opens but is blank / black
   Install the WebView2 Runtime, link above.

 "Not an Xbox 360 disc image"
   The .iso has to be the Xbox 360 version (PS3 isos don't work).

 Game does not start, or closes right away
   - Update your graphics driver.
   - Make sure your antivirus didn't remove or block anything in the
     "bin" folder.
   - Try the game folder instead of the .iso, or the other way round.

 Low framerate
   Lower the resolution (1080p or 720p), turn on FSR, turn off Shadows,
   or set Anti-Aliasing to Off.

 A mod crashes the game
   Turn the mod off in the MODS tab and try again. Some mods made for
   other versions of the game won't work.

 Logos tab says there is no logo save
   Start the game, open Creation Suite > Paint Tool and save any logo
   once. After that the launcher can add logos.

 When reporting a problem, please include:
   - what you did and what happened
   - your graphics card and Windows version
   - the newest file from the "logs" folder


--------------------------------------------------------------------------
 GOOD TO KNOW
--------------------------------------------------------------------------

 - This is an early release. Expect some bugs, and please report them.
 - Online features of the original game are not available.
 - This project is made by a fan, is not affiliated with WWE, 2K or
   Yuke's, and does not include any game files. Please only use a copy of
   the game you own.
 - Built on the open-source ReXGlue recompilation SDK, which is based on
   the Xenia project. Their licences, and those of the other libraries
   used, are in "Third-party licenses.txt".

Enjoy, and thanks again for the support!
"""
    (STAGING / "ReadMe!.txt").write_text(readme_text, encoding="utf-8")
    shutil.copy2(ROOT / "THIRD_PARTY_NOTICES.md", STAGING / "Third-party licenses.txt")

    print("\n--- Verifying release layout ---")
    root_items = sorted(p.name for p in STAGING.iterdir())
    print(f"Root directory items: {root_items}")
    expected_root = ["ReadMe!.txt", "Third-party licenses.txt", "WWE 2K14 PC.exe", "bin"]
    if root_items != expected_root:
        raise RuntimeError(f"Unexpected items in root directory: {root_items} (expected {expected_root})")

    bin_items = sorted(p.name for p in bin_dir.iterdir())
    print(f"bin directory items: {bin_items}")

    print("\n--- Verifying privacy & security in release staging ---")
    leaks_found = 0
    forbidden_terms = DEV_PATHS + [str(Path.home()).encode()]
    if os.environ.get("USERNAME"):
        forbidden_terms.append(os.environ["USERNAME"].encode())

    for root, dirs, files in os.walk(STAGING):
        for file in files:
            file_path = Path(root) / file
            if file.endswith(".pdb") or file.endswith(".xml") or file.endswith(".log") or file.endswith(".deps.json") or file.endswith(".runtimeconfig.json"):
                print(f"ERROR: Disallowed file found: {file_path}")
                leaks_found += 1
            if file.endswith((".html", ".css", ".js")):
                print(f"ERROR: Plaintext web asset found: {file_path} (must be encrypted in ui.dat)")
                leaks_found += 1
            try:
                data = file_path.read_bytes()
                for term in forbidden_terms:
                    if term in data:
                        print(f"ERROR: Forbidden term {term.decode()} found in: {file_path.relative_to(STAGING)}")
                        leaks_found += 1
            except Exception:
                pass

    if leaks_found > 0:
        raise RuntimeError(f"Audit failed with {leaks_found} security/privacy issues! Aborting zip creation.")

    print("Audit passed completely! Zero leaks found.")

    print(f"\nCompressing release to: {ZIP_OUT}...")
    if ZIP_OUT.exists():
        ZIP_OUT.unlink()

    with zipfile.ZipFile(ZIP_OUT, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zipf:
        for root, dirs, files in os.walk(STAGING):
            for file in files:
                file_path = Path(root) / file
                # Direct in zip root, no pre-folder!
                archive_name = file_path.relative_to(STAGING)
                zipf.write(file_path, archive_name)

    zip_size_mb = ZIP_OUT.stat().st_size / (1024 * 1024)
    print(f"SUCCESS: Release archive created at: {ZIP_OUT} ({zip_size_mb:.2f} MB)")


if __name__ == "__main__":
    main()
