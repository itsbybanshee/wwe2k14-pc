using System.Buffers.Binary;
using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace Wwe2k14Launcher;

// Manages non-destructive mods for WWE 2K14.
// Mods live in <Root>\mods\<ModName>\ as loose or packaged assets (e.g. pac\ch\ch*.pac, pac\arena\*, etc.).
//
// When the game starts, enabled mods are staged into <UserData>\mods_active\ (via instant NTFS hardlinks or copy)
// and mounted as update:\ via --update_data_root. ReXGlue's VFS prioritizes update:\ over game:\, so modded assets
// override the base game files natively without modifying the original game installation.
public sealed class ModStore
{
    readonly LauncherPaths _paths;
    readonly string _modsFolder;
    readonly string _manifestPath;

    public ModStore(LauncherPaths paths)
    {
        _paths = paths;
        _modsFolder = paths.ModsFolder;
        _manifestPath = Path.Combine(paths.UserData, "mods.json");
        Directory.CreateDirectory(_modsFolder);
    }

    public string Folder => _modsFolder;

    public sealed record InstalledMod(
        string Id,
        string Name,
        bool Enabled,
        string RelativePath,
        long SizeBytes,
        int FileCount,
        string Summary
    );

    public sealed record ModInstallResult(string File, bool Ok, string Message, string? DisplayName = null);

    private sealed class ModManifestItem
    {
        public string Id { get; set; } = "";
        public bool Enabled { get; set; } = true;
    }

    private Dictionary<string, bool> LoadManifest()
    {
        var dict = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
        try
        {
            if (File.Exists(_manifestPath))
            {
                var items = JsonSerializer.Deserialize<List<ModManifestItem>>(File.ReadAllText(_manifestPath));
                if (items != null)
                {
                    foreach (var it in items)
                    {
                        dict[it.Id] = it.Enabled;
                    }
                }
            }
        }
        catch (Exception ex) when (ex is IOException or JsonException) { }
        return dict;
    }

