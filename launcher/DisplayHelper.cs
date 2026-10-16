using System;
using System.Runtime.InteropServices;

namespace Wwe2k14Launcher;

public static class DisplayHelper
{
    private const int CCHDEVICENAME = 32;
    private const int CCHFORMNAME = 32;

    private const int ENUM_CURRENT_SETTINGS = -1;
    private const int DISP_CHANGE_SUCCESSFUL = 0;

    private const uint DM_PELSWIDTH = 0x00080000;
    private const uint DM_PELSHEIGHT = 0x00100000;
    private const uint DM_BITSPERPEL = 0x00040000;
    private const uint DM_DISPLAYFREQUENCY = 0x00400000;
    private const uint DESKTOP_ALL = 0x01FF;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DEVMODE
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = CCHDEVICENAME)]
        public string dmDeviceName;
        public ushort dmSpecVersion;
        public ushort dmDriverVersion;
        public ushort dmSize;
        public ushort dmDriverExtra;
        public uint dmFields;

        public int dmPositionX;
        public int dmPositionY;
        public uint dmDisplayOrientation;
        public uint dmDisplayFixedOutput;

        public short dmColor;
        public short dmDuplex;
        public short dmYResolution;
        public short dmTTOption;
        public short dmCollate;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = CCHFORMNAME)]
        public string dmFormName;
        public ushort dmLogPixels;
        public uint dmBitsPerPel;
        public uint dmPelsWidth;
        public uint dmPelsHeight;
        public uint dmDisplayFlags;
        public uint dmDisplayFrequency;
    }

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr OpenDesktop(string lpszDesktop, uint dwFlags, bool fInherit, uint dwDesiredAccess);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetThreadDesktop(IntPtr hDesktop);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool CloseDesktop(IntPtr hDesktop);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern bool EnumDisplaySettingsW(string? lpszDeviceName, int iModeNum, ref DEVMODE lpDevMode);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int ChangeDisplaySettingsExW(string? lpszDeviceName, ref DEVMODE lpDevMode, IntPtr hwnd, uint dwflags, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int ChangeDisplaySettingsExW(string? lpszDeviceName, IntPtr lpDevMode, IntPtr hwnd, uint dwflags, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int ChangeDisplaySettingsW(ref DEVMODE lpDevMode, uint dwflags);

    private static DEVMODE _originalMode;
    private static bool _hasOriginal;
    private static bool _switched;

    static DisplayHelper()
    {
        AppDomain.CurrentDomain.ProcessExit += (_, _) => RestoreOriginal();
    }

    private static void EnsureInteractiveDesktop()
    {
        try
        {
            IntPtr hDefault = OpenDesktop("Default", 0, false, DESKTOP_ALL);
            if (hDefault != IntPtr.Zero)
            {
                SetThreadDesktop(hDefault);
                CloseDesktop(hDefault);
            }
        }
        catch { }
    }

    public static int CurrentRefreshRate
    {
        get
        {
            try
            {
                EnsureInteractiveDesktop();
                var dm = new DEVMODE { dmSize = 188 };
                if (EnumDisplaySettingsW(null, ENUM_CURRENT_SETTINGS, ref dm))
                {
                    return (int)dm.dmDisplayFrequency;
                }
            }
            catch { }
            return 60;
        }
    }

    public static bool SwitchTo60Hz()
    {
        try
        {
            EnsureInteractiveDesktop();
            var current = new DEVMODE { dmSize = 188 };
            if (!EnumDisplaySettingsW(null, ENUM_CURRENT_SETTINGS, ref current))
                return false;

            if (!_hasOriginal)
            {
                _originalMode = current;
                _hasOriginal = true;
            }

            if (current.dmDisplayFrequency == 60)
                return true; // Already 60Hz

            var targetMode = current;
            targetMode.dmDisplayFrequency = 60;
            targetMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;

            int res = ChangeDisplaySettingsExW(null, ref targetMode, IntPtr.Zero, 0, IntPtr.Zero);
            if (res == DISP_CHANGE_SUCCESSFUL)
            {
                _switched = true;
                return true;
            }

            // Fallback to non-Ex
            res = ChangeDisplaySettingsW(ref targetMode, 0);
            if (res == DISP_CHANGE_SUCCESSFUL)
            {
                _switched = true;
                return true;
            }
        }
        catch { }
        return false;
    }

    public static void RestoreOriginal()
    {
        if (!_switched) return;
        try
        {
            EnsureInteractiveDesktop();
            if (_hasOriginal)
            {
                var mode = _originalMode;
                mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
                int res = ChangeDisplaySettingsExW(null, ref mode, IntPtr.Zero, 0, IntPtr.Zero);
                if (res != DISP_CHANGE_SUCCESSFUL)
                {
                    ChangeDisplaySettingsW(ref mode, 0);
                }
            }
            else
            {
                ChangeDisplaySettingsExW(null, IntPtr.Zero, IntPtr.Zero, 0, IntPtr.Zero);
            }
            _switched = false;
        }
        catch { }
    }
}
