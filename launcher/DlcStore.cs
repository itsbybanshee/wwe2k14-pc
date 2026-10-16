using System.Buffers.Binary;
using System.Text;

namespace Wwe2k14Launcher;

// Installs Xbox 360 DLC packages where the recompiled game finds them. The launcher unpacks them itself: the
// runtime lists DLC as extracted folders and has no STFS reader of its own.
//
// The runtime's content manager lists marketplace content as extracted folders plus a header sidecar:
//   <user data>\0000000000000000\545408B2\00000002\<name>\                 package contents
//   <user data>\0000000000000000\545408B2\Headers\00000002\<name>.header   XCONTENT_AGGREGATE_DATA (0x148 bytes,
//                                                                          big-endian) + 4-byte license mask
// <name> is the package's file name cut to 42 characters, like XamContentCreate names it on the console.
public sealed class DlcStore
{
    public const uint TitleId = 0x545408B2;
    const int HeaderSize = 0x148;

    readonly string _contentRoot;
    readonly string _headerRoot;

    public DlcStore(string userDataRoot)
    {
        string title = Path.Combine(userDataRoot, "0000000000000000", TitleId.ToString("X8"));
        _contentRoot = Path.Combine(title, "00000002");
        _headerRoot = Path.Combine(title, "Headers", "00000002");
    }

    public string ContentFolder => _contentRoot;

    public sealed record InstalledDlc(string Name, string DisplayName, long SizeBytes);
    public sealed record InstallResult(string File, bool Ok, string Message, string? DisplayName = null);

    public List<InstalledDlc> List()
    {
        var list = new List<InstalledDlc>();
        if (!Directory.Exists(_headerRoot))
            return list;
        foreach (var header in Directory.EnumerateFiles(_headerRoot, "*.header"))
        {
            string name = Path.GetFileNameWithoutExtension(header);
            string folder = Path.Combine(_contentRoot, name);
            if (!Directory.Exists(folder))
                continue;
            string display = name;
            try
            {
                var bytes = File.ReadAllBytes(header);
                if (bytes.Length >= HeaderSize)
                {
                    var s = Encoding.BigEndianUnicode.GetString(bytes, 0x08, 0x100);
                    int end = s.IndexOf('\0');
                    s = (end >= 0 ? s[..end] : s).Trim();
                    if (s.Length > 0) display = s;
                }
            }
            catch (IOException) { }
            long size = 0;
            try
            {
                size = new DirectoryInfo(folder).EnumerateFiles("*", SearchOption.AllDirectories).Sum(f => f.Length);
            }
            catch (IOException) { }
            list.Add(new InstalledDlc(name, display, size));
        }
        return list.OrderBy(d => d.DisplayName, StringComparer.OrdinalIgnoreCase).ToList();
    }

    public InstallResult Install(string packagePath, Action<long, long>? progress = null)
    {
        string fileName = Path.GetFileName(packagePath);
        string name = ContentName(fileName);
        string target = Path.Combine(_contentRoot, name);
        string headerPath = Path.Combine(_headerRoot, name + ".header");
        string staging = target + ".installing";
        try
        {
            using var package = StfsPackage.Open(packagePath);
            if (package.ContentType != StfsPackage.MarketplaceContent)
                return new(fileName, false, package.ContentType switch
                {
                    0x000B0000 => "This is a title update, not DLC.",
                    0x00000001 => "This is a saved game, not DLC.",
                    0x00010000 => "This is a gamer profile, not DLC.",
                    _ => $"Not a DLC package (content type {package.ContentType:X8}).",
                });
            if (package.TitleId != TitleId)
                return new(fileName, false,
                    $"This DLC is for another game ({(package.TitleName.Length > 0 ? package.TitleName : package.TitleId.ToString("X8"))}).",
                    package.DisplayName);
            if (File.Exists(headerPath) && Directory.Exists(target))
                return new(fileName, true, "Already installed.", package.DisplayName);

            if (Directory.Exists(staging))
                Directory.Delete(staging, true);
            package.ExtractTo(staging, progress);

            if (Directory.Exists(target))
                Directory.Delete(target, true);
            Directory.Move(staging, target);
            Directory.CreateDirectory(_headerRoot);
            File.WriteAllBytes(headerPath, BuildHeader(name, package.DisplayName));
            return new(fileName, true, "Installed.", package.DisplayName);
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or NotSupportedException
                                       or UnauthorizedAccessException)
        {
            TryDelete(staging);
            return new(fileName, false, ex.Message);
        }
    }

    public void Remove(string name)
    {
        if (name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 || name is "." or "..")
            throw new ArgumentException("Invalid DLC name.");
        TryDelete(Path.Combine(_contentRoot, name));
        string header = Path.Combine(_headerRoot, name + ".header");
        if (File.Exists(header))
            File.Delete(header);
    }

    // Same rule as XCONTENT_DATA::set_file_name: at most 42 single-byte characters.
    static string ContentName(string fileName)
    {
        var invalid = Path.GetInvalidFileNameChars();
        var sb = new StringBuilder();
        foreach (char c in fileName)
        {
            if (sb.Length == 42) break;
            sb.Append(c < 0x80 && !invalid.Contains(c) ? c : '_');
        }
        return sb.ToString().TrimEnd('.', ' ') is { Length: > 0 } s ? s : "dlc";
    }

    // XCONTENT_AGGREGATE_DATA, big-endian: device_id @0, content_type @4, display_name[128] UTF-16 @8,
    // file_name[42] @0x108, padding @0x132, (align) xuid @0x138, title_id @0x140. Then the license mask the
    // runtime reads after the struct: all bits set = every item in the pack is licensed.
    static byte[] BuildHeader(string name, string displayName)
    {
        var h = new byte[HeaderSize + 4];
        BinaryPrimitives.WriteUInt32BigEndian(h.AsSpan(0x00), 1);   // DummyDeviceId::HDD
        BinaryPrimitives.WriteUInt32BigEndian(h.AsSpan(0x04), StfsPackage.MarketplaceContent);
        var display = Encoding.BigEndianUnicode.GetBytes(displayName.Length > 0 ? displayName : name);
        Array.Copy(display, 0, h, 0x08, Math.Min(display.Length, 0xFE));
        var file = Encoding.ASCII.GetBytes(name);
        Array.Copy(file, 0, h, 0x108, Math.Min(file.Length, 42));
        BinaryPrimitives.WriteUInt64BigEndian(h.AsSpan(0x138), 0);
        BinaryPrimitives.WriteUInt32BigEndian(h.AsSpan(0x140), TitleId);
        BinaryPrimitives.WriteUInt32LittleEndian(h.AsSpan(HeaderSize), 0xFFFFFFFF);
        return h;
    }

    static void TryDelete(string dir)
    {
        try
        {
            if (Directory.Exists(dir))
                Directory.Delete(dir, true);
        }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}
