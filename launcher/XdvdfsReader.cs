using System.Buffers.Binary;
using System.Text;

namespace Wwe2k14Launcher;

// Reads single files out of an Xbox 360 disc image (XDVDFS). The game partition starts at one of a few fixed offsets
// depending on the disc layout; 32 sectors into it is the volume descriptor ("MICROSOFT*XBOX*MEDIA", root directory
// sector and size). Each directory is a binary tree of entries sorted by name:
// left/right child (in dwords), first sector, size, attributes, name length, name.
public sealed class XdvdfsReader : IDisposable
{
    const int SectorSize = 2048;
    static readonly byte[] Magic = "MICROSOFT*XBOX*MEDIA"u8.ToArray();
    static readonly long[] PartitionOffsets = { 0, 0xFD90000, 0x2080000, 0x18300000 };

    readonly FileStream _file;
    readonly long _partition;
    readonly uint _rootSector;
    readonly uint _rootSize;

    XdvdfsReader(FileStream file, long partition, uint rootSector, uint rootSize)
    {
        _file = file;
        _partition = partition;
        _rootSector = rootSector;
        _rootSize = rootSize;
    }

    public static XdvdfsReader? Open(string imagePath)
    {
        FileStream? file = null;
        try
        {
            file = File.OpenRead(imagePath);
            var header = new byte[Magic.Length + 8];
            foreach (long partition in PartitionOffsets)
            {
                long at = partition + 32L * SectorSize;
                if (at + header.Length > file.Length) continue;
                file.Position = at;
                if (file.Read(header, 0, header.Length) != header.Length) continue;
                if (!header.AsSpan(0, Magic.Length).SequenceEqual(Magic)) continue;
                uint rootSector = BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(Magic.Length));
                uint rootSize = BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(Magic.Length + 4));
                return new XdvdfsReader(file, partition, rootSector, rootSize);
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        file?.Dispose();
        return null;
    }

    public static bool IsXboxImage(string imagePath)
    {
        using var reader = Open(imagePath);
        return reader != null;
    }

    // Copies the file at `relativePath` (e.g. "plist360.arc" or "pac\ch\ch115.pac") to `destination`.
    public bool Extract(string relativePath, string destination)
    {
        var entry = Find(relativePath);
        if (entry is not { IsDirectory: false }) return false;
        Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
        using var output = File.Create(destination);
        _file.Position = _partition + (long)entry.Value.Sector * SectorSize;
        var buffer = new byte[1 << 20];
        long left = entry.Value.Size;
        while (left > 0)
        {
            int read = _file.Read(buffer, 0, (int)Math.Min(buffer.Length, left));
            if (read <= 0) return false;
            output.Write(buffer, 0, read);
            left -= read;
        }
        return true;
    }

    public bool Exists(string relativePath) => Find(relativePath) != null;

    readonly record struct Entry(uint Sector, uint Size, bool IsDirectory);

    Entry? Find(string relativePath)
    {
        var parts = relativePath.Split('\\', '/', StringSplitOptions.RemoveEmptyEntries);
        Entry current = new(_rootSector, _rootSize, true);
        foreach (var name in parts)
        {
            if (!current.IsDirectory) return null;
            var next = FindInDirectory(current, name);
            if (next == null) return null;
            current = next.Value;
        }
        return current;
    }

    Entry? FindInDirectory(Entry dir, string name)
    {
        if (dir.Size == 0 || dir.Size > 64 * 1024 * 1024) return null;
        var table = new byte[dir.Size];
        _file.Position = _partition + (long)dir.Sector * SectorSize;
        if (_file.Read(table, 0, table.Length) != table.Length) return null;

        int offset = 0;
        for (int guard = 0; guard < 65536; guard++)
        {
            if (offset < 0 || offset + 14 > table.Length) return null;
            ushort left = BinaryPrimitives.ReadUInt16LittleEndian(table.AsSpan(offset));
            ushort right = BinaryPrimitives.ReadUInt16LittleEndian(table.AsSpan(offset + 2));
            if (left == 0xFFFF && right == 0xFFFF) return null;   // padding: empty directory
            uint sector = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(offset + 4));
            uint size = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(offset + 8));
            byte attributes = table[offset + 12];
            int nameLength = table[offset + 13];
            if (offset + 14 + nameLength > table.Length) return null;
            string entryName = Encoding.ASCII.GetString(table, offset + 14, nameLength);

            int order = string.Compare(name, entryName, StringComparison.OrdinalIgnoreCase);
            if (order == 0) return new Entry(sector, size, (attributes & 0x10) != 0);
            int child = order < 0 ? left : right;
            if (child == 0) return null;
            offset = child * 4;
        }
        return null;
    }

    public void Dispose() => _file.Dispose();
}
