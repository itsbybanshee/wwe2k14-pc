using System.Buffers.Binary;
using System.Text;
using System.Text.RegularExpressions;

namespace Wwe2k14Launcher;

// Handles 1-click installation of WWE 2K14 Total Conversion mods (e.g. Wrestling Legends 1.1).
// Automatically discovers the standalone game directory, Title Update requirements,
// STFS DLC packages (content type 00000002), and STFS Save packages (content type 00000001).
public sealed class TotalConversion
{
    const string WrestlingLegends = "Wrestling Legends";

    readonly LauncherPaths _paths;
    readonly DlcStore _dlc;

    public TotalConversion(LauncherPaths paths, DlcStore dlc)
    {
        _paths = paths;
        _dlc = dlc;
    }

    public sealed record Result(
        bool Ok,
        string Message,
        string? GameFolder = null,
        int DlcInstalled = 0,
        int SavesInstalled = 0,
        List<string>? DlcAdded = null
    );

    public Result Install(string sourceFolder, Action<string, int, int>? onProgress = null)
    {
        if (string.IsNullOrWhiteSpace(sourceFolder) || !Directory.Exists(sourceFolder))
            return new(false, "Selected folder does not exist.");

        onProgress?.Invoke("Scanning mod folder...", 0, 100);

        // 1. Locate game directory containing default.xex and pac assets
        string? gameFolder = FindGameFolder(sourceFolder);

        // 2. Discover DLC packages
        var dlcFiles = FindDlcPackages(sourceFolder);
        if (gameFolder != null && dlcFiles.Count == 0) dlcFiles = FindDlcNextTo(sourceFolder);

        // 3. Discover Save packages
        var saveFiles = FindSavePackages(sourceFolder);

        if (gameFolder == null && dlcFiles.Count == 0 && saveFiles.Count == 0)
        {
            return new(false, "No valid WWE 2K14 game files, DLC packages, or save data found in the selected folder.");
        }

        int dlcInstalled = 0;
        int savesInstalled = 0;
        var dlcBefore = _dlc.List().Select(d => d.Name).ToHashSet(StringComparer.OrdinalIgnoreCase);

        // 4. Install DLC packages
        int totalItems = dlcFiles.Count + saveFiles.Count;
        int currentItem = 0;
        string? dlcError = null;

        for (int i = 0; i < dlcFiles.Count; i++)
        {
            var dlcPath = dlcFiles[i];
            string fileName = Path.GetFileName(dlcPath);
            onProgress?.Invoke($"Installing DLC ({i + 1}/{dlcFiles.Count}): {fileName}", currentItem++, totalItems);
            var res = _dlc.Install(dlcPath);
            if (res.Ok) dlcInstalled++;
            else dlcError = $"{fileName}: {res.Message}";
        }

        // 5. Install Save packages (00000001)
        if (saveFiles.Count > 0)
        {
            string saveRoot = Path.Combine(_paths.UserData, "B13EBABEBABEBABE", "545408B2", "00000001");
            string headerRoot = Path.Combine(_paths.UserData, "B13EBABEBABEBABE", "545408B2", "Headers", "00000001");
            string backupDir = Path.Combine(_paths.UserData, "backups", "vanilla_save");

            // Backup existing saves if backup doesn't already exist
            if (Directory.Exists(saveRoot) && !Directory.Exists(backupDir))
            {
                try
                {
                    Directory.CreateDirectory(backupDir);
                    foreach (var dir in Directory.EnumerateDirectories(saveRoot))
                    {
                        string dest = Path.Combine(backupDir, Path.GetFileName(dir));
                        CopyDirectory(dir, dest);
                    }
                    foreach (var file in Directory.EnumerateFiles(saveRoot))
                    {
                        File.Copy(file, Path.Combine(backupDir, Path.GetFileName(file)), true);
                    }
                }
                catch { }
            }

            Directory.CreateDirectory(saveRoot);
            Directory.CreateDirectory(headerRoot);

            for (int i = 0; i < saveFiles.Count; i++)
            {
                var savePath = saveFiles[i];
                string fileName = Path.GetFileName(savePath);
                onProgress?.Invoke($"Extracting save data ({i + 1}/{saveFiles.Count}): {fileName}", currentItem++, totalItems);

                try
                {
                    using var pkg = StfsPackage.Open(savePath);
                    string targetFolder = Path.Combine(saveRoot, fileName);
                    string targetHeader = Path.Combine(headerRoot, fileName + ".header");

                    if (Directory.Exists(targetFolder))
                    {
                        Directory.Delete(targetFolder, true);
                    }
                    Directory.CreateDirectory(targetFolder);

                    pkg.ExtractTo(targetFolder);

                    byte[] header = BuildSaveHeader(fileName, pkg.DisplayName);
                    File.WriteAllBytes(targetHeader, header);

                    savesInstalled++;
                }
                catch (Exception ex)
                {
                    // Continue with remaining saves
                    System.Diagnostics.Debug.WriteLine($"Failed to install save {fileName}: {ex.Message}");
                }
            }
        }

        onProgress?.Invoke("Finishing up...", totalItems, totalItems);

        var summary = new StringBuilder("Total Conversion installed!");
        if (gameFolder != null)
            summary.Append(DivasEvolution.IsGameFolder(gameFolder)
                ? " Configured game folder (Divas Evolution, its own build of the game)."
                : " Configured game folder & Title Update.");
        if (dlcInstalled > 0)
            summary.Append($" Installed {dlcInstalled} DLCs.");
        if (savesInstalled > 0)
            summary.Append($" Extracted {savesInstalled} save packages.");
        if (dlcError != null)
            summary.Append($" {dlcFiles.Count - dlcInstalled} DLC packages failed ({dlcError}).");
        else if (dlcFiles.Count == 0 && gameFolder != null)
            summary.Append(" No DLC found: add the mod's DLC in the DLC tab.");

        var dlcAdded = _dlc.List().Select(d => d.Name).Where(name => !dlcBefore.Contains(name)).ToList();
        return new(true, summary.ToString(), gameFolder, dlcInstalled, savesInstalled, dlcAdded);
    }

