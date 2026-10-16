using System.Buffers.Binary;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO.Compression;
using System.Runtime.InteropServices;

namespace Wwe2k14Launcher;

// The game's custom logos (Creation Suite > Paint Tool), saved per profile in
//   <user data>\<profile>\545408B2\00000001\00PaintTool.pt\SaveData.Dat
//
// The file is 4 "ZLIB" chunks (magic, then little-endian version 0x3012, compressed size, uncompressed size) whose
// data, concatenated, is one big-endian buffer of 0x64582C bytes:
//   0x000000  u32 0x02A78E9A, u32 7
//   0x000008  20 slots of 0x50468 bytes
//     +0x00000  header: u32 used, u32 preset (The Rock), u32 7, ... (0x48 bytes)
//     +0x00048  256x256 image, A R G B bytes (the editor's copy)
//     +0x40048  info (0x14 bytes), then a 256-colour palette (A R G B)
//     +0x4045C  256x256 palette indices (the copy used in game)
//     +0x5045C  save date, +0x50464 u32 slot checksum
//   0x645828  u32 total checksum
// Checksums are 32-bit sums (the game's sub_8341A528, tools\paint_checksum.py): when the total doesn't match, the game
// says "Custom Logo data is damaged".
public sealed class PaintToolSave
{
    public const int Size = 0x64582C;
    public const int SlotSize = 0x50468;
    public const int SlotCount = 20;
    const int SlotBase = 8;
    const int TotalOffset = 0x645828;
    const int ImageOffset = 0x48, InfoOffset = 0x40048, PaletteOffset = 0x4005C, IndexOffset = 0x4045C;
    const int DateOffset = 0x5045C, SlotSumOffset = 0x50464;
    const int Chunks = 4;
    const uint ChunkVersion = 0x3012;

    // Header and info of a logo made in the Paint Tool (slot 1 of a real save); the image fields are filled in.
    static readonly byte[] UserLogoHeader = Convert.FromHexString(
        "000000010000000000000007003E5C12000000000000000000000000000000000000000000000000" +
        "000000006ACD6AA9010000000100000000000000000000000000010000000100");
    static readonly byte[] Info = Convert.FromHexString("0001010000010020000000000100010008000000");

    readonly byte[] _buf;
    public string Path { get; }

    PaintToolSave(string path, byte[] buf)
    {
        Path = path;
        _buf = buf;
    }

    // The most recently written Paint Tool save of any profile, or null (the game creates it the first time a logo is
    // saved in the Paint Tool).
    public static string? Find(string userData)
    {
        if (!Directory.Exists(userData)) return null;
        return Directory.EnumerateDirectories(userData)
            .Select(profile => System.IO.Path.Combine(profile, "545408B2", "00000001"))
            .Where(Directory.Exists)
            .SelectMany(dir => Directory.EnumerateDirectories(dir, "*PaintTool.pt"))
            .Select(dir => System.IO.Path.Combine(dir, "SaveData.Dat"))
            .Where(File.Exists)
            .OrderByDescending(File.GetLastWriteTimeUtc)
            .FirstOrDefault();
    }

