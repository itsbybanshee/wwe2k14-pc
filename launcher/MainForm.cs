using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

namespace Wwe2k14Launcher;

// A window holding the launcher page (ui\index.html, styled after grapsheet.com). The page talks to this class with
// JSON messages: {type: "init" | "save" | "play" | "browseGame" | "addDlcFiles" | "addDlcFolder" | "removeDlc" |
// "openFolder"}; it receives {type: "state" | "dlcProgress" | "dlcResult" | "toast" | "gameRunning"}.
public sealed class MainForm : Form
{
    const string Host = "launcher.wwe2k14";
    const string DataHost = "data.wwe2k14";

    readonly WebView2 _web = new() { Dock = DockStyle.Fill, DefaultBackgroundColor = Color.FromArgb(7, 11, 18) };
    readonly LauncherPaths _paths = new();
    readonly DlcStore _dlc;
    readonly TitleUpdate _tu;
    readonly ModStore _mods;
    Process? _tuBuild;
    bool _tuBusy;
    LauncherSettings _settings;
    Process? _game;
    bool _dlcBusy;

    static readonly JsonSerializerOptions Json = new() { PropertyNamingPolicy = JsonNamingPolicy.CamelCase };

    public MainForm()
    {
        Text = "WWE 2K14 Launcher";
        Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);   // res\wwe2k14.ico, embedded at build time
        BackColor = Color.FromArgb(7, 11, 18);
        int targetWidth = 1120;
        int targetHeight = 790;
        var workArea = Screen.PrimaryScreen?.WorkingArea ?? SystemInformation.WorkingArea;
        if (targetHeight > workArea.Height - 30)
            targetHeight = Math.Max(680, workArea.Height - 30);
        if (targetWidth > workArea.Width - 30)
            targetWidth = Math.Max(960, workArea.Width - 30);
        StartPosition = FormStartPosition.CenterScreen;
        ClientSize = new Size(targetWidth, targetHeight);
        MinimumSize = new Size(960, 680);
        Opacity = 0;
        Controls.Add(_web);

