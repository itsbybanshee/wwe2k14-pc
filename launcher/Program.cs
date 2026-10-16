namespace Wwe2k14Launcher;

static class Program
{
    [STAThread]
    static int Main(string[] args)
    {
        // Scripted UI packer: WWE2K14Launcher.exe --pack-ui <uiDir> <outputFile>
        if (args.Length == 3 && args[0] == "--pack-ui")
        {
            UiBundle.Pack(args[1], args[2]);
            Console.WriteLine($"Encrypted UI packed to: {args[2]}");
            return 0;
        }
        // Scripted install: WWE2K14Launcher.exe --install-tu <package> (same as the Title update button).
        if (args.Length == 2 && args[0] == "--install-tu")
        {
            string? error = new TitleUpdate(new LauncherPaths()).Install(args[1]);
            Console.Error.WriteLine(error ?? "Title update installed.");
            return error is null ? 0 : 1;
        }
        // Scripted logo import: WWE2K14Launcher.exe --import-logo <image> (same as the Logos tab's Add logo).
        // Scripted PS3 logo import: --import-ps3-logos <LOGOS.DAT | save folder | .zip | .rar> (the Import PS3 save button).
        if (args.Length == 2 && args[0] == "--import-ps3-logos")
        {
            string? error = LogoStore.ImportPs3(new LauncherPaths(), args[1], out string message);
            Console.Error.WriteLine(error ?? message);
            return error is null ? 0 : 1;
        }
        // Scripted mod staging for a disc image: --stage-image-mods <game .iso> (what Play does before starting the game).
        if (args.Length == 2 && args[0] == "--stage-image-mods")
        {
            var (overlay, _) = new ModStore(new LauncherPaths()).PrepareImageLaunch(args[1], null);
            Console.Error.WriteLine(overlay.Length > 0 ? $"Mods staged in {overlay}" : "No mods enabled.");
            return 0;
        }
        if (args.Length == 2 && args[0] == "--import-logo")
        {
            var paths = new LauncherPaths();
            string? error = LogoStore.Add(paths, [args[1]], out string message);
            Console.Error.WriteLine(error ?? message);
            return error is null ? 0 : 1;
        }
        if (args.Length == 2 && args[0] == "--install-tc")
        {
            var paths = new LauncherPaths();
            var dlc = new DlcStore(paths.UserData);
            var tc = new TotalConversion(paths, dlc);
            var res = tc.Install(args[1], (step, cur, tot) => Console.WriteLine($"[{cur}/{tot}] {step}"));
            Console.WriteLine(res.Message);
            if (res.Ok)
            {
                var settings = LauncherSettings.Load(paths.SettingsFile);
                if (res.GameFolder != null)
                {
                    settings.GameFolder = res.GameFolder;
                    settings.UseTitleUpdate = true;
                    settings.Save(paths.SettingsFile);
                    Console.WriteLine($"Configured GameFolder: {res.GameFolder}");
                    Console.WriteLine("Enabled TitleUpdate.");
                }
            }
            return res.Ok ? 0 : 1;
        }
        if (args.Length == 1 && args[0] == "--restore-vanilla")
        {
            var paths = new LauncherPaths();
            var dlc = new DlcStore(paths.UserData);
            var tc = new TotalConversion(paths, dlc);
            bool ok = tc.RestoreVanilla(out string msg);
            Console.WriteLine(msg);
            if (ok)
            {
                var settings = LauncherSettings.Load(paths.SettingsFile);
                settings.UseTitleUpdate = false;
                if (!string.IsNullOrEmpty(paths.DefaultGameFolder))
                    settings.GameFolder = paths.DefaultGameFolder;
                settings.Save(paths.SettingsFile);
            }
            return ok ? 0 : 1;
        }
        if (args.Length == 1 && args[0] == "--test-staging")
        {
            var paths = new LauncherPaths();
            var settings = LauncherSettings.Load(paths.SettingsFile);
            var mods = new ModStore(paths);
            var (staged, _) = mods.PrepareLaunch(settings.GameFolder, null);
            Console.WriteLine($"Staged: {staged}");
            string ch131 = Path.Combine(staged, "pac", "ch", "ch131.pac");
            Console.WriteLine($"ch131.pac exists: {File.Exists(ch131)}");
            mods.CleanupStaged();
            Console.WriteLine($"Cleaned: {!Directory.Exists(staged)}");
            return 0;
        }
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
        return 0;
    }
}
