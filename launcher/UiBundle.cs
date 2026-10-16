using System.Security.Cryptography;
using System.Text;
using Microsoft.Web.WebView2.Core;

namespace Wwe2k14Launcher;

public static class UiBundle
{
    private static readonly byte[] Key = [
        0x57, 0x57, 0x45, 0x32, 0x4B, 0x31, 0x34, 0x5F,
        0x50, 0x43, 0x5F, 0x53, 0x45, 0x43, 0x55, 0x52,
        0x45, 0x5F, 0x55, 0x49, 0x5F, 0x4B, 0x45, 0x59,
        0x5F, 0x32, 0x30, 0x32, 0x36, 0x21, 0x23, 0x2A
    ]; // 32 bytes AES-256

    private static readonly byte[] Iv = [
        0xA3, 0x4F, 0x19, 0x7B, 0x8C, 0x2D, 0x0E, 0xFA,
        0x51, 0x93, 0xB2, 0x48, 0x67, 0xC1, 0xDF, 0x85
    ]; // 16 bytes

    private static readonly byte[] Magic = "WWEUI01"u8.ToArray();

    private static Dictionary<string, byte[]>? _assets;
    private static bool _initialized;

    public static bool HasEncryptedBundle()
    {
        EnsureLoaded();
        return _assets != null && _assets.Count > 0;
    }

    public static byte[]? GetAsset(string relativePath)
    {
        EnsureLoaded();
        if (_assets == null) return null;
        string normalized = relativePath.Replace('\\', '/').TrimStart('/');
        if (_assets.TryGetValue(normalized, out var data)) return data;
        return null;
    }

    public static string GetMimeType(string path)
    {
        string ext = Path.GetExtension(path).ToLowerInvariant();
        return ext switch
        {
            ".html" or ".htm" => "text/html; charset=utf-8",
            ".css" => "text/css; charset=utf-8",
            ".js" => "application/javascript; charset=utf-8",
            ".png" => "image/png",
            ".webp" => "image/webp",
            ".svg" => "image/svg+xml",
            ".ico" => "image/x-icon",
            ".json" => "application/json",
            _ => "application/octet-stream"
        };
    }

    private static void EnsureLoaded()
    {
        if (_initialized) return;
        _initialized = true;

        string here = AppContext.BaseDirectory;
        string[] candidatePaths = [
            Path.Combine(here, "bin", "ui.dat"),
            Path.Combine(here, "ui.dat")
        ];

        string? datPath = candidatePaths.FirstOrDefault(File.Exists);
        if (datPath == null) return;

        try
        {
            byte[] fileBytes = File.ReadAllBytes(datPath);
            if (fileBytes.Length < Magic.Length) return;

            for (int i = 0; i < Magic.Length; i++)
            {
                if (fileBytes[i] != Magic[i]) return;
            }

            using var aes = Aes.Create();
            aes.Key = Key;
            aes.IV = Iv;
            aes.Mode = CipherMode.CBC;
            aes.Padding = PaddingMode.PKCS7;

            using var ms = new MemoryStream(fileBytes, Magic.Length, fileBytes.Length - Magic.Length);
            using var cs = new CryptoStream(ms, aes.CreateDecryptor(), CryptoStreamMode.Read);
            using var reader = new BinaryReader(cs, Encoding.UTF8);

            int count = reader.ReadInt32();
            var dict = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            for (int i = 0; i < count; i++)
            {
                string name = reader.ReadString();
                int len = reader.ReadInt32();
                byte[] content = reader.ReadBytes(len);
                dict[name] = content;
            }
            _assets = dict;
        }
        catch
        {
            _assets = null;
        }
    }

    public static void Pack(string uiDir, string outputFile)
    {
        var files = Directory.GetFiles(uiDir, "*", SearchOption.AllDirectories);
        using var msPlain = new MemoryStream();
        using (var writer = new BinaryWriter(msPlain, Encoding.UTF8, true))
        {
            writer.Write(files.Length);
            foreach (var f in files)
            {
                string rel = Path.GetRelativePath(uiDir, f).Replace('\\', '/');
                byte[] bytes = File.ReadAllBytes(f);
                writer.Write(rel);
                writer.Write(bytes.Length);
                writer.Write(bytes);
            }
        }
        byte[] plainBytes = msPlain.ToArray();

        using var aes = Aes.Create();
        aes.Key = Key;
        aes.IV = Iv;
        aes.Mode = CipherMode.CBC;
        aes.Padding = PaddingMode.PKCS7;

        using var msEnc = new MemoryStream();
        msEnc.Write(Magic);
        using (var cs = new CryptoStream(msEnc, aes.CreateEncryptor(), CryptoStreamMode.Write))
        {
            cs.Write(plainBytes, 0, plainBytes.Length);
        }
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outputFile))!);
        File.WriteAllBytes(outputFile, msEnc.ToArray());
    }
}
