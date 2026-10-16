using System.Runtime.InteropServices;

namespace Wwe2k14Launcher;

// Custom logos for the launcher's Logos tab: lists the logos in the game's Paint Tool save and adds / removes them.
// Every change first copies the save to userdata\backups\PaintTool-<time>.Dat.
public static class LogoStore
{
    public sealed record Logo(int Slot, bool Preset, string Image);

    public static string? SavePath(LauncherPaths paths) => PaintToolSave.Find(paths.UserData);

    // Logos as PNG data URLs, or null when there is no Paint Tool save yet.
    public static List<Logo>? List(LauncherPaths paths, out string? error)
    {
        error = null;
        string? path = SavePath(paths);
        if (path is null) return null;
        try
        {
            var save = PaintToolSave.Load(path);
            var list = new List<Logo>();
            for (int i = 0; i < PaintToolSave.SlotCount; i++)
                if (save.IsUsed(i))
                    list.Add(new Logo(i, save.IsPreset(i), "data:image/png;base64," + Convert.ToBase64String(save.RenderPng(i))));
            return list;
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException)
        {
            error = ex.Message;
            return null;
        }
    }

    // --- Library ---------------------------------------------------------------------------------------------------------
    // Every logo you add or import is kept in userdata\logo_library (raw slot + PNG preview, named by the picture's id),
    // whether or not it is in the game. The game's save holds at most 20; the library decides which.

    public sealed record LibraryLogo(string Id, bool InGame, string Image);

    static string LibraryFolder(LauncherPaths paths) => Path.Combine(paths.UserData, "logo_library");

    static IEnumerable<string> LibraryIds(LauncherPaths paths) =>
        Directory.Exists(LibraryFolder(paths))
            ? new DirectoryInfo(LibraryFolder(paths)).EnumerateFiles("*.slot").OrderBy(f => f.CreationTimeUtc)
                .Select(f => Path.GetFileNameWithoutExtension(f.Name))
            : [];

    // Stores a slot in the library; returns its id, or null when it was already there.
    static string? Keep(LauncherPaths paths, byte[] slot)
    {
        string id = PaintToolSave.ImageId(slot);
        string folder = LibraryFolder(paths);
        string file = Path.Combine(folder, id + ".slot");
        if (File.Exists(file)) return null;
        Directory.CreateDirectory(folder);
        var blank = PaintToolSave.Blank();
        blank.PutSlot(0, slot);
        File.WriteAllBytes(Path.Combine(folder, id + ".png"), blank.RenderPng(0));
        File.WriteAllBytes(file, slot);
        return id;
    }

    // Logos already in the game's save (made in the Paint Tool, or added before the library existed) join the library.
    static void KeepGameLogos(LauncherPaths paths, PaintToolSave save)
    {
        for (int i = 0; i < PaintToolSave.SlotCount; i++)
            if (save.IsUsed(i) && !save.IsPreset(i)) Keep(paths, save.ExportSlot(i));
    }