    // The name to show for the selected game folder, or null when it isn't a total conversion.
    public static string? ActiveName(LauncherSettings settings) => ActiveName(settings, settings.GameFolder);

    static string? ActiveName(LauncherSettings settings, string folder)
    {
        if (folder.Length == 0) return null;
        if (!folder.EndsWith(".iso", StringComparison.OrdinalIgnoreCase) && DivasEvolution.IsGameFolder(folder)) return DivasEvolution.Name;
        if (settings.TotalConversionName.Length > 0 && folder.Equals(settings.TotalConversionFolder, StringComparison.OrdinalIgnoreCase))
            return settings.TotalConversionName;
        // Installed before the launcher remembered the name.
        return folder.Contains(WrestlingLegends, StringComparison.OrdinalIgnoreCase) ? WrestlingLegends : null;
    }

    // A mod's name from the folder it was installed from: "Wrestling Legends 1.1" -> "Wrestling Legends".
    public static string NameFromFolder(string sourceFolder, string gameFolder)
    {
        string name = Path.GetFileName(sourceFolder.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
        foreach (string known in new[] { WrestlingLegends, DivasEvolution.Name })
        {
            if (name.Contains(known, StringComparison.OrdinalIgnoreCase) || gameFolder.Contains(known, StringComparison.OrdinalIgnoreCase))
                return known;
        }
        return Regex.Replace(name, @"[\s_-]*\(?v?\d+(\.\d+)*\)?$", "", RegexOptions.IgnoreCase).Trim();
    }

    // The game to go back to: the first remembered game that still exists and isn't itself a total conversion.
    public static string? FindVanillaGame(LauncherSettings settings, LauncherPaths paths)
    {
        string legacyFile = Path.Combine(paths.UserData, "backups", "vanilla_save", "game_folder.txt");
        string legacy = "";
        try { if (File.Exists(legacyFile)) legacy = File.ReadAllText(legacyFile).Trim(); }
        catch (IOException) { }
        string commonDisc = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads", "xiso", "WWE 2K14 (USA, Europe)");
        return new[] { settings.VanillaGameFolder, settings.PreviousGameFolder, legacy, paths.DefaultGameFolder, commonDisc }
            .FirstOrDefault(game => game.Length > 0 && (File.Exists(game) || Directory.Exists(game)) && ActiveName(settings, game) is null);
    }

    // Undoes what Install did: the saves it replaced and the DLC it added. Saves the mod didn't replace stay.
    public bool RestoreVanilla(LauncherSettings settings, out string message)
    {
        string backupDir = Path.Combine(_paths.UserData, "backups", "vanilla_save");
        string saveRoot = Path.Combine(_paths.UserData, "B13EBABEBABEBABE", "545408B2", "00000001");

        int dlcRemoved = 0;
        try
        {
            foreach (var item in _dlc.List())
            {
                // Mods installed before the launcher tracked their DLC: Divas Evolution's own package is recognisable.
                if (settings.TotalConversionDlc.Contains(item.Name, StringComparer.OrdinalIgnoreCase) ||
                    item.Name.Contains(DivasEvolution.Name, StringComparison.OrdinalIgnoreCase) ||
                    item.DisplayName.Contains(DivasEvolution.Name, StringComparison.OrdinalIgnoreCase))
                {
                    _dlc.Remove(item.Name);
                    dlcRemoved++;
                }
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
        {
            message = $"Could not remove the mod's DLC: {ex.Message}";
            return false;
        }
        settings.TotalConversionDlc.Clear();
        settings.TotalConversionName = "";
        settings.TotalConversionFolder = "";

        int restored = 0;
        // Only an actual save backup may replace the saves (older launchers also kept just game_folder.txt here).
        bool hasSaveBackup = Directory.Exists(backupDir) &&
            Directory.EnumerateFileSystemEntries(backupDir).Any(entry => !Path.GetFileName(entry).Equals("game_folder.txt", StringComparison.OrdinalIgnoreCase));
        if (hasSaveBackup)
        {
            try
            {
                // Remove existing mod saves
                if (Directory.Exists(saveRoot))
                {
                    Directory.Delete(saveRoot, true);
                }
                Directory.CreateDirectory(saveRoot);

                foreach (var dir in Directory.EnumerateDirectories(backupDir))
                {
                    string dest = Path.Combine(saveRoot, Path.GetFileName(dir));
                    CopyDirectory(dir, dest);
                    restored++;
                }
                foreach (var file in Directory.EnumerateFiles(backupDir))
                {
                    if (string.Equals(Path.GetFileName(file), "game_folder.txt", StringComparison.OrdinalIgnoreCase)) continue;
                    File.Copy(file, Path.Combine(saveRoot, Path.GetFileName(file)), true);
                    restored++;
                }
            }
            catch (Exception ex)
            {
                message = $"Could not restore vanilla saves: {ex.Message}";
                return false;
            }
        }
        // Used up: the next total conversion backs up the saves as they are then.
        try { if (Directory.Exists(backupDir)) Directory.Delete(backupDir, true); }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }

        message = restored > 0
            ? $"Restored {restored} vanilla save items and switched to Vanilla version."
            : "Switched back to Vanilla version.";
        if (dlcRemoved > 0) message += $" Removed {dlcRemoved} DLC packages of the mod.";
        return true;
    }

    static string? FindGameFolder(string root)
    {
        if (IsGameFolder(root)) return root;

        var queue = new Queue<(string Path, int Depth)>();
        queue.Enqueue((root, 0));

        while (queue.Count > 0)
        {
            var (current, depth) = queue.Dequeue();
            if (depth > 3) continue;

            try
            {
                foreach (var sub in Directory.EnumerateDirectories(current))
                {
                    if (IsGameFolder(sub)) return sub;
                    queue.Enqueue((sub, depth + 1));
                }
            }
            catch { }
        }

        return null;
    }

    static bool IsGameFolder(string dir)
    {
        return File.Exists(Path.Combine(dir, "default.xex")) &&
               (Directory.Exists(Path.Combine(dir, "pac")) || File.Exists(Path.Combine(dir, "plist360.arc")));
    }

    static List<string> FindDlcPackages(string root)
    {
        var result = new List<string>();
        try
        {
            foreach (var file in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
            {
                string dir = Path.GetFileName(Path.GetDirectoryName(file) ?? "");
                string parentDir = Path.GetFileName(Path.GetDirectoryName(Path.GetDirectoryName(file) ?? "") ?? "");

                bool inDlc = dir.Equals("00000002", StringComparison.OrdinalIgnoreCase) ||
                             dir.Equals("DLC", StringComparison.OrdinalIgnoreCase) ||
                             parentDir.Equals("DLC", StringComparison.OrdinalIgnoreCase);

                if (inDlc && StfsPackage.HasPackageMagic(file))
                {
                    result.Add(file);
                }
            }
        }
        catch { }

        // Fallback: check all packages if none found in DLC directory
        if (result.Count == 0)
        {
            try
            {
                foreach (var file in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
                {
                    if (StfsPackage.HasPackageMagic(file))
                    {
                        try
                        {
                            using var pkg = StfsPackage.Open(file);
                            if (pkg.ContentType == StfsPackage.MarketplaceContent)
                                result.Add(file);
                        }
                        catch { }
                    }
                }
            }
            catch { }
        }

        return result;
    }

    // Mods can ship their DLC as a separate download that ends up beside the mod's folder (Divas Evolution's
    // "Downloadable Content"). Looks up to two levels above the selected folder, as it may be the inner game folder.
    static List<string> FindDlcNextTo(string sourceFolder)
    {
        var dir = new DirectoryInfo(sourceFolder).Parent;
        for (int level = 0; level < 2 && dir != null; level++, dir = dir.Parent)
        {
            try
            {
                var folders = dir.GetDirectories();
                var matches = folders.Where(f => f.Name.Contains("Downloadable Content", StringComparison.OrdinalIgnoreCase)).ToList();
                if (matches.Count == 0)
                    matches = folders.Where(f => f.Name.Split(' ', '_', '-', '.').Contains("DLC", StringComparer.OrdinalIgnoreCase)).ToList();
                var packages = matches.SelectMany(f => FindDlcPackages(f.FullName)).ToList();
                if (packages.Count > 0) return packages;
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        }
        return [];
    }

    static List<string> FindSavePackages(string root)
    {
        var result = new List<string>();
        try
        {
            foreach (var file in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
            {
                string dir = Path.GetFileName(Path.GetDirectoryName(file) ?? "");
                string parentDir = Path.GetFileName(Path.GetDirectoryName(Path.GetDirectoryName(file) ?? "") ?? "");

                bool inSave = dir.Equals("00000001", StringComparison.OrdinalIgnoreCase) ||
                              dir.Equals("SAVE", StringComparison.OrdinalIgnoreCase) ||
                              parentDir.Equals("SAVE", StringComparison.OrdinalIgnoreCase);

                if (inSave && StfsPackage.HasPackageMagic(file))
                {
                    result.Add(file);
                }
            }
        }
        catch { }

        if (result.Count == 0)
        {
            try
            {
                foreach (var file in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
                {
                    if (StfsPackage.HasPackageMagic(file))
                    {
                        try
                        {
                            using var pkg = StfsPackage.Open(file);
                            if (pkg.ContentType == 0x00000001) // SavedGame
                                result.Add(file);
                        }
                        catch { }
                    }
                }
            }
            catch { }
        }

        return result;
    }

    // Creates the 328-byte (0x148) XCONTENT_AGGREGATE_DATA header matching ReXGlue runtime specifications.
    static byte[] BuildSaveHeader(string name, string displayName)
    {
        var h = new byte[0x148];
        BinaryPrimitives.WriteUInt32BigEndian(h.AsSpan(0x00), 1);   // DummyDeviceId::HDD
        BinaryPrimitives.WriteUInt32BigEndian(h.AsSpan(0x04), 1);   // XContentType::kSavedGame

        // ReXGlue requires display names for saved games to contain a dot ('.') to safely parse sub-strings
        string display = (displayName.Contains('.') && displayName.Length > 0) ? displayName : name;
        var displayBytes = Encoding.BigEndianUnicode.GetBytes(display);
        Array.Copy(displayBytes, 0, h, 0x08, Math.Min(displayBytes.Length, 0xFE));

        var fileBytes = Encoding.ASCII.GetBytes(name);
        Array.Copy(fileBytes, 0, h, 0x108, Math.Min(fileBytes.Length, 42));

        BinaryPrimitives.WriteUInt64BigEndian(h.AsSpan(0x138), 0);
        BinaryPrimitives.WriteUInt32BigEndian(h.AsSpan(0x140), DlcStore.TitleId);
        return h;
    }

    static void CopyDirectory(string sourceDir, string destinationDir)
    {
        Directory.CreateDirectory(destinationDir);
        foreach (var file in Directory.EnumerateFiles(sourceDir))
        {
            File.Copy(file, Path.Combine(destinationDir, Path.GetFileName(file)), true);
        }
        foreach (var sub in Directory.EnumerateDirectories(sourceDir))
        {
            CopyDirectory(sub, Path.Combine(destinationDir, Path.GetFileName(sub)));
        }
    }
}