        _dlc = new DlcStore(_paths.UserData);
        _tu = new TitleUpdate(_paths);
        _mods = new ModStore(_paths);
        _settings = LauncherSettings.Load(_paths.SettingsFile);
        if (_settings.GameFolder.Length == 0)
            _settings.GameFolder = _paths.DefaultGameFolder;
        if (!File.Exists(_paths.SettingsFile))
        {
            try { _settings.Save(_paths.SettingsFile); } catch (IOException) { }
        }
    }

    Task? _initTask;

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        int dark = 1;   // dark title bar (DWMWA_USE_IMMERSIVE_DARK_MODE)
        DwmSetWindowAttribute(Handle, 20, ref dark, sizeof(int));
        _initTask = InitWebViewAsync();
    }

    async Task InitWebViewAsync()
    {
        string webData = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                                      "WWE2K14Launcher", "WebView2");
        var options = new CoreWebView2EnvironmentOptions
        {
            AdditionalBrowserArguments = "--disable-features=msSmartScreenProtection,Translate,MediaRouter,OptimizationHints,msEdgeFluentOverlayScrollbar " +
                                         "--disable-background-networking " +
                                         "--disable-component-update " +
                                         "--disable-sync " +
                                         "--no-pings " +
                                         "--disable-extensions " +
                                         "--disable-default-apps " +
                                         "--disable-breakpad " +
                                         "--no-first-run " +
                                         "--no-default-browser-check " +
                                         "--renderer-process-limit=1"
        };
        try
        {
            var env = await CoreWebView2Environment.CreateAsync(null, webData, options);
            await _web.EnsureCoreWebView2Async(env);
        }
        catch (WebView2RuntimeNotFoundException)
        {
            MessageBox.Show(this, "The Microsoft Edge WebView2 Runtime is missing. Install it from microsoft.com, then start the launcher again.",
                            Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            Close();
            return;
        }
        catch
        {
            // Fallback for folder lock or unexpected initialization issue
            try
            {
                string fallback = Path.Combine(Path.GetTempPath(), "WWE2K14Launcher_WV2_" + Environment.ProcessId);
                var env = await CoreWebView2Environment.CreateAsync(null, fallback, options);
                await _web.EnsureCoreWebView2Async(env);
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, $"Failed to initialize launcher interface: {ex.Message}",
                                Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
                Close();
                return;
            }
        }
        var core = _web.CoreWebView2;
        if (core == null) return;
        core.Settings.AreDefaultContextMenusEnabled = false;
        core.Settings.IsZoomControlEnabled = false;
        core.Settings.IsStatusBarEnabled = false;
#if !DEBUG
        core.Settings.AreDevToolsEnabled = false;
#endif
        if (UiBundle.HasEncryptedBundle())
        {
            core.AddWebResourceRequestedFilter($"https://{Host}/*", CoreWebView2WebResourceContext.All);
            core.WebResourceRequested += (sender, args) =>
            {
                string uri = args.Request.Uri;
                string prefix = $"https://{Host}/";
                if (!uri.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) return;
                string relative = uri.Substring(prefix.Length).Split('?')[0].TrimStart('/');
                if (string.IsNullOrEmpty(relative)) relative = "index.html";

                byte[]? data = UiBundle.GetAsset(relative);
                if (data != null)
                {
                    string mime = UiBundle.GetMimeType(relative);
                    var stream = new MemoryStream(data);
                    args.Response = core.Environment.CreateWebResourceResponse(
                        stream, 200, "OK", $"Content-Type: {mime}\r\nCache-Control: max-age=3600\r\n");
                }
                else
                {
                    args.Response = core.Environment.CreateWebResourceResponse(
                        null, 404, "Not Found", "Content-Type: text/plain\r\n");
                }
            };
        }
        else
        {
            core.SetVirtualHostNameToFolderMapping(Host, Path.Combine(AppContext.BaseDirectory, "ui"),
                                                   CoreWebView2HostResourceAccessKind.Allow);
        }
        // Achievement icons, extracted by the game from its own data into userdata\achievements.
        string achievementData = Path.Combine(_paths.UserData, "achievements");
        Directory.CreateDirectory(achievementData);
        core.SetVirtualHostNameToFolderMapping(DataHost, achievementData, CoreWebView2HostResourceAccessKind.Allow);
        core.WebMessageReceived += OnWebMessage;
        // Links (none yet) open in the real browser, never inside the launcher.
        core.NewWindowRequested += (_, args) => { args.Handled = true; };
        core.NavigationCompleted += (_, _) => BeginInvoke(() => { if (Opacity < 1) Opacity = 1; });
        core.Navigate($"https://{Host}/index.html");
    }

    protected override async void OnLoad(EventArgs e)
    {
        base.OnLoad(e);
        if (_initTask != null)
        {
            await _initTask;
        }
        _ = Task.Delay(1000).ContinueWith(_ => {
            if (IsHandleCreated && !IsDisposed)
            {
                BeginInvoke(() => { if (Opacity < 1) Opacity = 1; });
            }
        });
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        base.OnFormClosing(e);
        DisplayHelper.RestoreOriginal();
        _web.Dispose();
    }

    // --- Messages from the page ---------------------------------------------------------------------------------------

    void OnWebMessage(object? sender, CoreWebView2WebMessageReceivedEventArgs e)
    {
        JsonNode? msg;
        try { msg = JsonNode.Parse(e.WebMessageAsJson); }
        catch (JsonException) { return; }
        switch ((string?)msg?["type"])
        {
            case "init":
                SendState();
                if (Opacity < 1) Opacity = 1;
                break;
            case "save":
                ApplySettings(msg!["settings"]);
                break;
            case "play":
                ApplySettings(msg!["settings"]);
                Play();
                break;
            case "browseGame":
                BrowseGameFolder();
                break;
            case "browseGameImage":
                BrowseGameImage();
                break;
            case "addDlcFiles":
                AddDlcFiles();
                break;
            case "addDlcFolder":
                AddDlcFolder();
                break;
            case "removeDlc":
                RemoveDlc((string?)msg!["name"]);
                break;
            case "openFolder":
                OpenFolder((string?)msg!["which"]);
                break;
            case "addMusic":
                AddMusic();
                break;
            case "removeMusic":
                RemoveMusic((string?)msg!["name"]);
                break;
            case "installTu":
                InstallTitleUpdate();
                break;
            case "buildTu":
                BuildTitleUpdate();
                break;
            case "removeTu":
                RemoveTitleUpdate();
                break;
            case "importPs3Logos":
                ImportPs3Logos();
                break;
            case "addLogo":
                AddLogos();
                break;
            case "toggleLibraryLogo":
                ToggleLibraryLogo((string?)msg!["id"]);
                break;
            case "deleteLibraryLogo":
                DeleteLibraryLogo((string?)msg!["id"]);
                break;
            case "exportControls":
                ExportControls(msg!["scheme"]);
                break;
            case "importControls":
                ImportControls();
                break;
            case "removeLogo":
                RemoveLogo((int?)msg!["slot"]);
                break;
            case "addModFiles":
                AddModFiles();
                break;
            case "addModFolder":
                AddModFolder();
                break;
            case "toggleMod":
                ToggleMod((string?)msg!["id"], (bool?)msg!["enabled"] ?? true);
                break;
            case "removeMod":
                RemoveMod((string?)msg!["id"]);
                break;
            case "installTotalConversion":
                InstallTotalConversion();
                break;
            case "restoreVanilla":
                RestoreVanilla();
                break;
            case "openUrl":
                OpenUrl((string?)msg!["url"]);
                break;
        }
    }

    void ApplySettings(JsonNode? node)
    {
        if (node is null) return;
        var s = node.Deserialize<LauncherSettings>(Json);
        if (s is null) return;
        if (!LauncherSettings.Resolutions.Contains(s.Resolution)) s.Resolution = "720";
        if (!LauncherSettings.AnisoOptions.Contains((s.Anisotropic ?? "").ToLowerInvariant())) s.Anisotropic = "16x";
        if (!LauncherSettings.LanguageIds.ContainsKey((s.Language ?? "").ToLowerInvariant())) s.Language = "english";
        if (!LauncherSettings.AaOptions.Contains((s.AntiAliasing ?? "").ToLowerInvariant())) s.AntiAliasing = "fxaa";
        if (!LauncherSettings.UpscalerOptions.Contains((s.Upscaler ?? "").ToLowerInvariant())) s.Upscaler = "off";
        if (string.IsNullOrWhiteSpace(s.WindowMode)) s.WindowMode = s.Fullscreen ? "fullscreen" : "borderless";
        else if (!LauncherSettings.WindowModeOptions.Contains(s.WindowMode.ToLowerInvariant())) s.WindowMode = "borderless";
        s.Fullscreen = s.WindowMode == "fullscreen";
        _settings = s;
        try { _settings.Save(_paths.SettingsFile); }
        catch (IOException ex) { Toast($"Settings could not be saved: {ex.Message}", true); }
    }

    // --- State --------------------------------------------------------------------------------------------------------

    void SendState()
    {
        bool gameOk = GameFolderStatus(_settings.GameFolder, out string gameMessage);
        Post(new
        {
            type = "state",
            settings = _settings,
            game = new { ok = gameOk, message = gameMessage },
            exe = new { ok = File.Exists(_paths.GameExe), path = _paths.GameExe },
            running = _game is { HasExited: false },
            dlc = _dlc.List(),
            mods = _mods.List(),
            achievements = ReadAchievements(),
            music = ListMusic(),
            logos = ListLogos(),
            tu = new { installed = _tu.Installed, built = _tu.ExeBuilt, building = _tuBusy, name = _tu.Name },
            tc = new { busy = _tcBusy, hasBackup = Directory.Exists(Path.Combine(_paths.UserData, "backups", "vanilla_save")) },
            screen = new { width = Screen.FromControl(this).Bounds.Width, height = Screen.FromControl(this).Bounds.Height },
        });
    }

    // Written by the game (SDK achievements.cpp) once it has loaded: every achievement with its unlock state.
    JsonNode? ReadAchievements()
    {
        string path = Path.Combine(_paths.UserData, "achievements", $"{DlcStore.TitleId:X8}.json");
        try { return File.Exists(path) ? JsonNode.Parse(File.ReadAllText(path)) : null; }
        catch (Exception ex) when (ex is IOException or JsonException) { return null; }
    }

    // The game is an extracted disc folder or a disc image (.iso); the game mounts an image itself (src/disc_image.cpp).
    static bool IsDiscImage(string path) => path.EndsWith(".iso", StringComparison.OrdinalIgnoreCase);

    static bool GameFolderStatus(string folder, out string message)
    {
        if (folder.Length == 0) { message = "Choose the folder of your extracted WWE 2K14 disc, or its .iso."; return false; }
        if (IsDiscImage(folder))
        {
            if (!File.Exists(folder)) { message = "This disc image no longer exists."; return false; }
            if (!HasXboxDiscHeader(folder)) { message = "Not an Xbox 360 disc image."; return false; }
            message = "Disc image ready";
            return true;
        }
        if (!Directory.Exists(folder)) { message = "This folder no longer exists."; return false; }
        if (!File.Exists(Path.Combine(folder, "default.xex"))) { message = "No default.xex in this folder."; return false; }
        if (!Directory.Exists(Path.Combine(folder, "pac"))) { message = "The pac folder is missing: use the full disc contents."; return false; }
        message = "Game files ready";
        return true;
    }

    // Xbox 360 disc images have "MICROSOFT*XBOX*MEDIA" 32 sectors into the game partition, which starts at one of these
    // offsets depending on the disc layout (plain, XGD2, XGD3, XGD1).
    static bool HasXboxDiscHeader(string path) => XdvdfsReader.IsXboxImage(path);

    void BrowseGameImage()
    {
        using var dialog = new OpenFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Choose your WWE 2K14 disc image"),
            Filter = "Xbox 360 disc image (*.iso)|*.iso|All files|*.*",
            InitialDirectory = IsDiscImage(_settings.GameFolder) ? Path.GetDirectoryName(_settings.GameFolder) ?? "" : "",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        if (!string.IsNullOrWhiteSpace(_settings.GameFolder) && _settings.GameFolder != dialog.FileName)
            _settings.PreviousGameFolder = _settings.GameFolder;
        _settings.GameFolder = dialog.FileName;
        try { _settings.Save(_paths.SettingsFile); } catch (IOException) { }
        SendState();
    }

    void BrowseGameFolder()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = DialogText.Get(_settings.Language, "Choose the folder of your extracted WWE 2K14 disc (the one containing default.xex)"),
            UseDescriptionForTitle = true,
            InitialDirectory = Directory.Exists(_settings.GameFolder) ? _settings.GameFolder : "",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;

        if (!string.IsNullOrWhiteSpace(_settings.GameFolder) && Directory.Exists(_settings.GameFolder) && _settings.GameFolder != dialog.SelectedPath)
        {
            _settings.PreviousGameFolder = _settings.GameFolder;
        }

        _settings.GameFolder = dialog.SelectedPath;
        try { _settings.Save(_paths.SettingsFile); } catch (IOException) { }
        SendState();
    }

    static bool IsTitleUpdateXex(string xexPath)
    {
        try
        {
            if (!File.Exists(xexPath)) return false;
            using var f = File.OpenRead(xexPath);
            Span<byte> head = stackalloc byte[0x50];
            if (f.Read(head) < 0x38) return false;
            for (int i = 0x18; i <= 0x38; i += 8)
            {
                uint key = System.Buffers.Binary.BinaryPrimitives.ReadUInt32BigEndian(head.Slice(i, 4));
                if (key == 0x00010100)
                {
                    uint ep = System.Buffers.Binary.BinaryPrimitives.ReadUInt32BigEndian(head.Slice(i + 4, 4));
                    return ep == 0x822626B8;
                }
            }
        }
        catch { }
        return false;
    }

    // --- Play ---------------------------------------------------------------------------------------------------------

    void Play()
    {
        if (_game is { HasExited: false }) { Toast("The game is already running."); return; }
        if (!GameFolderStatus(_settings.GameFolder, out string gameMessage)) { Toast(gameMessage, true); return; }
        if (!File.Exists(_paths.GameExe)) { Toast("wwe2k14.exe was not found. Build the game first.", true); return; }

        bool fromImage = IsDiscImage(_settings.GameFolder);
        string gameXex = Path.Combine(_settings.GameFolder, "default.xex");
        bool gameIsAlreadyTu = !fromImage && IsTitleUpdateXex(gameXex);  // a disc image is the retail game

        // Standalone total conversions (like Wrestling Legends) already have a TU2 default.xex,
        // so they require wwe2k14_tu.exe to match their entrypoint, but MUST NOT mount userdata\title_update
        // because delta-patching default.xexp on an already-patched XEX causes a signature mismatch crash.
        bool useTu = (_settings.UseTitleUpdate && _tu.Ready) || gameIsAlreadyTu;
        string exe = useTu ? _paths.TitleUpdateExe : _paths.GameExe;
        if (useTu && !File.Exists(_paths.TitleUpdateExe))
        {
            Toast("wwe2k14_tu.exe was not found. Build the title update first.", true);
            return;
        }

        bool mountUpdateRoot = useTu && !gameIsAlreadyTu && _tu.Ready;

        var (scale, width, height) = _settings.Display();
        bool isFullscreen = _settings.WindowMode == "fullscreen";
        bool isBorderless = _settings.WindowMode == "borderless";
        var area = Screen.FromControl(this).WorkingArea;
        var bounds = Screen.FromControl(this).Bounds;

        if (!isFullscreen && !isBorderless && (width > area.Width || height > area.Height))
        {
            double f = Math.Min(area.Width * 0.95 / width, area.Height * 0.95 / height);
            width = (int)(width * f);
            height = (int)(height * f);
        }
        else if (isBorderless)
        {
            width = bounds.Width;
            height = bounds.Height;
        }

        Directory.CreateDirectory(_paths.UserData);
        Directory.CreateDirectory(_paths.Logs);
        var info = new ProcessStartInfo(exe)
        {
            WorkingDirectory = Path.GetDirectoryName(exe)!,
            UseShellExecute = false,
        };
        // With a disc image the mods go to a small overlay folder the game checks before reading the image.
        string gameFolderToLaunch, overlayFolder;
        string? updateFolderToLaunch;
        if (fromImage)
        {
            (overlayFolder, updateFolderToLaunch) = _mods.PrepareImageLaunch(_settings.GameFolder, mountUpdateRoot ? _tu.Folder : null);
            gameFolderToLaunch = _settings.GameFolder;
            if (overlayFolder.Length > 0) info.Environment["WWE2K14_GAME_OVERLAY"] = overlayFolder;
        }
        else
        {
            (gameFolderToLaunch, updateFolderToLaunch) = _mods.PrepareLaunch(_settings.GameFolder, mountUpdateRoot ? _tu.Folder : null);
        }
        info.ArgumentList.Add(gameFolderToLaunch);
        info.ArgumentList.Add($"--user_data_root={_paths.UserData}");
        if (mountUpdateRoot) info.ArgumentList.Add($"--update_data_root={updateFolderToLaunch ?? _tu.Folder}");
        info.ArgumentList.Add($"--log_file={Path.Combine(_paths.Logs, $"run-{DateTime.Now:yyyyMMdd-HHmmss}.log")}");
        info.ArgumentList.Add($"--resolution_scale={scale}");
        info.ArgumentList.Add($"--window_width={width}");
        info.ArgumentList.Add($"--window_height={height}");
        info.ArgumentList.Add(isFullscreen ? "--fullscreen" : "--no-fullscreen");
        info.ArgumentList.Add("--no-enable_console");
        // Custom soundtrack (XMP): your own mp3/flac/wav/ogg files, picked in game like on Xbox 360.
        Directory.CreateDirectory(Path.Combine(_paths.UserData, "music"));
        info.ArgumentList.Add($"--xmp_music_dir={Path.Combine(_paths.UserData, "music")}");
        info.Environment["WWE2K14_VSYNC"] = _settings.Vsync ? "1" : "0";
        info.Environment["WWE2K14_KEEP_60"] = _settings.Keep60 ? "1" : "0";
        info.Environment["WWE2K14_SHADOWS"] = _settings.Shadows ? "1" : "0";
        info.Environment["WWE2K14_FPS_COUNTER"] = _settings.FpsCounter ? "1" : "0";
        info.Environment["WWE2K14_WINDOW_MODE"] = _settings.WindowMode;
        info.Environment["WWE2K14_RESOLUTION"] = _settings.Resolution;
        info.Environment["WWE2K14_WINDOW_WIDTH"] = width.ToString();
        info.Environment["WWE2K14_WINDOW_HEIGHT"] = height.ToString();

        int anisoVal = (_settings.Anisotropic ?? "16x").ToLowerInvariant() switch
        {
            "off" => 0,
            "4x" => 3,
            "16x" => 5,
            _ => 5
        };
        info.ArgumentList.Add($"--anisotropic_override={anisoVal}");
        info.ArgumentList.Add($"--user_language={LauncherSettings.LanguageIds.GetValueOrDefault((_settings.Language ?? "").ToLowerInvariant(), 1)}");
        // Button mapper: keyboard bindings and controller remap (SDK keybind_* and pad_remap settings).
        foreach (var (bind, keys) in _settings.KeyBinds ?? new())
        {
            if (System.Text.RegularExpressions.Regex.IsMatch(bind, "^[a-z_]+$") &&
                System.Text.RegularExpressions.Regex.IsMatch(keys ?? "", "^[A-Za-z0-9,]+$"))
                info.ArgumentList.Add($"--keybind_{bind}={keys}");
        }
        string[] padNames = ["A", "B", "X", "Y", "LB", "RB", "LT", "RT", "START", "BACK", "LS", "RS", "UP", "DOWN", "LEFT", "RIGHT"];
        var pairs = (_settings.PadMap ?? new())
            .Where(p => padNames.Contains(p.Key) && padNames.Contains(p.Value) && p.Key != p.Value)
            .Select(p => $"{p.Key}={p.Value}").ToList();
        if (pairs.Count > 0) info.ArgumentList.Add($"--pad_remap={string.Join(",", pairs)}");
        info.Environment["WWE2K14_ANISO"] = anisoVal.ToString();

        string aaVal = (_settings.AntiAliasing ?? "fxaa").ToLowerInvariant() switch
        {
            "off" => "none",
            "extreme" or "max" => "fxaa_extreme",
            _ => "fxaa"
        };
        info.ArgumentList.Add($"--swap_post_effect={aaVal}");
        info.Environment["WWE2K14_AA"] = aaVal;

        string upscaleVal = (_settings.Upscaler ?? "off").ToLowerInvariant() switch
        {
            "fsr" => "fsr",
            _ => "bilinear"
        };
        info.ArgumentList.Add($"--present_effect={upscaleVal}");
        info.Environment["WWE2K14_UPSCALE"] = _settings.Upscaler ?? "off";

        try
        {
            _game = Process.Start(info);
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            _mods.CleanupStaged();
            Toast($"The game could not start: {ex.Message}", true);
            return;
        }
        if (_game is null)
        {
            _mods.CleanupStaged();
            return;
        }
        DisplayHelper.SwitchTo60Hz();
        _game.EnableRaisingEvents = true;
        _game.Exited += (_, _) => BeginInvoke(() =>
        {
            int code = _game?.ExitCode ?? 0;
            _game = null;
            _mods.CleanupStaged();
            DisplayHelper.RestoreOriginal();
            if (WindowState == FormWindowState.Minimized) WindowState = FormWindowState.Normal;
            Activate();
            Post(new { type = "gameRunning", running = false });
            SendState();   // achievements unlocked during the session
            // Normal exits are 0; anything else is worth a hint (the log has the details).
            if (code != 0) Toast($"The game closed unexpectedly (code {code:X}). The log is in the logs folder.", true);
        });
        Post(new { type = "gameRunning", running = true });
        WindowState = FormWindowState.Minimized;
    }

    // --- DLC ----------------------------------------------------------------------------------------------------------

    void AddDlcFiles()
    {
        using var dialog = new OpenFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Add WWE 2K14 DLC packages"),
            Multiselect = true,
            Filter = "Xbox 360 packages (no extension, .bin, .stfs)|*.*",
        };
        if (dialog.ShowDialog(this) == DialogResult.OK)
            InstallDlc(dialog.FileNames);
    }

    void AddDlcFolder()
    {
        using var dialog = new FolderBrowserDialog
        {
            Description = DialogText.Get(_settings.Language, "Choose a folder with WWE 2K14 DLC packages (subfolders are searched too)"),
            UseDescriptionForTitle = true,
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        var files = Directory.EnumerateFiles(dialog.SelectedPath, "*", new EnumerationOptions
        {
            RecurseSubdirectories = true,
            IgnoreInaccessible = true,
        }).Where(StfsPackage.HasPackageMagic).ToArray();
        if (files.Length == 0)
        {
            Toast("No Xbox 360 packages were found in that folder.", true);
            return;
        }
        InstallDlc(files);
    }

    void InstallDlc(string[] files)
    {
        if (_dlcBusy) { Toast("DLC is already being installed."); return; }
        if (_game is { HasExited: false }) { Toast("Close the game before adding DLC.", true); return; }
        _dlcBusy = true;
        Task.Run(() =>
        {
            var results = new List<DlcStore.InstallResult>();
            for (int i = 0; i < files.Length; i++)
            {
                int index = i;
                Post(new { type = "dlcProgress", file = Path.GetFileName(files[i]), index, count = files.Length, percent = 0 });
                int lastPercent = -1;
                results.Add(_dlc.Install(files[i], (done, total) =>
                {
                    int percent = total > 0 ? (int)(done * 100 / total) : 100;
                    if (percent == lastPercent) return;
                    lastPercent = percent;
                    Post(new { type = "dlcProgress", file = Path.GetFileName(files[index]), index, count = files.Length, percent });
                }));
            }
            BeginInvoke(() =>
            {
                _dlcBusy = false;
                Post(new { type = "dlcResult", results });
                SendState();
            });
        });
    }

    void RemoveDlc(string? name)
    {
        if (string.IsNullOrEmpty(name)) return;
        if (_game is { HasExited: false }) { Toast("Close the game before removing DLC.", true); return; }
        try
        {
            _dlc.Remove(name);
            Toast("DLC removed.");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
        {
            Toast($"Could not remove it: {ex.Message}", true);
        }
        SendState();
    }

    void OpenFolder(string? which)
    {
        string? path = which switch
        {
            "dlc" => _dlc.ContentFolder,
            "mods" => _mods.Folder,
            "userdata" => _paths.UserData,
            "music" => Path.Combine(_paths.UserData, "music"),
            "logs" => _paths.Logs,
            // Written by the game's F12 key (src/screenshot.cpp).
            "screenshots" => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
                                          "wwe2k14", "Screenshots"),
            "game" => IsDiscImage(_settings.GameFolder) ? Path.GetDirectoryName(_settings.GameFolder) : _settings.GameFolder,
            _ => null,
        };
        if (path is null || path.Length == 0) return;
        Directory.CreateDirectory(path);
        Process.Start(new ProcessStartInfo("explorer.exe") { ArgumentList = { path }, UseShellExecute = false });
    }

    // --- Title update (optional; vanilla always stays playable) ----------------------------------------------------------

    void InstallTitleUpdate()
    {
        if (_tuBusy) return;
        if (_game is { HasExited: false }) { Toast("Close the game before installing the title update.", true); return; }
        using var dialog = new OpenFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Choose the WWE 2K14 title update file (e.g. tu00000002_00000000)"),
            Filter = "Xbox 360 title update (tu*)|tu*|All files|*.*",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        _tuBusy = true;
        SendState();
        string file = dialog.FileName;
        Task.Run(() =>
        {
            string? error = _tu.Install(file);
            BeginInvoke(() =>
            {
                _tuBusy = false;
                if (error != null) { Toast(error, true); SendState(); return; }
                Toast("Title update installed.");
                if (!_tu.ExeBuilt) BuildTitleUpdate();
                else { _settings.UseTitleUpdate = true; SaveSettingsQuietly(); SendState(); }
            });
        });
    }

    void BuildTitleUpdate()
    {
        if (_tuBusy || !_tu.Installed) return;
        Process? build;
        try { build = _tu.StartBuild(); }
        catch (System.ComponentModel.Win32Exception ex) { Toast($"The build could not start: {ex.Message}", true); return; }
        if (build is null) { Toast(@"The build tools (tools\build-tu.ps1) are only in the WWE2K14PC project folder.", true); return; }
        _tuBusy = true;
        _tuBuild = build;
        build.EnableRaisingEvents = true;
        build.Exited += (_, _) => BeginInvoke(() =>
        {
            _tuBusy = false;
            _tuBuild = null;
            if (_tu.ExeBuilt) { _settings.UseTitleUpdate = true; SaveSettingsQuietly(); Toast("Title update ready."); }
            else Toast("The title update build failed. The log is in the logs folder.", true);
            SendState();
        });
        SendState();
    }

    void RemoveTitleUpdate()
    {
        if (_tuBusy) return;
        if (_game is { HasExited: false }) { Toast("Close the game before removing the title update.", true); return; }
        try
        {
            _tu.Remove();
            _settings.UseTitleUpdate = false;
            SaveSettingsQuietly();
            Toast("Title update removed. Playing the original version.");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { Toast($"Could not remove it: {ex.Message}", true); }
        SendState();
    }

    void SaveSettingsQuietly()
    {
        try { _settings.Save(_paths.SettingsFile); } catch (IOException) { }
    }

    // --- Custom logos (the game's Paint Tool save, LogoStore.cs) --------------------------------------------------------

    // Rendering the logos takes a moment: cached until the save file or the library changes.
    (string Key, object Value)? _logoCache;

    object ListLogos()
    {
        string? path = LogoStore.SavePath(_paths);
        string library = Path.Combine(_paths.UserData, "logo_library");
        string key = $"{path}|{(path is null ? 0 : File.GetLastWriteTimeUtc(path).Ticks)}|" +
                     $"{(Directory.Exists(library) ? Directory.GetLastWriteTimeUtc(library).Ticks : 0)}";
        if (_logoCache is { } c && c.Key == key) return c.Value;
        var libraryItems = LogoStore.Library(_paths).Select(l => (object)new { id = l.Id, inGame = l.InGame, image = l.Image }).ToArray();
        object value;
        if (path is null)
        {
            value = new { available = false, message = "No logo save yet. In the game, save a logo once in the Paint Tool.", items = Array.Empty<object>(), library = libraryItems };
        }
        else
        {
            var items = LogoStore.List(_paths, out string? error);
            value = items is null
                ? new { available = false, message = $"The logo save could not be read: {error}", items = Array.Empty<object>(), library = libraryItems }
                : new { available = true, message = "", items = items.Select(l => (object)new { slot = l.Slot, preset = l.Preset, image = l.Image }).ToArray(), library = libraryItems };
        }
        _logoCache = (key, value);
        return value;
    }

    void AddLogos()
    {
        if (_game is { HasExited: false }) { Toast("Close the game before changing logos.", true); return; }
        using var dialog = new OpenFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Add custom logos"),
            Multiselect = true,
            Filter = "Images (*.png;*.jpg;*.jpeg;*.bmp;*.gif)|*.png;*.jpg;*.jpeg;*.bmp;*.gif",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        string? error = LogoStore.Add(_paths, dialog.FileNames, out string message);
        Toast(error ?? message, error != null);
        SendState();
    }

    void ImportPs3Logos()
    {
        if (_game is { HasExited: false }) { Toast("Close the game before changing logos.", true); return; }
        using var dialog = new OpenFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Import logos from a WWE 2K14 PS3 save (its LOGOS.DAT, or a .zip / .rar of the save)"),
            Filter = "PS3 save (LOGOS.DAT, .zip, .rar)|LOGOS.DAT;*.zip;*.rar|All files|*.*",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        string? error = LogoStore.ImportPs3(_paths, dialog.FileName, out string message);
        Toast(error ?? message, error != null);
        SendState();
    }

    void ToggleLibraryLogo(string? id)
    {
        if (string.IsNullOrEmpty(id)) return;
        if (_game is { HasExited: false }) { Toast("Close the game before changing logos.", true); return; }
        string? error = LogoStore.Toggle(_paths, id, out _);
        if (error != null) Toast(error, true);
        SendState();
    }

    void DeleteLibraryLogo(string? id)
    {
        if (string.IsNullOrEmpty(id)) return;
        if (_game is { HasExited: false }) { Toast("Close the game before changing logos.", true); return; }
        string? error = LogoStore.Delete(_paths, id, out string message);
        Toast(error ?? message, error != null);
        SendState();
    }

    void RemoveLogo(int? slot)
    {
        if (slot is null) return;
        if (_game is { HasExited: false }) { Toast("Close the game before changing logos.", true); return; }
        string? error = LogoStore.Remove(_paths, slot.Value, out string message);
        Toast(error ?? message, error != null);
        SendState();
    }

    // --- Custom music (userdata\music, used by the game's custom soundtrack feature) ----------------------------------

    static readonly string[] MusicExtensions = [".mp3", ".flac", ".wav", ".ogg"];
    string MusicFolder => Path.Combine(_paths.UserData, "music");

    object[] ListMusic()
    {
        if (!Directory.Exists(MusicFolder)) return [];
        return new DirectoryInfo(MusicFolder).EnumerateFiles()
            .Where(f => MusicExtensions.Contains(f.Extension.ToLowerInvariant()))
            .OrderBy(f => f.Name, StringComparer.OrdinalIgnoreCase)
            .Select(f => (object)new { name = f.Name, title = Path.GetFileNameWithoutExtension(f.Name), sizeBytes = f.Length })
            .ToArray();
    }

    void AddMusic()
    {
        using var dialog = new OpenFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Add songs to the WWE 2K14 custom soundtrack"),
            Multiselect = true,
            Filter = "Music (*.mp3;*.flac;*.wav;*.ogg)|*.mp3;*.flac;*.wav;*.ogg",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        Directory.CreateDirectory(MusicFolder);
        int added = 0;
        foreach (var file in dialog.FileNames)
        {
            string target = Path.Combine(MusicFolder, Path.GetFileName(file));
            if (File.Exists(target)) continue;
            try { File.Copy(file, target); added++; }
            catch (IOException ex) { Toast($"{Path.GetFileName(file)}: {ex.Message}", true); }
        }
        Toast(added == 1 ? "1 song added." : $"{added} songs added.");
        SendState();
    }

    void RemoveMusic(string? name)
    {
        if (string.IsNullOrEmpty(name) || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0) return;
        string path = Path.Combine(MusicFolder, name);
        try { if (File.Exists(path)) File.Delete(path); Toast("Song removed."); }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { Toast($"Could not remove it: {ex.Message}", true); }
        SendState();
    }

    // --- Mods (ModStore.cs) ---------------------------------------------------------------------------------------------

    bool _modBusy;

    void AddModFiles()
    {
        if (_modBusy) return;
        if (_game is { HasExited: false }) { Toast("Close the game before installing mods.", true); return; }
        using var dialog = new OpenFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Select mod package (.zip) or asset files (.pac, .bik)"),
            Multiselect = true,
            Filter = "Mod packages (*.zip;*.pac;*.bik)|*.zip;*.pac;*.bik|All files (*.*)|*.*",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;

        _modBusy = true;
        SendState();
        var files = dialog.FileNames;
        Task.Run(() =>
        {
            int installed = 0;
            string? lastError = null;
            string? lastTitle = null;

            for (int i = 0; i < files.Length; i++)
            {
                var file = files[i];
                ModStore.ModInstallResult res;
                string ext = Path.GetExtension(file).ToLowerInvariant();
                if (ext == ".zip")
                {
                    res = _mods.InstallZip(file, (done, total) =>
                    {
                        int percent = total > 0 ? (int)(done * 100 / total) : 0;
                        Post(new { type = "modProgress", file = Path.GetFileName(file), count = files.Length, index = i, percent });
                    });
                }
                else
                {
                    res = _mods.InstallLooseFile(file);
                }

                if (res.Ok)
                {
                    installed++;
                    lastTitle = res.DisplayName ?? res.File;
                }
                else
                {
                    lastError = res.Message;
                }
            }

            BeginInvoke(() =>
            {
                _modBusy = false;
                Post(new { type = "modProgressDone" });
                if (lastError != null && installed == 0)
                {
                    Toast(lastError, true);
                }
                else if (installed > 0)
                {
                    Toast(installed == 1 ? $"Mod '{lastTitle}' installed." : $"{installed} mods installed.");
                }
                SendState();
            });
        });
    }

    void AddModFolder()
    {
        if (_modBusy) return;
        if (_game is { HasExited: false }) { Toast("Close the game before installing mods.", true); return; }
        using var dialog = new FolderBrowserDialog
        {
            Description = DialogText.Get(_settings.Language, "Select an extracted mod folder (containing pac, stream, or asset files)"),
            UseDescriptionForTitle = true,
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;

        var res = _mods.InstallFolder(dialog.SelectedPath);
        Toast(res.Ok ? $"Mod folder '{res.DisplayName}' imported." : res.Message, !res.Ok);
        SendState();
    }

    void ToggleMod(string? id, bool enabled)
    {
        if (string.IsNullOrEmpty(id)) return;
        _mods.SetEnabled(id, enabled);
        SendState();
    }

    void RemoveMod(string? id)
    {
        if (string.IsNullOrEmpty(id)) return;
        if (_game is { HasExited: false }) { Toast("Close the game before removing mods.", true); return; }
        bool ok = _mods.Remove(id);
        Toast(ok ? "Mod removed." : "Could not remove mod.", !ok);
        SendState();
    }

    // --- Total Conversion (Wrestling Legends etc.) -------------------------------------------------------------------

    bool _tcBusy;

    void InstallTotalConversion()
    {
        if (_tcBusy) return;
        if (_game is { HasExited: false }) { Toast("Close the game before installing a total conversion.", true); return; }

        using var dialog = new FolderBrowserDialog
        {
            Description = DialogText.Get(_settings.Language, "Select your Total Conversion mod folder (e.g. Wrestling Legends 1.1)"),
            UseDescriptionForTitle = true,
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;

        string folder = dialog.SelectedPath;
        _tcBusy = true;
        SendState();

        Task.Run(() =>
        {
            try
            {
                var tc = new TotalConversion(_paths, _dlc);
                var res = tc.Install(folder, (step, current, total) =>
                {
                    int percent = total > 0 ? (current * 100 / total) : 0;
                    Post(new { type = "tcProgress", step, current, total, percent });
                });

                BeginInvoke(() =>
                {
                    _tcBusy = false;
                    Post(new { type = "tcProgressDone" });
                    if (res.Ok)
                    {
                        if (res.GameFolder != null)
                        {
                            if (!string.IsNullOrWhiteSpace(_settings.GameFolder) && _settings.GameFolder != res.GameFolder)
                            {
                                _settings.PreviousGameFolder = _settings.GameFolder;
                                try
                                {
                                    string backupDir = Path.Combine(_paths.UserData, "backups", "vanilla_save");
                                    Directory.CreateDirectory(backupDir);
                                    File.WriteAllText(Path.Combine(backupDir, "game_folder.txt"), _settings.GameFolder);
                                }
                                catch { }
                            }
                            _settings.GameFolder = res.GameFolder;
                            _settings.UseTitleUpdate = true;
                            SaveSettingsQuietly();
                        }
                        Toast(res.Message);
                    }
                    else
                    {
                        Toast(res.Message, true);
                    }
                    SendState();
                });
            }
            catch (Exception ex)
            {
                BeginInvoke(() =>
                {
                    _tcBusy = false;
                    Post(new { type = "tcProgressDone" });
                    Toast($"Total conversion installation failed: {ex.Message}", true);
                    SendState();
                });
            }
        });
    }

    void RestoreVanilla()
    {
        if (_tcBusy) return;
        if (_game is { HasExited: false }) { Toast("Close the game before restoring vanilla.", true); return; }

        string restoredFolder = "";
        string backupFolderFile = Path.Combine(_paths.UserData, "backups", "vanilla_save", "game_folder.txt");

        if (!string.IsNullOrEmpty(_settings.PreviousGameFolder) && Directory.Exists(_settings.PreviousGameFolder))
        {
            restoredFolder = _settings.PreviousGameFolder;
        }
        else if (File.Exists(backupFolderFile))
        {
            try
            {
                string txt = File.ReadAllText(backupFolderFile).Trim();
                if (Directory.Exists(txt)) restoredFolder = txt;
            }
            catch { }
        }

        if (string.IsNullOrEmpty(restoredFolder) && !string.IsNullOrEmpty(_paths.DefaultGameFolder) && Directory.Exists(_paths.DefaultGameFolder))
        {
            restoredFolder = _paths.DefaultGameFolder;
        }

        if (string.IsNullOrEmpty(restoredFolder))
        {
            string commonDisc = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads", "xiso", "WWE 2K14 (USA, Europe)");
            if (Directory.Exists(commonDisc)) restoredFolder = commonDisc;
        }

        if (!string.IsNullOrEmpty(restoredFolder))
        {
            _settings.GameFolder = restoredFolder;
        }

        _settings.UseTitleUpdate = false;

        var tc = new TotalConversion(_paths, _dlc);
        if (tc.RestoreVanilla(out string message))
        {
            SaveSettingsQuietly();
            string folderName = !string.IsNullOrEmpty(_settings.GameFolder) ? Path.GetFileName(_settings.GameFolder) : "";
            Toast(string.IsNullOrEmpty(folderName) ? message : $"{message} Restored game folder to '{folderName}'.");
        }
        else
        {
            SaveSettingsQuietly();
            Toast(message, true);
        }
        SendState();
    }

    // Only these links open: the page can't make the launcher start arbitrary URLs or programs.
    static readonly string[] AllowedUrls = ["https://x.com/itsbybanshee", "https://www.patreon.com/c/Banshee734"];

    static void OpenUrl(string? url)
    {
        if (url is null || !AllowedUrls.Contains(url)) return;
        Process.Start(new ProcessStartInfo(url) { UseShellExecute = true });
    }

    // --- Helpers ------------------------------------------------------------------------------------------------------

    void Toast(string message, bool error = false) => Post(new { type = "toast", message, error });

    // --- Control schemes (Controls > Custom): a JSON file with the controller remap and the keyboard keys ---------------

    static readonly string[] PadButtonNames = ["A", "B", "X", "Y", "LB", "RB", "LT", "RT", "START", "BACK", "LS", "RS", "UP", "DOWN", "LEFT", "RIGHT"];

    // Only known buttons and plain key lists, so a shared file cannot slip anything else onto the game's command line.
    static (Dictionary<string, string> Pad, Dictionary<string, string> Keys)? ReadScheme(JsonNode? scheme)
    {
        if (scheme is not JsonObject obj) return null;
        var pad = new Dictionary<string, string>();
        var keys = new Dictionary<string, string>();
        if (obj["padMap"] is JsonObject padMap)
            foreach (var (from, to) in padMap)
                if (PadButtonNames.Contains(from) && to?.GetValueKind() == JsonValueKind.String && PadButtonNames.Contains((string)to!))
                    pad[from] = (string)to!;
        if (obj["keyBinds"] is JsonObject keyBinds)
            foreach (var (bind, value) in keyBinds)
                if (System.Text.RegularExpressions.Regex.IsMatch(bind, "^[a-z_]{1,32}$") && value?.GetValueKind() == JsonValueKind.String &&
                    System.Text.RegularExpressions.Regex.IsMatch((string)value!, "^[A-Za-z0-9,]{1,80}$"))
                    keys[bind] = (string)value!;
        return (pad, keys);
    }

    void ExportControls(JsonNode? scheme)
    {
        var read = ReadScheme(scheme);
        if (read is null) return;
        using var dialog = new SaveFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Export control scheme"),
            Filter = "Control scheme (*.json)|*.json",
            FileName = "WWE 2K14 controls.json",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        var file = new { game = "WWE 2K14 PC", type = "controls", padMap = read.Value.Pad, keyBinds = read.Value.Keys };
        try
        {
            File.WriteAllText(dialog.FileName, JsonSerializer.Serialize(file, new JsonSerializerOptions { WriteIndented = true }));
            Toast("Control scheme exported.");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            Toast($"Could not save the file: {ex.Message}", true);
        }
    }

    void ImportControls()
    {
        using var dialog = new OpenFileDialog
        {
            Title = DialogText.Get(_settings.Language, "Import control scheme"),
            Filter = "Control scheme (*.json)|*.json|All files|*.*",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        JsonNode? node = null;
        try { node = JsonNode.Parse(File.ReadAllText(dialog.FileName)); }
        catch (Exception ex) when (ex is IOException or JsonException or UnauthorizedAccessException) { }
        var read = node?["type"]?.GetValue<string>() == "controls" ? ReadScheme(node) : null;
        if (read is null)
        {
            Toast("That file is not a control scheme.", true);
            return;
        }
        Post(new { type = "controlsImported", padMap = read.Value.Pad, keyBinds = read.Value.Keys });
    }

    void Post(object message)
    {
        string json = JsonSerializer.Serialize(message, Json);
        if (InvokeRequired)
        {
            BeginInvoke(() => _web.CoreWebView2?.PostWebMessageAsJson(json));
            return;
        }
        _web.CoreWebView2?.PostWebMessageAsJson(json);
    }

    [DllImport("dwmapi.dll")]
    static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);
}
