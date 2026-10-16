using System.Buffers.Binary;
using System.Text;

namespace Wwe2k14Launcher;

// Reader for Xbox 360 STFS packages ("CON ", "LIVE", "PIRS"): the container DLC ships in.
//
// Layout (offsets in the file): 0x340 header size, 0x344 content type, 0x360 title ID, 0x379 the STFS volume
// descriptor (0x37B flags, 0x37C file table block count, 0x37E file table first block), 0x3A9 descriptor type
// (0 = STFS, 1 = SVOD), 0x411 display names (UTF-16BE, 0x80 bytes per language, English first). Data blocks are
// 0x1000 bytes, starting at the header size rounded up to 0x1000, with hash tables interleaved every 170 blocks
// (one or two copies each, depending on the package flags).
public sealed class StfsPackage : IDisposable
{
    public const uint MarketplaceContent = 0x00000002;
    const int BlockSize = 0x1000;
    const int BlocksPerHashTable = 170;
    const int HashEntrySize = 0x18;

    readonly FileStream _file;
    readonly long _dataStart;
    readonly int _tablesPerLevel;   // 1 or 2 copies of each hash table
    readonly int _blockShift;       // log2(_tablesPerLevel)
    readonly uint _totalBlocks;
    readonly int _fileTableBlocks;
    readonly uint _fileTableStart;

    public string Magic { get; }
    public uint ContentType { get; }
    public uint TitleId { get; }
    public string DisplayName { get; }
    public string TitleName { get; }
    public bool IsSvod { get; }

    StfsPackage(FileStream file)
    {
        _file = file;
        var head = new byte[0x1712];
        if (file.Length < head.Length || ReadAt(0, head) != head.Length)
            throw new InvalidDataException("File too small to be an Xbox 360 package.");
        Magic = Encoding.ASCII.GetString(head, 0, 4);
        if (Magic is not ("CON " or "LIVE" or "PIRS"))
            throw new InvalidDataException("Not an Xbox 360 package (no CON/LIVE/PIRS header).");

        uint headerSize = BinaryPrimitives.ReadUInt32BigEndian(head.AsSpan(0x340));
        ContentType = BinaryPrimitives.ReadUInt32BigEndian(head.AsSpan(0x344));
        TitleId = BinaryPrimitives.ReadUInt32BigEndian(head.AsSpan(0x360));
        IsSvod = head[0x3A9] == 1;
        DisplayName = ReadUtf16Be(head, 0x411, 0x80);
        TitleName = ReadUtf16Be(head, 0x1691, 0x80);

        byte flags = head[0x37B];
        _fileTableBlocks = BinaryPrimitives.ReadUInt16LittleEndian(head.AsSpan(0x37C));
        _fileTableStart = ReadInt24Le(head, 0x37E);
        _totalBlocks = BinaryPrimitives.ReadUInt32BigEndian(head.AsSpan(0x395));

        bool readOnly = (flags & 1) != 0;
        _blockShift = (((headerSize + 0xFFF) & 0xB000) == 0xB000 || readOnly) ? 0 : 1;
        _tablesPerLevel = 1 << _blockShift;
        _dataStart = (headerSize + 0xFFFL) & ~0xFFFL;
    }