    private void SaveManifest(Dictionary<string, bool> dict)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_manifestPath)!);
            var list = dict.Select(kv => new ModManifestItem { Id = kv.Key, Enabled = kv.Value }).ToList();
            File.WriteAllText(_manifestPath, JsonSerializer.Serialize(list, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (IOException) { }
    }

    public List<InstalledMod> List()
    {
        var list = new List<InstalledMod>();
        if (!Directory.Exists(_modsFolder))
        {
            Directory.CreateDirectory(_modsFolder);
            return list;
        }

        var manifest = LoadManifest();
        bool manifestChanged = false;

        // 1. Scan subdirectories (packaged mod folders)
        foreach (var dir in Directory.EnumerateDirectories(_modsFolder))
        {
            string name = Path.GetFileName(dir);
            if (name.StartsWith('.') || name.Equals("active", StringComparison.OrdinalIgnoreCase))
                continue;

            int fileCount = 0;
            long sizeBytes = 0;
            bool hasChar = false, hasArena = false, hasMenu = false, hasVideo = false, hasAudio = false;

            try
            {
                var files = new DirectoryInfo(dir).EnumerateFiles("*", SearchOption.AllDirectories);
                foreach (var f in files)
                {
                    fileCount++;
                    sizeBytes += f.Length;
                    string fn = f.Name.ToLowerInvariant();
                    string rel = Path.GetRelativePath(dir, f.FullName).ToLowerInvariant();

                    if (fn.StartsWith("ch") && fn.EndsWith(".pac")) hasChar = true;
                    if (fn.StartsWith("arena") || rel.Contains("arena")) hasArena = true;
                    if (fn.StartsWith("menu") || fn.StartsWith("title") || rel.Contains("menu")) hasMenu = true;
                    if (fn.EndsWith(".bik") || rel.Contains("movies")) hasVideo = true;
                    if (fn.EndsWith(".xma") || fn.EndsWith(".wav") || fn.EndsWith(".mp3") || fn.EndsWith(".ogg")) hasAudio = true;
                }
            }
            catch (IOException) { }

            if (fileCount == 0)
                continue;

            string summary = BuildSummary(hasChar, hasArena, hasMenu, hasVideo, hasAudio);
            string id = name;

            if (!manifest.TryGetValue(id, out bool enabled))
            {
                enabled = true; // Enabled by default on discovery
                manifest[id] = true;
                manifestChanged = true;
            }

            list.Add(new InstalledMod(id, CleanDisplayName(name), enabled, name, sizeBytes, fileCount, summary));
        }

        // 2. Scan loose files in root mods/ (e.g. single .pac dropped in)
        foreach (var file in Directory.EnumerateFiles(_modsFolder))
        {
            string fn = Path.GetFileName(file);
            if (fn.StartsWith('.') || fn.Equals("mods.json", StringComparison.OrdinalIgnoreCase))
                continue;

            string ext = Path.GetExtension(fn).ToLowerInvariant();
            if (ext is not (".pac" or ".bik" or ".epk"))
                continue;

            long sizeBytes = 0;
            try { sizeBytes = new FileInfo(file).Length; } catch (IOException) { }

            string id = fn;
            if (!manifest.TryGetValue(id, out bool enabled))
            {
                enabled = true;
                manifest[id] = true;
                manifestChanged = true;
            }

            string summary = ext == ".bik" ? "Video / Titantron" : "Standalone Asset";
            list.Add(new InstalledMod(id, fn, enabled, fn, sizeBytes, 1, summary));
        }

        if (manifestChanged)
        {
            SaveManifest(manifest);
        }

        return list.OrderBy(m => m.Name, StringComparer.OrdinalIgnoreCase).ToList();
    }

    private static string CleanDisplayName(string raw)
    {
        return raw.Replace('_', ' ').Trim();
    }

    private static string BuildSummary(bool ch, bool arena, bool menu, bool video, bool audio)
    {
        var tags = new List<string>();
        if (ch) tags.Add("Superstars");
        if (arena) tags.Add("Arenas");
        if (menu) tags.Add("UI & Menus");
        if (video) tags.Add("Titantrons");
        if (audio) tags.Add("Audio");
        return tags.Count > 0 ? string.Join(" · ", tags) : "Game Assets";
    }

    public ModInstallResult InstallZip(string zipPath, Action<long, long>? progress = null)
    {
        string fileName = Path.GetFileName(zipPath);
        string modName = Path.GetFileNameWithoutExtension(zipPath);
        string targetDir = Path.Combine(_modsFolder, modName);
        string stagingDir = targetDir + ".installing";

        try
        {
            using var archive = ZipFile.OpenRead(zipPath);
            long totalBytes = archive.Entries.Sum(e => e.Length);
            long extractedBytes = 0;

            if (Directory.Exists(stagingDir))
                Directory.Delete(stagingDir, true);
            Directory.CreateDirectory(stagingDir);

            int entryIndex = 0;
            foreach (var entry in archive.Entries)
            {
                entryIndex++;
                if (string.IsNullOrEmpty(entry.Name))
                    continue; // Skip directory entries

                // Normalize path and protect against path traversal
                string relPath = entry.FullName.Replace('/', Path.DirectorySeparatorChar);
                string destPath = Path.GetFullPath(Path.Combine(stagingDir, relPath));
                if (!destPath.StartsWith(Path.GetFullPath(stagingDir), StringComparison.OrdinalIgnoreCase))
                    continue;

                Directory.CreateDirectory(Path.GetDirectoryName(destPath)!);
                entry.ExtractToFile(destPath, overwrite: true);
                extractedBytes += entry.Length;
                progress?.Invoke(extractedBytes, totalBytes);
            }

            if (Directory.Exists(targetDir))
                Directory.Delete(targetDir, true);
            Directory.Move(stagingDir, targetDir);

            var manifest = LoadManifest();
            manifest[modName] = true;
            SaveManifest(manifest);

            return new ModInstallResult(fileName, true, "Mod installed successfully.", CleanDisplayName(modName));
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException)
        {
            try { if (Directory.Exists(stagingDir)) Directory.Delete(stagingDir, true); } catch (IOException) { }
            return new ModInstallResult(fileName, false, $"Installation failed: {ex.Message}");
        }
    }

    public ModInstallResult InstallFolder(string sourceFolder)
    {
        string folderName = Path.GetFileName(sourceFolder.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
        string targetDir = Path.Combine(_modsFolder, folderName);

        try
        {
            if (!Directory.Exists(sourceFolder))
                return new ModInstallResult(folderName, false, "Source folder does not exist.");

            CopyDirectory(sourceFolder, targetDir);

            var manifest = LoadManifest();
            manifest[folderName] = true;
            SaveManifest(manifest);

            return new ModInstallResult(folderName, true, "Mod folder imported.", CleanDisplayName(folderName));
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return new ModInstallResult(folderName, false, $"Import failed: {ex.Message}");
        }
    }

    public ModInstallResult InstallLooseFile(string sourceFilePath)
    {
        string fileName = Path.GetFileName(sourceFilePath);
        string targetFile = Path.Combine(_modsFolder, fileName);
        try
        {
            File.Copy(sourceFilePath, targetFile, overwrite: true);
            var manifest = LoadManifest();
            manifest[fileName] = true;
            SaveManifest(manifest);
            return new ModInstallResult(fileName, true, "File added to mods.", fileName);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return new ModInstallResult(fileName, false, ex.Message);
        }
    }

    public void SetEnabled(string id, bool enabled)
    {
        var manifest = LoadManifest();
        manifest[id] = enabled;
        SaveManifest(manifest);
    }

    public bool Remove(string id)
    {
        var manifest = LoadManifest();
        manifest.Remove(id);
        SaveManifest(manifest);

        string dirPath = Path.Combine(_modsFolder, id);
        if (Directory.Exists(dirPath))
        {
            try { Directory.Delete(dirPath, true); return true; } catch (IOException) { return false; }
        }

        string filePath = Path.Combine(_modsFolder, id);
        if (File.Exists(filePath))
        {
            try { File.Delete(filePath); return true; } catch (IOException) { return false; }
        }

        return true;
    }

    // Prepares the game directory for launch.
    // If mods are active, creates fast, zero-disk-cost staged folders (UserData\game_staged and optional
    // UserData\update_staged) linking original files and overlaying enabled mod files.
    // If no mods are active, returns original folders directly with zero staging.
    public (string GameFolder, string? UpdateFolder) PrepareLaunch(string baseGameFolder, string? titleUpdateFolder)
    {
        string stagedDir = _paths.StagedGameFolder;
        string stagedUpdateDir = _paths.StagedUpdateFolder;
        var activeMods = List().Where(m => m.Enabled).ToList();

        if (activeMods.Count == 0)
        {
            CleanDirectory(stagedDir);
            CleanDirectory(stagedUpdateDir);
            return (baseGameFolder, titleUpdateFolder);
        }

        // Clean staging directories
        CleanDirectory(stagedDir);
        Directory.CreateDirectory(stagedDir);
        CleanDirectory(stagedUpdateDir);

        // 1. Stage the base game files directly using instant NTFS hard links (0 disk space)
        StageFilesFromDirectory(baseGameFolder, stagedDir, preserveDirectHierarchy: true);

        // 2. Overlay enabled mods in order, seamlessly overriding vanilla files
        var stagedModFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var mod in activeMods)
        {
            string modPath = Path.Combine(_modsFolder, mod.RelativePath);
            if (Directory.Exists(modPath))
            {
                StageFilesFromDirectory(modPath, stagedDir, preserveDirectHierarchy: false, stagedModFiles);
            }
            else if (File.Exists(modPath))
            {
                // Single loose file: route intelligently into its proper subfolder
                string destRel = GuessRelativePathForLooseFile(mod.RelativePath);
                string destFile = Path.Combine(stagedDir, destRel);
                Directory.CreateDirectory(Path.GetDirectoryName(destFile)!);
                LinkOrCopy(modPath, destFile);
                stagedModFiles.Add(destRel);
            }
        }

        // 3. Auto-update archive catalogs (plist360.arc & plist360_4x3.arc) for any modified PAC files
        PatchArcCatalogs(baseGameFolder, stagedDir, stagedModFiles);

        // 4. If Title Update is also active, stage the Title Update files + mods + patched catalogs
        string? resultUpdateFolder = null;
        if (!string.IsNullOrEmpty(titleUpdateFolder) && Directory.Exists(titleUpdateFolder))
        {
            Directory.CreateDirectory(stagedUpdateDir);
            StageFilesFromDirectory(titleUpdateFolder, stagedUpdateDir, preserveDirectHierarchy: true);

            var stagedTuModFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var mod in activeMods)
            {
                string modPath = Path.Combine(_modsFolder, mod.RelativePath);
                if (Directory.Exists(modPath))
                {
                    StageFilesFromDirectory(modPath, stagedUpdateDir, preserveDirectHierarchy: false, stagedTuModFiles);
                }
                else if (File.Exists(modPath))
                {
                    string destRel = GuessRelativePathForLooseFile(mod.RelativePath);
                    string destFile = Path.Combine(stagedUpdateDir, destRel);
                    Directory.CreateDirectory(Path.GetDirectoryName(destFile)!);
                    LinkOrCopy(modPath, destFile);
                    stagedTuModFiles.Add(destRel);
                }
            }

            PatchArcCatalogs(titleUpdateFolder, stagedUpdateDir, stagedTuModFiles);
            resultUpdateFolder = stagedUpdateDir;
        }

        return (stagedDir, resultUpdateFolder);
    }

    // Same as PrepareLaunch for a game in a disc image. The image cannot be linked file by file, so UserData\game_staged
    // only holds the enabled mod files (plus the catalogs they need patched); the game reads that folder first and the
    // image for everything else. The catalogs are patched from copies read out of the image.
    // Returns an empty overlay folder when no mods are enabled.
    public (string OverlayFolder, string? UpdateFolder) PrepareImageLaunch(string imagePath, string? titleUpdateFolder)
    {
        string stagedDir = _paths.StagedGameFolder;
        CleanDirectory(stagedDir);
        CleanDirectory(_paths.StagedUpdateFolder);
        var activeMods = List().Where(m => m.Enabled).ToList();
        if (activeMods.Count == 0) return ("", titleUpdateFolder);

        Directory.CreateDirectory(stagedDir);
        var stagedModFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        StageMods(activeMods, stagedDir, stagedModFiles);

        string imageBase = Path.Combine(_paths.UserData, "image_catalogs");
        CleanDirectory(imageBase);
        using (var image = XdvdfsReader.Open(imagePath))
        {
            if (image != null)
            {
                foreach (var name in new[] { "plist360.def", "plist360.arc", "plist360_4x3.def", "plist360_4x3.arc" })
                    image.Extract(name, Path.Combine(imageBase, name));
            }
        }
        PatchArcCatalogs(imageBase, stagedDir, stagedModFiles);
        CleanDirectory(imageBase);

        string? resultUpdateFolder = titleUpdateFolder;
        if (!string.IsNullOrEmpty(titleUpdateFolder) && Directory.Exists(titleUpdateFolder))
        {
            string stagedUpdateDir = _paths.StagedUpdateFolder;
            Directory.CreateDirectory(stagedUpdateDir);
            StageFilesFromDirectory(titleUpdateFolder, stagedUpdateDir, preserveDirectHierarchy: true);
            var stagedTuModFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            StageMods(activeMods, stagedUpdateDir, stagedTuModFiles);
            PatchArcCatalogs(titleUpdateFolder, stagedUpdateDir, stagedTuModFiles);
            resultUpdateFolder = stagedUpdateDir;
        }
        return (stagedDir, resultUpdateFolder);
    }

    void StageMods(List<InstalledMod> mods, string targetDir, HashSet<string> stagedFiles)
    {
        foreach (var mod in mods)
        {
            string modPath = Path.Combine(_modsFolder, mod.RelativePath);
            if (Directory.Exists(modPath))
            {
                StageFilesFromDirectory(modPath, targetDir, preserveDirectHierarchy: false, stagedFiles);
            }
            else if (File.Exists(modPath))
            {
                string destRel = GuessRelativePathForLooseFile(mod.RelativePath);
                string destFile = Path.Combine(targetDir, destRel);
                Directory.CreateDirectory(Path.GetDirectoryName(destFile)!);
                LinkOrCopy(modPath, destFile);
                stagedFiles.Add(destRel);
            }
        }
    }

    public void CleanupStaged()
    {
        CleanDirectory(_paths.StagedGameFolder);
        CleanDirectory(_paths.StagedUpdateFolder);
    }

    private static void StageFilesFromDirectory(string sourceDir, string targetRoot, bool preserveDirectHierarchy = false, HashSet<string>? trackedDestFiles = null)
    {
        var dirInfo = new DirectoryInfo(sourceDir);
        foreach (var file in dirInfo.EnumerateFiles("*", SearchOption.AllDirectories))
        {
            string rel = Path.GetRelativePath(sourceDir, file.FullName);
            // Ignore manifest/hidden files
            if (rel.StartsWith('.') || rel.Equals("mods.json", StringComparison.OrdinalIgnoreCase))
                continue;

            string destRel;
            if (preserveDirectHierarchy)
            {
                destRel = rel;
            }
            else
            {
                string top = rel.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)[0].ToLowerInvariant();
                if (top is "pac" or "movies" or "sound")
                {
                    destRel = rel;
                }
                else
                {
                    destRel = GuessRelativePathForLooseFile(Path.GetFileName(rel));
                }
            }

            string dest = Path.Combine(targetRoot, destRel);
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
            LinkOrCopy(file.FullName, dest);
            trackedDestFiles?.Add(destRel);
        }
    }

    private sealed record PacSubfileEntry(string Name, uint Offset, uint Size);

    // Automatically inspects modified PAC containers and synchronizes their entry sector offsets and sizes
    // into plist360.arc & plist360_4x3.arc. Yuke's engine relies on plist360.arc for character and arena memory
    // allocations and streaming sector offsets; without updating the catalog, modded PACs will corrupt memory
    // and crash (EXCEPTION_ACCESS_VIOLATION).
    private static void PatchArcCatalogs(string baseGameFolder, string stagedDir, HashSet<string> stagedModFiles)
    {
        string defPath = Path.Combine(stagedDir, "plist360.def");
        if (!File.Exists(defPath))
            defPath = Path.Combine(baseGameFolder, "plist360.def");
        if (!File.Exists(defPath))
            return;

        List<string> defLines;
        try
        {
            defLines = File.ReadAllLines(defPath)
                .Select(l => l.Trim())
                .Where(l => !string.IsNullOrEmpty(l) && !l.StartsWith(';'))
                .ToList();
        }
        catch (IOException)
        {
            return;
        }

        var pacUpdates = new Dictionary<int, List<PacSubfileEntry>>();

        foreach (var modRel in stagedModFiles)
        {
            if (!modRel.EndsWith(".pac", StringComparison.OrdinalIgnoreCase))
                continue;

            string normRel = modRel.Replace('/', '\\');
            int defIndex = defLines.FindIndex(l => l.Equals(normRel, StringComparison.OrdinalIgnoreCase));
            if (defIndex < 0)
                continue;

            string fullPacPath = Path.Combine(stagedDir, modRel);
            if (!File.Exists(fullPacPath))
                continue;

            var subfiles = ReadPacDirectoryTable(fullPacPath);
            if (subfiles.Count > 0)
            {
                pacUpdates[defIndex] = subfiles;
            }
        }

        if (pacUpdates.Count == 0)
            return;

        string[] arcNames = ["plist360.arc", "plist360_4x3.arc"];
        foreach (var arcName in arcNames)
        {
            string stagedArc = Path.Combine(stagedDir, arcName);
            string baseArc = Path.Combine(baseGameFolder, arcName);

            try
            {
                // Ensure stagedArc is a physical independent file (break any NTFS hard link to the base disc)
                if (File.Exists(stagedArc))
                {
                    byte[] currentBytes = File.ReadAllBytes(stagedArc);
                    File.Delete(stagedArc);
                    File.WriteAllBytes(stagedArc, currentBytes);
                }
                else if (File.Exists(baseArc))
                {
                    File.Copy(baseArc, stagedArc, overwrite: true);
                }
                else
                {
                    continue;
                }

                byte[] arcBytes = File.ReadAllBytes(stagedArc);
                bool modified = false;

                foreach (var (defIndex, subfiles) in pacUpdates)
                {
                    if (ApplyArcEntryUpdate(arcBytes, defIndex, subfiles))
                    {
                        modified = true;
                    }
                }

                if (modified)
                {
                    File.WriteAllBytes(stagedArc, arcBytes);
                }
            }
            catch (IOException) { }
        }
    }

    private static List<PacSubfileEntry> ReadPacDirectoryTable(string pacPath)
    {
        var entries = new List<PacSubfileEntry>();
        try
        {
            byte[] headerBuffer;
            using (var fs = File.OpenRead(pacPath))
            {
                headerBuffer = new byte[Math.Min(fs.Length, 65536)];
                int read = fs.Read(headerBuffer, 0, headerBuffer.Length);
                if (read < 32) return entries;
            }

            // Normalize EPK8 -> EPAC if present (breaking hardlink)
            if (headerBuffer[0] == 0x45 && headerBuffer[1] == 0x50 && headerBuffer[2] == 0x4B && headerBuffer[3] == 0x38)
            {
                NormalizePacMagic(pacPath);
                using var fs = File.OpenRead(pacPath);
                int read = fs.Read(headerBuffer, 0, headerBuffer.Length);
                if (read < 32) return entries;
            }

            // Scan first 64KB for EMD or STG table signature
            int tableOffset = -1;
            for (int i = 0; i <= headerBuffer.Length - 16; i += 4)
            {
                if ((headerBuffer[i] == 'E' && headerBuffer[i + 1] == 'M' && headerBuffer[i + 2] == 'D' && headerBuffer[i + 3] == ' ') ||
                    (headerBuffer[i] == 'S' && headerBuffer[i + 1] == 'T' && headerBuffer[i + 2] == 'G' && headerBuffer[i + 3] == ' '))
                {
                    tableOffset = i;
                    break;
                }
            }

            if (tableOffset < 0) return entries;

            // EMD / STG record table: 4 bytes tag + 8 bytes metadata = 12 bytes header
            // Followed by 16-byte records: 8 bytes Name, 4 bytes Offset, 4 bytes Size
            int entryPos = tableOffset + 12;
            while (entryPos + 16 <= headerBuffer.Length)
            {
                bool isValidName = true;
                for (int k = 0; k < 8; k++)
                {
                    byte b = headerBuffer[entryPos + k];
                    if (b == 0 && k > 0) break;
                    if (!((b >= '0' && b <= '9') || (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || b == '_' || b == ' '))
                    {
                        isValidName = false;
                        break;
                    }
                }

                if (!isValidName || headerBuffer[entryPos] == 0 || headerBuffer[entryPos] == 0xFF)
                    break;

                string name = System.Text.Encoding.ASCII.GetString(headerBuffer, entryPos, 8).TrimEnd('\0', ' ');
                uint offset = BinaryPrimitives.ReadUInt32LittleEndian(headerBuffer.AsSpan(entryPos + 8, 4));
                uint size = BinaryPrimitives.ReadUInt32LittleEndian(headerBuffer.AsSpan(entryPos + 12, 4));

                if (!string.IsNullOrEmpty(name) && size > 0)
                {
                    entries.Add(new PacSubfileEntry(name, offset, size));
                }

                entryPos += 16;
            }
        }
        catch (IOException) { }
        return entries;
    }

    private static void NormalizePacMagic(string pacPath)
    {
        try
        {
            // Break hard link so source file in mods/ is untouched
            byte[] bytes = File.ReadAllBytes(pacPath);
            if (bytes.Length >= 4 && bytes[0] == 'E' && bytes[1] == 'P' && bytes[2] == 'K' && bytes[3] == '8')
            {
                bytes[2] = (byte)'A';
                bytes[3] = (byte)'C';
                File.Delete(pacPath);
                File.WriteAllBytes(pacPath, bytes);
            }
        }
        catch (IOException) { }
    }

    private static bool ApplyArcEntryUpdate(byte[] arcBytes, int defIndex, List<PacSubfileEntry> subfiles)
    {
        byte hi = (byte)(defIndex >> 8);
        byte lo = (byte)(defIndex & 0xFF);
        int pos = -1;

        for (int i = 0; i <= arcBytes.Length - 4; i++)
        {
            if (arcBytes[i] == 0xFF && arcBytes[i + 1] == 0xFF && arcBytes[i + 2] == hi && arcBytes[i + 3] == lo)
            {
                pos = i;
                break;
            }
        }

        if (pos < 0) return false;

        // Subfile entries in the arc entry start at pos + 16 (after 4-byte marker, 4-byte tag, 8-byte metadata)
        int recPos = pos + 16;
        bool anyUpdated = false;

        while (recPos + 16 <= arcBytes.Length)
        {
            if (arcBytes[recPos] == 0xFF && arcBytes[recPos + 1] == 0xFF)
                break;

            string arcSubName = System.Text.Encoding.ASCII.GetString(arcBytes, recPos, 8).TrimEnd('\0', ' ');
            if (string.IsNullOrEmpty(arcSubName))
                break;

            var match = subfiles.FirstOrDefault(s => s.Name.Equals(arcSubName, StringComparison.OrdinalIgnoreCase));
            if (match != null)
            {
                BinaryPrimitives.WriteUInt32BigEndian(arcBytes.AsSpan(recPos + 8, 4), match.Offset);
                BinaryPrimitives.WriteUInt32BigEndian(arcBytes.AsSpan(recPos + 12, 4), match.Size);
                anyUpdated = true;
            }

            recPos += 16;
        }

        return anyUpdated;
    }

    private static string GuessRelativePathForLooseFile(string fileName)
    {
        string fn = fileName.ToLowerInvariant();
        if (fn.StartsWith("ch") && fn.EndsWith(".pac")) return Path.Combine("pac", "ch", fileName);
        if (fn.StartsWith("bg") && fn.EndsWith(".pac")) return Path.Combine("pac", "bg", fileName);
        if (fn.StartsWith("arena") && fn.EndsWith(".pac")) return Path.Combine("pac", "bg", fileName);
        if (fn.StartsWith("menu") && fn.EndsWith(".pac")) return Path.Combine("pac", "menu", fileName);
        if (fn.StartsWith("title") && fn.EndsWith(".pac")) return Path.Combine("pac", "menu", fileName);
        if (fn.Equals("matchhd.pac", StringComparison.OrdinalIgnoreCase)) return Path.Combine("pac", "menu", fileName);
        if (fn.Equals("matchsd.pac", StringComparison.OrdinalIgnoreCase)) return Path.Combine("pac", "menu", fileName);
        if (fn.Equals("string.pac", StringComparison.OrdinalIgnoreCase)) return Path.Combine("pac", fileName);
        if (fn.Equals("plist360.arc", StringComparison.OrdinalIgnoreCase)) return fileName;
        if (fn.Equals("plist360.def", StringComparison.OrdinalIgnoreCase)) return fileName;
        if (fn.Equals("plist360_4x3.arc", StringComparison.OrdinalIgnoreCase)) return fileName;
        if (fn.Equals("plist360_4x3.def", StringComparison.OrdinalIgnoreCase)) return fileName;
        if (fn.EndsWith(".bik")) return Path.Combine("movies", fileName);
        if (fn.EndsWith(".bnk") || fn.EndsWith(".pck")) return Path.Combine("sound", fileName);
        return Path.Combine("pac", fileName);
    }

    [DllImport("kernel32.dll", EntryPoint = "CreateHardLinkW", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool CreateHardLink(string lpFileName, string lpExistingFileName, IntPtr lpSecurityAttributes);

    private static void LinkOrCopy(string src, string dst)
    {
        try
        {
            if (File.Exists(dst))
                File.Delete(dst);

            // Instant zero-cost NTFS hardlink
            if (CreateHardLink(dst, src, IntPtr.Zero))
                return;

            // Fallback to copy if cross-volume or permission restricted
            File.Copy(src, dst, overwrite: true);
        }
        catch (IOException) { }
    }

    private static void CopyDirectory(string sourceDir, string destinationDir)
    {
        var dir = new DirectoryInfo(sourceDir);
        if (!dir.Exists) return;

        Directory.CreateDirectory(destinationDir);
        foreach (var file in dir.GetFiles())
        {
            file.CopyTo(Path.Combine(destinationDir, file.Name), overwrite: true);
        }
        foreach (var subDir in dir.GetDirectories())
        {
            CopyDirectory(subDir.FullName, Path.Combine(destinationDir, subDir.Name));
        }
    }

    private static void CleanDirectory(string path)
    {
        try
        {
            if (Directory.Exists(path))
                Directory.Delete(path, true);
        }
        catch (IOException) { }
    }
}
