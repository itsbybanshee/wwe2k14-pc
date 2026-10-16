using System.Diagnostics;

namespace Wwe2k14Launcher;

// Optional title update. The game always plays vanilla; a title update is an extra, chosen in the launcher.
//
// A title update package (e.g. tu00000002_00000000) holds a code patch (default.xexp) plus data files. The code
// patch can't be applied to the recompiled exe: the game code is compiled into it. So the update gets its own exe,
// wwe2k14_tu.exe, built once from your default.xex with the patch applied (tools\build-tu.ps1). At run time that exe
// mounts the extracted package as update:\ (--update_data_root), where the game finds its patched data files and the
// runtime finds default.xexp for the patched data sections.
//
//   <user data>\title_update\            extracted package contents (default.xexp, m3_patch.pac, ...)
//   <user data>\title_update\.installed   display name of the installed update
public sealed class TitleUpdate
{
    const uint TitleUpdateContent = 0x000B0000;

    readonly LauncherPaths _paths;

    public TitleUpdate(LauncherPaths paths) => _paths = paths;

    public string Folder => Path.Combine(_paths.UserData, "title_update");
    string Marker => Path.Combine(Folder, ".installed");

    public bool Installed => File.Exists(Marker) && File.Exists(Path.Combine(Folder, "default.xexp"));
    public string Name => Installed ? File.ReadAllText(Marker).Trim() : "";
    public bool ExeBuilt => File.Exists(_paths.TitleUpdateExe);
    public bool Ready => Installed && ExeBuilt;

    // Extracts the package to the title_update folder. Returns an error message, or null on success.
    public string? Install(string packagePath, Action<long, long>? progress = null)
    {
        string staging = Folder + ".installing";
        try
        {
            using var package = StfsPackage.Open(packagePath);
            if (package.ContentType != TitleUpdateContent)
                return package.ContentType == StfsPackage.MarketplaceContent
                    ? "This is DLC, not a title update: add it in the DLC tab."
                    : $"Not a title update (content type {package.ContentType:X8}).";
            if (package.TitleId != DlcStore.TitleId)
                return $"This title update is for another game ({(package.TitleName.Length > 0 ? package.TitleName : package.TitleId.ToString("X8"))}).";
            if (!package.ReadFileTable().Any(e => e.Name.Equals("default.xexp", StringComparison.OrdinalIgnoreCase)))
                return "This package has no default.xexp: not a WWE 2K14 title update.";

            if (Directory.Exists(staging)) Directory.Delete(staging, true);
            package.ExtractTo(staging, progress);
            string name = package.DisplayName.Length > 0 ? package.DisplayName : Path.GetFileName(packagePath);
            File.WriteAllText(Path.Combine(staging, ".installed"), $"{name} ({Path.GetFileName(packagePath)})");
            if (Directory.Exists(Folder)) Directory.Delete(Folder, true);
            Directory.Move(staging, Folder);
            return null;
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or NotSupportedException
                                       or UnauthorizedAccessException)
        {
            try { if (Directory.Exists(staging)) Directory.Delete(staging, true); } catch (IOException) { }
            return ex.Message;
        }
    }

    public void Remove()
    {
        if (Directory.Exists(Folder)) Directory.Delete(Folder, true);
    }

    // Builds wwe2k14_tu.exe in a console window (needs the development tools, like the first build). Only possible
    // from the repository.
    public Process? StartBuild()
    {
        string script = Path.Combine(_paths.Root, "tools", "build-tu.ps1");
        if (!File.Exists(script)) return null;
        var info = new ProcessStartInfo("powershell.exe") { UseShellExecute = false, WorkingDirectory = _paths.Root };
        foreach (var a in new[] { "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-UpdateFolder", Folder })
            info.ArgumentList.Add(a);
        return Process.Start(info);
    }
}
