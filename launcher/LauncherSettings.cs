using System.Text.Json;
using System.Text.Json.Serialization;

namespace Wwe2k14Launcher;

public sealed class LauncherSettings
{
    // "720" (native Xbox 360), "1080", "1440", "2160" (4K)
    public string Resolution { get; set; } = "1080";
    public bool Vsync { get; set; } = true;
    public string WindowMode { get; set; } = "borderless";
    public bool Fullscreen { get; set; } = false;
    public bool Keep60 { get; set; } = true;
    public bool LowLatency { get; set; } = true;
    public bool Shadows { get; set; } = true;
    public bool FpsCounter { get; set; } = false;
    public string Anisotropic { get; set; } = "4x";
    // Game text language, passed to the game as the console language (the languages the game has text for).
    public string Language { get; set; } = "english";
    // Button mapper (Controls > Custom). Keyboard: binding name (a, left_trigger, dpad_up...) -> comma-separated keys,
    // only the ones changed from the defaults. Controller: button -> button it acts as (A -> B).
    public Dictionary<string, string> KeyBinds { get; set; } = new();
    public Dictionary<string, string> PadMap { get; set; } = new();
    public string AntiAliasing { get; set; } = "fxaa";
    public string Upscaler { get; set; } = "off";
    // Play with the installed title update (wwe2k14_tu.exe) instead of the vanilla game.
    public bool UseTitleUpdate { get; set; } = true;
    public string GameFolder { get; set; } = "";
    public string PreviousGameFolder { get; set; } = "";

    [JsonIgnore] public static readonly string[] Resolutions = ["720", "1080", "1440", "2160"];
    [JsonIgnore] public static readonly string[] WindowModeOptions = ["windowed", "borderless", "fullscreen"];
    [JsonIgnore] public static readonly string[] AnisoOptions = ["off", "4x", "16x"];
    // Xbox 360 language ids.
    [JsonIgnore] public static readonly Dictionary<string, int> LanguageIds = new()
    {
        ["english"] = 1, ["german"] = 3, ["french"] = 4, ["spanish"] = 5, ["italian"] = 6,
    };
    [JsonIgnore] public static readonly string[] AaOptions = ["off", "fxaa", "extreme", "max"];
    [JsonIgnore] public static readonly string[] UpscalerOptions = ["off", "fsr"];

    public static LauncherSettings Load(string path)
    {
        try
        {
            if (File.Exists(path))
            {
                var s = JsonSerializer.Deserialize<LauncherSettings>(File.ReadAllText(path)) ?? new();
                if (!Resolutions.Contains(s.Resolution)) s.Resolution = "720";
                if (string.IsNullOrWhiteSpace(s.WindowMode)) s.WindowMode = s.Fullscreen ? "fullscreen" : "borderless";
                else if (!WindowModeOptions.Contains(s.WindowMode.ToLowerInvariant())) s.WindowMode = "borderless";
                s.Fullscreen = s.WindowMode == "fullscreen";
                if (!AnisoOptions.Contains((s.Anisotropic ?? "").ToLowerInvariant())) s.Anisotropic = "4x";
                if (!LanguageIds.ContainsKey((s.Language ?? "").ToLowerInvariant())) s.Language = "english";
                if (!AaOptions.Contains((s.AntiAliasing ?? "").ToLowerInvariant())) s.AntiAliasing = "fxaa";
                if (!UpscalerOptions.Contains((s.Upscaler ?? "").ToLowerInvariant())) s.Upscaler = "off";
                return s;
            }
        }
        catch (Exception ex) when (ex is IOException or JsonException) { }
        return new();
    }

    public void Save(string path)
    {
        Fullscreen = WindowMode == "fullscreen";
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
    }

    // Internal render scale on top of the native 1280x720 (the SDK only scales by whole numbers), and the window
    // size that goes with it. 1080p renders at 2x and is shown at 1920x1080: sharper than a true 1080p render.
    // When FSR is enabled, internal render runs at lower scale and is upscaled via FSR EASU + RCAS to display resolution.
    public (int Scale, int Width, int Height) Display()
    {
        if ((Upscaler ?? "").Equals("fsr", StringComparison.OrdinalIgnoreCase))
        {
            return Resolution switch
            {
                "1080" => (1, 1920, 1080),
                "1440" => (1, 2560, 1440),
                "2160" => (2, 3840, 2160),
                _ => (1, 1280, 720),
            };
        }
        return Resolution switch
        {
            "1080" => (2, 1920, 1080),
            "1440" => (2, 2560, 1440),
            // 4K renders natively at 3840x2160 (3x 720p).
            "2160" => (3, 3840, 2160),
            _ => (1, 1280, 720),
        };
    }
}

// Where the game, its saves and logs live.
//   Development: the launcher runs from inside the WWE2K14PC repository -> the repo's build output, userdata\, logs\.
//   Portable:    wwe2k14.exe sits next to the launcher -> userdata\ and logs\ next to it too.
public sealed class LauncherPaths
{
    public string Root { get; }
    public string GameExe { get; }
    public string TitleUpdateExe => Path.Combine(Path.GetDirectoryName(GameExe)!, "wwe2k14_tu.exe");
    public string UserData { get; }
    public string Logs { get; }
    public string SettingsFile { get; }
    public string DefaultGameFolder { get; }
    public string ModsFolder => Path.Combine(Root, "mods");
    public string ActiveModsFolder => Path.Combine(UserData, "mods_active");
    public string StagedGameFolder => Path.Combine(UserData, "game_staged");
    public string StagedUpdateFolder => Path.Combine(UserData, "update_staged");

    public LauncherPaths()
    {
        string here = AppContext.BaseDirectory;
        if (File.Exists(Path.Combine(here, "bin", "wwe2k14.exe")))
        {
            Root = here;
            GameExe = Path.Combine(here, "bin", "wwe2k14.exe");
        }
        else if (File.Exists(Path.Combine(here, "wwe2k14.exe")))
        {
            Root = here;
            GameExe = Path.Combine(here, "wwe2k14.exe");
        }
        else
        {
            string? repo = null;
            for (var dir = new DirectoryInfo(here); dir != null; dir = dir.Parent)
            {
                if (File.Exists(Path.Combine(dir.FullName, "wwe2k14_config.toml")))
                {
                    repo = dir.FullName;
                    break;
                }
            }
            if (repo != null && File.Exists(Path.Combine(repo, "out", "build", "win-amd64", "RelWithDebInfo", "wwe2k14.exe")))
            {
                Root = repo;
                GameExe = Path.Combine(repo, "out", "build", "win-amd64", "RelWithDebInfo", "wwe2k14.exe");
            }
            else if (File.Exists(Path.Combine(here, "..", "wwe2k14.exe")))
            {
                Root = Path.GetFullPath(Path.Combine(here, ".."));
                GameExe = Path.Combine(Root, "wwe2k14.exe");
            }
            else
            {
                Root = repo ?? here;
                GameExe = Path.Combine(Root, "out", "build", "win-amd64", "RelWithDebInfo", "wwe2k14.exe");
            }
        }
        UserData = Path.Combine(Root, "userdata");
        Logs = Path.Combine(Root, "logs");
        SettingsFile = Path.Combine(UserData, "launcher.json");
        DefaultGameFolder = File.Exists(Path.Combine(Root, "game", "default.xex")) && Directory.Exists(Path.Combine(Root, "game", "pac"))
            ? Path.Combine(Root, "game")
            : "";
    }
}