    public static PaintToolSave Load(string path)
    {
        var data = File.ReadAllBytes(path);
        var buf = new byte[Size];
        int o = 0, written = 0;
        while (o + 16 <= data.Length && data[o] == 'Z' && data[o + 1] == 'L' && data[o + 2] == 'I' && data[o + 3] == 'B')
        {
            int csize = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(o + 8));
            int usize = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(o + 12));
            if (written + usize > Size) throw new InvalidDataException("The logo save is larger than expected.");
            using var z = new ZLibStream(new MemoryStream(data, o + 16, csize), CompressionMode.Decompress);
            z.ReadExactly(buf, written, usize);
            written += usize;
            o += 16 + csize;
        }
        if (written != Size) throw new InvalidDataException("This is not a WWE 2K14 logo save.");
        return new PaintToolSave(path, buf);
    }

    public void Save()
    {
        UpdateChecksums();
        using var output = new MemoryStream();
        int part = Size / Chunks;
        for (int i = 0; i < Chunks; i++)
        {
            int len = i == Chunks - 1 ? Size - part * i : part;
            using var comp = new MemoryStream();
            using (var z = new ZLibStream(comp, CompressionLevel.Optimal, leaveOpen: true))
                z.Write(_buf, part * i, len);
            Span<byte> header = stackalloc byte[16];
            "ZLIB"u8.CopyTo(header);
            BinaryPrimitives.WriteUInt32LittleEndian(header[4..], ChunkVersion);
            BinaryPrimitives.WriteInt32LittleEndian(header[8..], (int)comp.Length);
            BinaryPrimitives.WriteInt32LittleEndian(header[12..], len);
            output.Write(header);
            comp.Position = 0;
            comp.CopyTo(output);
        }
        string tmp = Path + ".tmp";
        File.WriteAllBytes(tmp, output.ToArray());
        File.Move(tmp, Path, overwrite: true);
    }

    // --- Slots ---------------------------------------------------------------------------------------------------------

    static int Slot(int i) => SlotBase + i * SlotSize;
    uint U32(int o) => BinaryPrimitives.ReadUInt32BigEndian(_buf.AsSpan(o));

    public bool IsUsed(int i) => U32(Slot(i)) == 1;
    public bool IsPreset(int i) => U32(Slot(i) + 4) != 0;
    public int FreeSlot() => Enumerable.Range(0, SlotCount).FirstOrDefault(i => !IsUsed(i), -1);

    // The editor's 256x256 copy as a PNG.
    public byte[] RenderPng(int i)
    {
        using var bmp = new Bitmap(256, 256, PixelFormat.Format32bppArgb);
        var data = bmp.LockBits(new Rectangle(0, 0, 256, 256), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        var bgra = new byte[256 * 256 * 4];
        int src = Slot(i) + ImageOffset;
        for (int p = 0; p < 256 * 256; p++)
        {
            bgra[p * 4 + 0] = _buf[src + p * 4 + 3];
            bgra[p * 4 + 1] = _buf[src + p * 4 + 2];
            bgra[p * 4 + 2] = _buf[src + p * 4 + 1];
            bgra[p * 4 + 3] = _buf[src + p * 4 + 0];
        }
        Marshal.Copy(bgra, 0, data.Scan0, bgra.Length);
        bmp.UnlockBits(data);
        using var ms = new MemoryStream();
        bmp.Save(ms, ImageFormat.Png);
        return ms.ToArray();
    }

    // Puts an image file (PNG, JPG, BMP, GIF) into slot i: fitted into 256x256, centred, transparency kept.
    public void Import(int i, string imageFile)
    {
        var argb = LoadImage(imageFile);
        var (palette, indices) = Quantize(argb);
        int s = Slot(i);
        // Keep the slot's date (the game writes the same save date into every slot).
        var date = _buf.AsSpan(s + DateOffset, 8).ToArray();
        Array.Clear(_buf, s, SlotSize);
        UserLogoHeader.CopyTo(_buf, s);
        // A distinct id per logo (word at +0x2C), like the game's own logos.
        BinaryPrimitives.WriteUInt32BigEndian(_buf.AsSpan(s + 0x2C), (uint)Random.Shared.Next(1, int.MaxValue));
        argb.CopyTo(_buf, s + ImageOffset);
        Info.CopyTo(_buf, s + InfoOffset);
        for (int c = 0; c < 256; c++)
            BinaryPrimitives.WriteUInt32BigEndian(_buf.AsSpan(s + PaletteOffset + c * 4), palette[c]);
        indices.CopyTo(_buf, s + IndexOffset);
        date.CopyTo(_buf, s + DateOffset);
    }

    // Empties slot i, like a slot the game never used. The game's own default logo can't be removed.
    public void Clear(int i)
    {
        int s = Slot(i);
        var date = _buf.AsSpan(s + DateOffset, 8).ToArray();
        Array.Clear(_buf, s, SlotSize);
        BinaryPrimitives.WriteUInt32BigEndian(_buf.AsSpan(s + 8), 7);
        date.CopyTo(_buf, s + DateOffset);
    }

    // Moves the logos to the front, in order, so the game's Logos list has no empty gaps.
    public void Compact()
    {
        int to = 0;
        for (int from = 0; from < SlotCount; from++)
        {
            if (!IsUsed(from)) continue;
            if (from != to)
            {
                Buffer.BlockCopy(_buf, Slot(from), _buf, Slot(to), SlotSize);
                Clear(from);
            }
            to++;
        }
    }

    // --- PS3 saves ------------------------------------------------------------------------------------------------------

    // The PS3 version keeps the same buffer in LOGOS.DAT (same layout and checksums), as "ZLIB" chunks with a 20-byte
    // header: magic, then little-endian version 0x3012, compressed size (zlib header + deflate + Adler-32), uncompressed
    // size, flags. Chunk k fills the buffer from k * 2 MB. Save editors often leave the size and Adler-32 fields stale
    // (the game doesn't check them), so chunks go to their fixed offsets and the streams aren't verified.
    const int Ps3ChunkSpan = 0x200000;

    public static PaintToolSave FromPs3Logos(byte[] data, string source)
    {
        var buf = new byte[Size];
        int o = 0, chunk = 0;
        while (o + 20 <= data.Length && data[o] == 'Z' && data[o + 1] == 'L' && data[o + 2] == 'I' && data[o + 3] == 'B')
        {
            int csize = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(o + 8));
            int usize = BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(o + 12));
            if (csize < 6 || o + 20 + csize > data.Length)
                throw new InvalidDataException("A compressed chunk is cut short.");
            int dest = chunk * Ps3ChunkSpan;
            int want = Math.Min(usize, Size - dest);
            if (want > 0)
            {
                using var deflate = new DeflateStream(new MemoryStream(data, o + 22, csize - 6), CompressionMode.Decompress);
                for (int got = 0, n; got < want && (n = deflate.Read(buf, dest + got, want - got)) > 0;) got += n;
            }
            o += 20 + csize;
            chunk++;
        }
        if (chunk == 0 || BinaryPrimitives.ReadUInt32BigEndian(buf) != 0x02A78E9A)
            throw new InvalidDataException("This is not a WWE 2K14 PS3 logo file.");
        return new PaintToolSave(source, buf);
    }

    public bool SameImage(int slot, PaintToolSave other, int otherSlot) =>
        _buf.AsSpan(Slot(slot) + ImageOffset, InfoOffset - ImageOffset)
            .SequenceEqual(other._buf.AsSpan(Slot(otherSlot) + ImageOffset, InfoOffset - ImageOffset));

    // Copies a whole logo (header, both images, palette, date) from another save; it becomes a user logo, so it can be
    // removed like any other.
    public void CopySlot(PaintToolSave from, int fromSlot, int to)
    {
        Buffer.BlockCopy(from._buf, Slot(fromSlot), _buf, Slot(to), SlotSize);
        BinaryPrimitives.WriteUInt32BigEndian(_buf.AsSpan(Slot(to) + 4), 0);
    }

    // --- Single logos (the launcher's logo library keeps each one as its raw slot) ---------------------------------------

    // An empty save in memory, used to convert images into slots and render stored slots.
    public static PaintToolSave Blank() => new("", new byte[Size]);

    public byte[] ExportSlot(int i) => _buf.AsSpan(Slot(i), SlotSize).ToArray();

    // Puts a stored slot into slot i as a user logo; the save's own date for that slot is kept.
    public void PutSlot(int i, byte[] slot)
    {
        if (slot.Length != SlotSize) throw new InvalidDataException("A library logo has the wrong size.");
        int s = Slot(i);
        var date = _buf.AsSpan(s + DateOffset, 8).ToArray();
        slot.CopyTo(_buf, s);
        BinaryPrimitives.WriteUInt32BigEndian(_buf.AsSpan(s + 4), 0);
        if (date.Any(b => b != 0)) date.CopyTo(_buf, s + DateOffset);
    }

    // Identifies a logo by its picture, so the same logo in the save and in the library is recognised.
    public string ImageId(int i) => ImageId(_buf.AsSpan(Slot(i), SlotSize));

    public static string ImageId(ReadOnlySpan<byte> slot) =>
        Convert.ToHexString(System.Security.Cryptography.SHA1.HashData(slot.Slice(ImageOffset, InfoOffset - ImageOffset)))[..16]
            .ToLowerInvariant();

    // --- Image conversion ----------------------------------------------------------------------------------------------

    // 256x256, A R G B bytes.
    static byte[] LoadImage(string file)
    {
        using var source = Image.FromFile(file);
        using var bmp = new Bitmap(256, 256, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(bmp))
        {
            g.Clear(Color.Transparent);
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.CompositingQuality = CompositingQuality.HighQuality;
            double f = Math.Min(256.0 / source.Width, 256.0 / source.Height);
            int w = Math.Max(1, (int)Math.Round(source.Width * f)), h = Math.Max(1, (int)Math.Round(source.Height * f));
            using var attributes = new ImageAttributes();
            attributes.SetWrapMode(WrapMode.TileFlipXY);   // no dark fringe at the edges
            g.DrawImage(source, new Rectangle((256 - w) / 2, (256 - h) / 2, w, h), 0, 0, source.Width, source.Height,
                        GraphicsUnit.Pixel, attributes);
        }
        var data = bmp.LockBits(new Rectangle(0, 0, 256, 256), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var bgra = new byte[256 * 256 * 4];
        Marshal.Copy(data.Scan0, bgra, 0, bgra.Length);
        bmp.UnlockBits(data);
        var argb = new byte[bgra.Length];
        for (int p = 0; p < 256 * 256; p++)
        {
            byte a = bgra[p * 4 + 3];
            argb[p * 4 + 0] = a;
            // Fully transparent pixels are stored as 0, like the game's own logos.
            argb[p * 4 + 1] = a == 0 ? (byte)0 : bgra[p * 4 + 2];
            argb[p * 4 + 2] = a == 0 ? (byte)0 : bgra[p * 4 + 1];
            argb[p * 4 + 3] = a == 0 ? (byte)0 : bgra[p * 4 + 0];
        }
        return argb;
    }

    // 256-colour palette (A R G B words) + indices, by median cut over the visible pixels; entry 0 is transparent.
    static (uint[] Palette, byte[] Indices) Quantize(byte[] argb)
    {
        const int n = 256 * 256;
        var colors = new Dictionary<uint, int>();
        for (int p = 0; p < n; p++)
        {
            uint c = BinaryPrimitives.ReadUInt32BigEndian(argb.AsSpan(p * 4));
            if ((c >> 24) == 0) continue;
            colors[c] = colors.GetValueOrDefault(c) + 1;
        }
        var palette = new uint[256];
        var boxes = new List<List<(uint Color, int Count)>>();
        if (colors.Count > 0) boxes.Add(colors.Select(kv => (kv.Key, kv.Value)).ToList());
        static int Channel(uint c, int ch) => (int)(c >> (24 - ch * 8)) & 0xFF;
        while (boxes.Count < 255)
        {
            // Split the box with the widest channel range (weighted by pixel count).
            int best = -1, bestChannel = 0;
            long bestScore = 0;
            for (int b = 0; b < boxes.Count; b++)
            {
                if (boxes[b].Count < 2) continue;
                for (int ch = 0; ch < 4; ch++)
                {
                    int min = 255, max = 0;
                    long count = 0;
                    foreach (var (c, k) in boxes[b])
                    {
                        int v = Channel(c, ch);
                        if (v < min) min = v;
                        if (v > max) max = v;
                        count += k;
                    }
                    long score = (long)(max - min) * count;
                    if (score > bestScore) { bestScore = score; best = b; bestChannel = ch; }
                }
            }
            if (best < 0) break;
            var box = boxes[best];
            box.Sort((x, y) => Channel(x.Color, bestChannel).CompareTo(Channel(y.Color, bestChannel)));
            long total = box.Sum(e => (long)e.Count), half = 0;
            int cut = 0;
            while (cut < box.Count - 1 && half + box[cut].Count <= total / 2) half += box[cut++].Count;
            if (cut == 0) cut = 1;
            boxes[best] = box.GetRange(0, cut);
            boxes.Add(box.GetRange(cut, box.Count - cut));
        }
        for (int b = 0; b < boxes.Count; b++)
        {
            long sa = 0, sr = 0, sg = 0, sb = 0, k = 0;
            foreach (var (c, cnt) in boxes[b])
            {
                sa += Channel(c, 0) * (long)cnt; sr += Channel(c, 1) * (long)cnt;
                sg += Channel(c, 2) * (long)cnt; sb += Channel(c, 3) * (long)cnt; k += cnt;
            }
            palette[b + 1] = (uint)(sa / k) << 24 | (uint)(sr / k) << 16 | (uint)(sg / k) << 8 | (uint)(sb / k);
        }
        int used = boxes.Count + 1;
        var indices = new byte[n];
        var cache = new Dictionary<uint, byte>();
        for (int p = 0; p < n; p++)
        {
            uint c = BinaryPrimitives.ReadUInt32BigEndian(argb.AsSpan(p * 4));
            if ((c >> 24) == 0) { indices[p] = 0; continue; }
            if (!cache.TryGetValue(c, out byte idx))
            {
                long bestDist = long.MaxValue;
                for (int e = 1; e < used; e++)
                {
                    long d = 0;
                    for (int ch = 0; ch < 4; ch++)
                    {
                        int diff = Channel(c, ch) - Channel(palette[e], ch);
                        d += diff * diff;
                    }
                    if (d < bestDist) { bestDist = d; idx = (byte)e; }
                }
                cache[c] = idx;
            }
            indices[p] = idx;
        }
        return (palette, indices);
    }

    // --- Checksums (sub_8341A528) --------------------------------------------------------------------------------------

    uint Words(int start, int end)
    {
        uint sum = 0;
        for (int o = start; o < end; o += 4) sum += U32(o);
        return sum;
    }

    uint SlotSum(int i)
    {
        int s = Slot(i);
        uint sum = Words(s, s + 0x30) + Words(s + 0x38, s + 0x48);
        for (int k = 0x30; k < 0x38; k++) sum += (uint)(sbyte)_buf[s + k];
        sum += Words(s + ImageOffset, s + InfoOffset);
        int b = s + InfoOffset;
        foreach (int k in new[] { 0, 1, 2, 7, 16, 17 }) sum += _buf[b + k];
        foreach (int k in new[] { 3, 5, 8, 10, 12, 14, 18 }) sum += BinaryPrimitives.ReadUInt16BigEndian(_buf.AsSpan(b + k));
        sum += Words(b + 20, b + 20 + 0x400);
        for (int k = 0; k < 0x10000; k++) sum += _buf[b + 0x414 + k];
        int e = s + DateOffset;
        sum += BinaryPrimitives.ReadUInt16BigEndian(_buf.AsSpan(e));
        for (int k = 2; k < 8; k++) sum += _buf[e + k];
        return sum;
    }

    void UpdateChecksums()
    {
        uint total = U32(0) + U32(4);
        for (int i = 0; i < SlotCount; i++)
        {
            uint v = SlotSum(i);
            BinaryPrimitives.WriteUInt32BigEndian(_buf.AsSpan(Slot(i) + SlotSumOffset), v);
            total += v;
        }
        BinaryPrimitives.WriteUInt32BigEndian(_buf.AsSpan(TotalOffset), total);
    }

    public bool ChecksumsValid()
    {
        uint total = U32(0) + U32(4);
        for (int i = 0; i < SlotCount; i++)
        {
            uint v = SlotSum(i);
            if (U32(Slot(i) + SlotSumOffset) != v) return false;
            total += v;
        }
        return U32(TotalOffset) == total;
    }
}
