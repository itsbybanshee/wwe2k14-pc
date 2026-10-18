"""Writes THIRD_PARTY_NOTICES.md from the licence texts of the SDK and the libraries built into the release.

Run after tools/setup.ps1 has fetched the SDK:  python tools/gen_notices.py
The release packager copies the result into the zip as "Third-party licenses.txt".
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SDK = ROOT / "sdk" / "rexglue"
TP = SDK / "thirdparty"

# (name, licence file, or source file whose header carries the notice). Build-time / test-only code (the code
# generator's disassembler, catch2) is not part of the release and is left out.
LIBRARIES = [
    ("FFmpeg (avcodec-58.dll, avutil-56.dll) - LGPL 2.1", TP / "FFmpeg" / "COPYING.LGPLv2.1"),
    ("fmt", TP / "fmt" / "LICENSE"),
    ("spdlog", TP / "spdlog" / "LICENSE"),
    ("Dear ImGui", TP / "imgui" / "LICENSE.txt"),
    ("SDL3", TP / "sdl3" / "LICENSE.txt"),
    ("volk", TP / "volk" / "LICENSE.md"),
    ("Vulkan Headers", TP / "vulkan-headers" / "LICENSE.md"),
    ("Vulkan Memory Allocator", TP / "vulkan-memory-allocator" / "LICENSE.txt"),
    ("glslang", TP / "glslang" / "LICENSE.txt"),
    ("SPIRV-Tools", TP / "spirv-tools" / "LICENSE"),
    ("SPIRV-Headers", TP / "spirv-headers" / "LICENSE"),
    ("toml++", TP / "tomlplusplus" / "LICENSE"),
    ("CLI11", TP / "cli11" / "LICENSE"),
    ("inja", TP / "inja" / "LICENSE"),
    ("xxHash", TP / "xxHash" / "LICENSE"),
    ("Snappy", TP / "snappy" / "COPYING"),
    ("SIMDe", TP / "simde" / "COPYING"),
    ("UTF8-CPP", TP / "utfcpp" / "LICENSE"),
    ("o1heap", TP / "o1heap" / "LICENSE"),
    ("Tracy", TP / "tracy" / "LICENSE"),
    ("aes_128", TP / "aes_128" / "LICENSE"),
    ("DES (crypto)", TP / "crypto" / "des" / "LICENSE"),
    ("libmspack - LGPL 2.1", TP / "libmspack" / "libmspack" / "COPYING.LIB"),
    ("dr_flac", TP / "decoders" / "dr_flac.h"),
    ("PicoSHA2", TP / "picosha2" / "picosha2.h"),
    ("RenderDoc API header", TP / "renderdoc" / "renderdoc_app.h"),
    ("DXBC checksum", TP / "dxbc" / "DXBCChecksum.cpp"),
    ("DirectX Shader Compiler headers", TP / "dxc" / "include" / "DxbcConverter.h"),
]

WEBVIEW2 = sorted((Path.home() / ".nuget" / "packages" / "microsoft.web.webview2").glob("*/LICENSE.txt"))


def notice(path: Path) -> str:
    text = path.read_text(encoding="utf-8", errors="replace")
    if path.suffix.lower() in (".h", ".c", ".cpp"):
        # Source file: keep its leading comment block, where the notice is.
        lines = []
        for line in text.splitlines():
            if lines and not line.strip().startswith(("/", "*", "//")) and line.strip():
                break
            lines.append(line)
        header = "\n".join(lines[:80])
        if "copyright" in header.lower() or "licen" in header.lower():
            return header.strip()
        # Otherwise the notice is further down (e.g. a licence block at the end of a single-header library).
        all_lines = text.splitlines()
        at = next(i for i, l in enumerate(all_lines) if "licen" in l.lower() or "copyright" in l.lower())
        return "\n".join(all_lines[max(0, at - 2):at + 40]).strip()
    return text.strip()


def main() -> None:
    out = [
        "# Third-party notices",
        "",
        "WWE 2K14 PC is built on the ReXGlue recompilation SDK, which is derived from the Xenia project, plus this",
        "project's changes in `patches/sdk/`. The licences below require their notices to be kept with the source and",
        "with any binary release, so they are reproduced here unchanged.",
        "",
        "WWE 2K14, its executable code and its assets are the property of their respective owners and are not covered",
        "by any licence in this file. Players supply their own copy of the game.",
        "",
        "FFmpeg is used as separate DLLs (avcodec-58.dll, avutil-56.dll) under the LGPL 2.1; its source code is",
        "available from https://ffmpeg.org and in the ReXGlue SDK.",
        "",
        "## ReXGlue SDK (includes Xenia)",
        "",
        "```",
        notice(SDK / "LICENSE"),
        "```",
    ]
    for name, path in LIBRARIES:
        if not path.exists():
            raise SystemExit(f"missing licence file: {path}")
        out += ["", f"## {name}", "", "```", notice(path), "```"]
    out += ["", "## Xbox 360 button icons (launcher)", "",
            "launcher/ui/icons/*.svg: \"Xbox button A/B/X/Y\", \"Xbox Left/Right Bumper\", \"Xbox Left/Right Trigger\",",
            "\"Xbox Start button\", \"Xbox Back button\" and \"Xbox Left/Right stick button\" by VictorPines, from Wikimedia",
            "Commons (https://commons.wikimedia.org/wiki/Category:Xbox_buttons), licensed under CC BY-SA 4.0",
            "(https://creativecommons.org/licenses/by-sa/4.0/). Used unchanged.",
            "icons/up, down, left, right, dpad.svg: D-pad icons by Banshee.",
            "controller.svg: \"Xbox Controller\" by Jishenaz, Wikimedia Commons, CC0 (public domain dedication)."]
    if WEBVIEW2:
        out += ["", "## Microsoft Edge WebView2 SDK (launcher)", "", "```", notice(WEBVIEW2[-1]), "```"]
    (ROOT / "THIRD_PARTY_NOTICES.md").write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"THIRD_PARTY_NOTICES.md: {len(LIBRARIES) + 2} entries")


if __name__ == "__main__":
    main()