    public static List<LibraryLogo> Library(LauncherPaths paths)
    {
        var inGame = new HashSet<string>();
        string? path = SavePath(paths);
        if (path != null)
        {
            try
            {
                var save = PaintToolSave.Load(path);
                KeepGameLogos(paths, save);
                for (int i = 0; i < PaintToolSave.SlotCount; i++)
                    if (save.IsUsed(i)) inGame.Add(save.ImageId(i));
            }
            catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException) { }
        }
        return LibraryIds(paths).Select(id =>
        {
            string png = Path.Combine(LibraryFolder(paths), id + ".png");
            string image = File.Exists(png) ? "data:image/png;base64," + Convert.ToBase64String(File.ReadAllBytes(png)) : "";
            return new LibraryLogo(id, inGame.Contains(id), image);
        }).ToList();
    }

    // Puts library logos into free game slots, in order, while there is room. Returns how many went in.
    static int PutInGame(LauncherPaths paths, PaintToolSave save, IEnumerable<string> ids)
    {
        int added = 0;
        foreach (var id in ids)
        {
            if (Enumerable.Range(0, PaintToolSave.SlotCount).Any(i => save.IsUsed(i) && save.ImageId(i) == id)) continue;
            int slot = save.FreeSlot();
            if (slot < 0) break;
            save.PutSlot(slot, File.ReadAllBytes(Path.Combine(LibraryFolder(paths), id + ".slot")));
            added++;
        }
        return added;
    }

    // A library logo goes into the game, or comes out of it.
    public static string? Toggle(LauncherPaths paths, string id, out string message)
    {
        message = "";
        if (!File.Exists(Path.Combine(LibraryFolder(paths), id + ".slot"))) return "That logo is no longer in the library.";
        return Change(paths, save =>
        {
            int slot = Enumerable.Range(0, PaintToolSave.SlotCount).FirstOrDefault(i => save.IsUsed(i) && save.ImageId(i) == id, -1);
            if (slot >= 0)
            {
                if (save.IsPreset(slot)) return "The game's default logo can't be removed.";
                save.Clear(slot);
                return null;
            }
            return PutInGame(paths, save, [id]) == 1 ? null : "All 20 logo slots in the game are used. Take one out first.";
        }, "", ref message);
    }

    // Deletes a logo from the library, and from the game if it is in there.
    public static string? Delete(LauncherPaths paths, string id, out string message)
    {
        message = "";
        if (SavePath(paths) != null)
        {
            string ignored = "";
            string? error = Change(paths, save =>
            {
                for (int i = 0; i < PaintToolSave.SlotCount; i++)
                    if (save.IsUsed(i) && !save.IsPreset(i) && save.ImageId(i) == id) save.Clear(i);
                return null;
            }, "", ref ignored);
            if (error != null) return error;
        }
        foreach (var ext in new[] { ".slot", ".png" })
            try { File.Delete(Path.Combine(LibraryFolder(paths), id + ext)); } catch (IOException) { }
        message = "Logo deleted from the library.";
        return null;
    }

    // Adds image files to the library, and to the game while there is room.
    public static string? Add(LauncherPaths paths, IEnumerable<string> imageFiles, out string message)
    {
        message = "";
        var ids = new List<string>();
        int fresh = 0;
        foreach (var file in imageFiles)
        {
            var blank = PaintToolSave.Blank();
            try { blank.Import(0, file); }
            catch (Exception ex) when (ex is ArgumentException or OutOfMemoryException or ExternalException or IOException)
            {
                return $"{Path.GetFileName(file)} could not be read as an image.";
            }
            var slot = blank.ExportSlot(0);
            if (Keep(paths, slot) != null) fresh++;
            ids.Add(PaintToolSave.ImageId(slot));
        }
        return AddToGame(paths, ids, fresh, "", out message);
    }

    // Adds the custom logos of a PS3 save (its folder, its LOGOS.DAT, or a .zip / .rar of it) to the library, and to the
    // game while there is room. The game's built-in default logo is left out.
    public static string? ImportPs3(LauncherPaths paths, string source, out string message)
    {
        message = "";
        PaintToolSave ps3;
        try
        {
            byte[]? data = ReadPs3Logos(source);
            if (data is null)
                return "No LOGOS.DAT in there: choose a WWE 2K14 PS3 save folder, its LOGOS.DAT, or a .zip / .rar of the save.";
            ps3 = PaintToolSave.FromPs3Logos(data, source);
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException)
        {
            return ex.Message;
        }
        // PS3 saves may flag every logo like a built-in one, so the game's own default logo is recognised by its picture.
        var defaults = new HashSet<string>();
        try
        {
            if (SavePath(paths) is { } savePath)
            {
                var save = PaintToolSave.Load(savePath);
                for (int i = 0; i < PaintToolSave.SlotCount; i++)
                    if (save.IsUsed(i) && save.IsPreset(i)) defaults.Add(save.ImageId(i));
            }
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException) { }
        var ids = new List<string>();
        int fresh = 0;
        for (int i = 0; i < PaintToolSave.SlotCount; i++)
        {
            if (!ps3.IsUsed(i) || defaults.Contains(ps3.ImageId(i))) continue;
            var slot = ps3.ExportSlot(i);
            if (Keep(paths, slot) != null) fresh++;
            ids.Add(PaintToolSave.ImageId(slot));
        }
        if (ids.Count == 0) return "That PS3 save has no custom logos.";
        return AddToGame(paths, ids, fresh, " from the PS3 save", out message);
    }

    static string? AddToGame(LauncherPaths paths, List<string> ids, int newInLibrary, string from, out string message)
    {
        message = "";
        int added = 0;
        bool hasSave = SavePath(paths) != null;
        if (hasSave)
        {
            string ignored = "";
            string? error = Change(paths, save =>
            {
                added = PutInGame(paths, save, ids);
                return null;
            }, "", ref ignored);
            if (error != null) return error;
        }
        static string Logos(int n) => n == 1 ? "1 logo" : $"{n} logos";
        if (newInLibrary == 0 && added == 0)
        {
            message = ids.Count == 1 ? "That logo is already in your library." : "Those logos are already in your library.";
            return null;
        }
        message = $"{Logos(newInLibrary)} added to the library{from}";
        if (added > 0) message += $", {added} put in the game";
        if (!hasSave) message += ". The game has no logo save yet: save a logo once in the Paint Tool, then pick them in the Library";
        else if (added < ids.Count && PutInGameFull(paths)) message += ". The game is full (20): pick which ones to use in the Library";
        message += ".";
        return null;
    }

    static bool PutInGameFull(LauncherPaths paths)
    {
        try { return PaintToolSave.Load(SavePath(paths)!).FreeSlot() < 0; }
        catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException) { return false; }
    }

    static byte[]? ReadPs3Logos(string source)
    {
        if (File.Exists(source) && !IsArchive(source))
            return File.ReadAllBytes(source);
        string? folder = Directory.Exists(source) ? source : null;
        string? temp = null;
        try
        {
            if (folder is null)
            {
                // Windows' tar reads .zip and .rar; save downloads are often a .rar inside a .zip.
                temp = Path.Combine(Path.GetTempPath(), "wwe2k14-ps3-" + Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(temp);
                Extract(source, temp);
                foreach (var inner in Directory.EnumerateFiles(temp, "*", SearchOption.AllDirectories).Where(IsArchive).ToList())
                    Extract(inner, Path.Combine(temp, Path.GetFileNameWithoutExtension(inner) + "-x"));
                folder = temp;
            }
            string? logos = Directory.EnumerateFiles(folder, "LOGOS.DAT", SearchOption.AllDirectories).FirstOrDefault();
            return logos is null ? null : File.ReadAllBytes(logos);
        }
        finally
        {
            if (temp != null)
                try { Directory.Delete(temp, true); } catch (IOException) { }
        }
    }

    static bool IsArchive(string file) =>
        file.EndsWith(".zip", StringComparison.OrdinalIgnoreCase) || file.EndsWith(".rar", StringComparison.OrdinalIgnoreCase);

    static void Extract(string archive, string destination)
    {
        Directory.CreateDirectory(destination);
        var info = new System.Diagnostics.ProcessStartInfo(Path.Combine(Environment.SystemDirectory, "tar.exe"))
        {
            UseShellExecute = false,
            CreateNoWindow = true,
        };
        foreach (var a in new[] { "-xf", archive, "-C", destination }) info.ArgumentList.Add(a);
        using var p = System.Diagnostics.Process.Start(info) ?? throw new IOException("Could not start tar.exe");
        p.WaitForExit();
        if (p.ExitCode != 0) throw new IOException($"Could not unpack {Path.GetFileName(archive)}.");
    }

    public static string? Remove(LauncherPaths paths, int slot, out string message)
    {
        message = "";
        return Change(paths, save =>
        {
            if (slot < 0 || slot >= PaintToolSave.SlotCount || !save.IsUsed(slot)) return "That logo no longer exists.";
            if (save.IsPreset(slot)) return "The game's default logo can't be removed.";
            Keep(paths, save.ExportSlot(slot));
            save.Clear(slot);
            return null;
        }, "Logo taken out of the game. It stays in your Library.", ref message);
    }

    static string? Change(LauncherPaths paths, Func<PaintToolSave, string?> edit, string success, ref string message)
    {
        string? path = SavePath(paths);
        if (path is null)
            return "No custom logo save yet: in the game, open Creation Suite > Paint Tool and save a logo once.";
        try
        {
            var save = PaintToolSave.Load(path);
            if (!save.ChecksumsValid())
                return "Your logo save is damaged (checksum mismatch), so it was left untouched.";
            string backups = Path.Combine(paths.UserData, "backups");
            Directory.CreateDirectory(backups);
            File.Copy(path, Path.Combine(backups, $"PaintTool-{DateTime.Now:yyyyMMdd-HHmmss}.Dat"), overwrite: true);
            // No gaps: a removed logo's place is taken by the ones after it, and new logos go after the last one.
            save.Compact();
            string? error = edit(save);
            if (error != null) return error;
            save.Compact();
            save.Save();
            message = success;
            return null;
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException)
        {
            return ex.Message;
        }
    }
}