    public static StfsPackage Open(string path) =>
        new(new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 16));

    // Quick check on the first four bytes, so folders full of other files scan fast.
    public static bool HasPackageMagic(string path)
    {
        try
        {
            using var f = File.OpenRead(path);
            Span<byte> magic = stackalloc byte[4];
            if (f.Read(magic) != 4) return false;
            var s = Encoding.ASCII.GetString(magic);
            return s is "CON " or "LIVE" or "PIRS";
        }
        catch (IOException) { return false; }
        catch (UnauthorizedAccessException) { return false; }
    }

    public sealed record Entry(string Name, bool IsDirectory, bool Consecutive, uint StartBlock, uint BlockCount,
                               int Parent, uint Size);

    public List<Entry> ReadFileTable()
    {
        if (IsSvod)
            throw new NotSupportedException("SVOD packages (large on-disc style content) are not supported yet.");
        var entries = new List<Entry>();
        var block = new byte[BlockSize];
        uint blockIndex = _fileTableStart;
        for (int b = 0; b < _fileTableBlocks; b++)
        {
            ReadBlock(blockIndex, block);
            for (int i = 0; i < BlockSize / 0x40; i++)
            {
                int o = i * 0x40;
                int nameLength = block[o + 0x28] & 0x3F;
                if (nameLength == 0 || nameLength > 40)
                    return entries;
                byte entryFlags = block[o + 0x28];
                bool isDir = (entryFlags & 0x80) != 0;
                bool isConsecutive = (entryFlags & 0x40) != 0;
                uint startBlock = ReadInt24Le(block, o + 0x2F);
                uint blockCount = ReadInt24Le(block, o + 0x29);
                short parent = BinaryPrimitives.ReadInt16BigEndian(block.AsSpan(o + 0x32));
                uint size = BinaryPrimitives.ReadUInt32BigEndian(block.AsSpan(o + 0x34));

                // Guard against uninitialized directory block slack / garbage
                if (!isDir && _totalBlocks > 0 && startBlock >= _totalBlocks)
                    return entries;
                if (parent < -1)
                    return entries;

                string name = Encoding.ASCII.GetString(block, o, nameLength);
                if (name.Any(c => c < ' ' || c > '~'))
                    return entries;

                entries.Add(new Entry(
                    name,
                    isDir,
                    isConsecutive,
                    startBlock,
                    blockCount,
                    parent,
                    size));
            }
            blockIndex = NextBlock(blockIndex);
        }
        return entries;
    }

    // Extracts every file under destination, keeping the package's folder structure.
    public void ExtractTo(string destination, Action<long, long>? progress = null)
    {
        var entries = ReadFileTable();
        long total = entries.Where(e => !e.IsDirectory).Sum(e => (long)e.Size), done = 0;
        string root = Path.GetFullPath(destination);
        Directory.CreateDirectory(root);

        string PathOf(int index)
        {
            var parts = new List<string>();
            for (int guard = 0; index >= 0 && index < entries.Count && guard < 64; guard++)
            {
                parts.Insert(0, SafeName(entries[index].Name));
                index = entries[index].Parent;
            }
            return Path.Combine(parts.ToArray());
        }

        var buffer = new byte[BlockSize];
        for (int i = 0; i < entries.Count; i++)
        {
            var e = entries[i];
            string target = Path.GetFullPath(Path.Combine(root, PathOf(i)));
            if (!target.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException($"Package entry escapes the install folder: {e.Name}");
            if (e.IsDirectory)
            {
                Directory.CreateDirectory(target);
                continue;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            using var output = new FileStream(target, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16);
            long remaining = e.Size;
            uint blockIndex = e.StartBlock;
            while (remaining > 0)
            {
                if (blockIndex >= _totalBlocks && _totalBlocks != 0)
                    throw new InvalidDataException($"{e.Name}: block {blockIndex} is outside the package.");
                ReadBlock(blockIndex, buffer);
                int n = (int)Math.Min(remaining, BlockSize);
                output.Write(buffer, 0, n);
                remaining -= n;
                done += n;
                if (remaining > 0)
                    blockIndex = e.Consecutive ? blockIndex + 1 : NextBlock(blockIndex);
            }
            progress?.Invoke(done, total);
        }
    }

    // --- Block addressing ---------------------------------------------------------------------------------------------

    long BlockToOffset(uint blockIndex)
    {
        long block = blockIndex;
        long levelBase = BlocksPerHashTable;
        for (int level = 0; level < 3; level++)
        {
            block += ((blockIndex + levelBase) / levelBase) << _blockShift;
            if (blockIndex < levelBase)
                break;
            levelBase *= BlocksPerHashTable;
        }
        return _dataStart + (block << 12);
    }

    // Raw block number of the level-0 hash table covering a data block (first copy).
    long Level0HashBlock(uint blockIndex)
    {
        const long perLevel1 = BlocksPerHashTable * BlocksPerHashTable;
        if (blockIndex < BlocksPerHashTable)
            return 0;
        long block = (blockIndex / BlocksPerHashTable) * (BlocksPerHashTable + _tablesPerLevel);
        block += (blockIndex / perLevel1 + 1) * _tablesPerLevel;
        if (blockIndex < perLevel1)
            return block;
        return block + _tablesPerLevel;
    }

    uint NextBlock(uint blockIndex)
    {
        var entry = new byte[HashEntrySize];
        long tableBlock = Level0HashBlock(blockIndex);
        long entryOffset = (blockIndex % BlocksPerHashTable) * HashEntrySize;
        // Two-copy packages keep an active and a stale table; take the first copy whose link is plausible.
        for (int copy = 0; copy < _tablesPerLevel; copy++)
        {
            ReadAt(_dataStart + ((tableBlock + copy) << 12) + entryOffset, entry);
            uint next = ReadInt24Be(entry, 0x15);
            if (next != 0xFFFFFF && (_totalBlocks == 0 || next < _totalBlocks))
                return next;
        }
        return blockIndex + 1;
    }

    void ReadBlock(uint blockIndex, byte[] buffer)
    {
        long offset = BlockToOffset(blockIndex);
        int read = ReadAt(offset, buffer);
        if (read < buffer.Length)
            Array.Clear(buffer, read, buffer.Length - read);   // last block of a trimmed package
    }

    int ReadAt(long offset, byte[] buffer)
    {
        _file.Position = offset;
        int total = 0;
        while (total < buffer.Length)
        {
            int n = _file.Read(buffer, total, buffer.Length - total);
            if (n == 0) break;
            total += n;
        }
        return total;
    }

    // --- Helpers ------------------------------------------------------------------------------------------------------

    static uint ReadInt24Le(byte[] b, int o) => (uint)(b[o] | b[o + 1] << 8 | b[o + 2] << 16);
    static uint ReadInt24Be(byte[] b, int o) => (uint)(b[o] << 16 | b[o + 1] << 8 | b[o + 2]);

    static string ReadUtf16Be(byte[] b, int offset, int length)
    {
        var s = Encoding.BigEndianUnicode.GetString(b, offset, length);
        int end = s.IndexOf('\0');
        return (end >= 0 ? s[..end] : s).Trim();
    }

    static string SafeName(string name)
    {
        var invalid = Path.GetInvalidFileNameChars();
        var clean = new string(name.Select(c => invalid.Contains(c) ? '_' : c).ToArray());
        return clean is "." or ".." || clean.Length == 0 ? "_" : clean;
    }

    public void Dispose() => _file.Dispose();
}
