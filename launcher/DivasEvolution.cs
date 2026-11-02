using System.Diagnostics;
using System.Security.Cryptography;

namespace Wwe2k14Launcher;

// The Divas Evolution mod (v1.0.0, by @stqned). Unlike data-only total conversions it patches the game's code, which
// the recompiled exe can't pick up from the mod's default.xex. So the mod gets its own exe, wwe2k14_divas.exe, built
// once from the mod's default.xex (tools\build-divas.ps1), like the title update's. The mod installs with the Total
// Conversion button like any other (its game folder and DLC); the launcher recognises its default.xex and plays it
// with that exe.
public sealed class DivasEvolution
{
    const string Sha256 = "9E4B720A4167A435F45F5E8B6D275012B513034319722F3BDDE6DD74C9B46931";

    readonly LauncherPaths _paths;

    public DivasEvolution(LauncherPaths paths) => _paths = paths;

    public string Exe => Path.Combine(Path.GetDirectoryName(_paths.GameExe)!, "wwe2k14_divas.exe");
    public bool ExeBuilt => File.Exists(Exe);

    // Hashing 24 MB on every state refresh is too slow: remember the answer per file version.
    static (string Path, long Length, DateTime Written, bool IsDivas)? _last;

    // True when the game folder holds the Divas Evolution default.xex.
    public static bool IsGameFolder(string folder)
    {
        string xex = Path.Combine(folder, "default.xex");
        try
        {
            var info = new FileInfo(xex);
            if (!info.Exists) return false;
            if (_last is { } c && c.Path == xex && c.Length == info.Length && c.Written == info.LastWriteTimeUtc) return c.IsDivas;
            bool isDivas;
            using (var stream = info.OpenRead())
                isDivas = Convert.ToHexString(SHA256.HashData(stream)) == Sha256;
            _last = (xex, info.Length, info.LastWriteTimeUtc, isDivas);
            return isDivas;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return false;
        }
    }

    // Builds wwe2k14_divas.exe in a console window (needs the development tools, like the first build). Only possible
    // from the repository.
    public Process? StartBuild(string modFolder)
    {
        string script = Path.Combine(_paths.Root, "tools", "build-divas.ps1");
        if (!File.Exists(script)) return null;
        var info = new ProcessStartInfo("powershell.exe") { UseShellExecute = false, WorkingDirectory = _paths.Root };
        foreach (var a in new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-ModFolder", modFolder })
            info.ArgumentList.Add(a);
        return Process.Start(info);
    }
}
