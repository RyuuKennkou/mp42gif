/*
 * mp4_to_gif_v2.cs  —  MP4 → GIF 转换器 (C# WinForms, Media Foundation)
 * v2.2  2026-06-01
 *
 * 编译 (cmd):
 *   C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe /target:winexe /out:mp4_to_gif_v2.exe mp4_to_gif_v2.cs
 * 要求: .NET Framework 4.x, Win10/11
 */

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using System.Windows.Forms;

// ====================================================================
//  Media Foundation P/Invoke (complete, 25 IMFAttributes methods)
// ====================================================================
static class MF
{
    public const int S_OK = 0;
    public const int MFSTARTUP_NOSOCKET = 0x1;
    public const int MF_SOURCE_READER_FIRST_VIDEO_STREAM = unchecked((int)0xFFFFFFFC);

    public static readonly Guid MFMediaType_Video   = new Guid("73646976-0000-0010-8000-00AA00389B71");
    public static readonly Guid MFVideoFormat_RGB32 = new Guid("00000016-0000-0010-8000-00AA00389B71");
    public static readonly Guid MF_MT_MAJOR_TYPE    = new Guid("48eba18e-f8c9-4687-bf11-0a74c9f96a8f");
    public static readonly Guid MF_MT_SUBTYPE       = new Guid("f7e34c9a-42e8-4714-b74b-cb29d72c35e5");
    public static readonly Guid MF_MT_FRAME_SIZE    = new Guid("1652c33d-d6b2-4012-b834-72030849a37d");
    public static readonly Guid MF_MT_FRAME_RATE    = new Guid("c459a2e8-3d2c-4e44-b132-fee5156c7bb0");

    [DllImport("mfplat.dll")]  public static extern int MFStartup(uint ver, int flags);
    [DllImport("mfplat.dll")]  public static extern int MFShutdown();
    [DllImport("mfplat.dll")]  public static extern int MFCreateMediaType(out IMFMediaType ppMFType);
    [DllImport("mfreadwrite.dll")] public static extern int MFCreateSourceReaderFromURL(
        [MarshalAs(UnmanagedType.LPWStr)] string url, IntPtr attrs, out IMFSourceReader reader);

    // ---- IMFAttributes: 25 methods, in exact vtable order ----
    [ComImport, Guid("2CD2D921-C447-44A7-A13C-4ADABFC247E3"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFAttributes
    {
        void _1_GetItem([MarshalAs(UnmanagedType.LPStruct)] Guid k, ref PropVariant v);
        void _2_GetItemType([MarshalAs(UnmanagedType.LPStruct)] Guid k, out int t);
        void _3_CompareItem([MarshalAs(UnmanagedType.LPStruct)] Guid k, ref PropVariant v, out bool r);
        void _4_Compare(IMFAttributes a, int t, out bool r);
        void _5_GetUINT32([MarshalAs(UnmanagedType.LPStruct)] Guid k, out uint v);
        void _6_GetUINT64([MarshalAs(UnmanagedType.LPStruct)] Guid k, out ulong v);
        void _7_GetDouble([MarshalAs(UnmanagedType.LPStruct)] Guid k, out double v);
        void _8_GetGUID([MarshalAs(UnmanagedType.LPStruct)] Guid k, out Guid v);
        void _9_GetStringLength([MarshalAs(UnmanagedType.LPStruct)] Guid k, out int l);
        void _10_GetString([MarshalAs(UnmanagedType.LPStruct)] Guid k,
            [MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder v, int s, out int l);
        void _11_GetAllocatedString([MarshalAs(UnmanagedType.LPStruct)] Guid k,
            [MarshalAs(UnmanagedType.LPWStr)] out string v, out int l);
        void _12_GetBlobSize([MarshalAs(UnmanagedType.LPStruct)] Guid k, out int s);
        void _13_GetBlob([MarshalAs(UnmanagedType.LPStruct)] Guid k,
            [In, Out] byte[] b, int s, out int r);
        void _14_SetUINT32([MarshalAs(UnmanagedType.LPStruct)] Guid k, uint v);
        void _15_SetUINT64([MarshalAs(UnmanagedType.LPStruct)] Guid k, ulong v);
        void _16_SetDouble([MarshalAs(UnmanagedType.LPStruct)] Guid k, double v);
        void _17_SetGUID([MarshalAs(UnmanagedType.LPStruct)] Guid k, [MarshalAs(UnmanagedType.LPStruct)] Guid v);
        void _18_SetString([MarshalAs(UnmanagedType.LPStruct)] Guid k, [MarshalAs(UnmanagedType.LPWStr)] string v);
        void _19_SetBlob([MarshalAs(UnmanagedType.LPStruct)] Guid k, [In] byte[] b, int s);
        void _20_DeleteItem([MarshalAs(UnmanagedType.LPStruct)] Guid k);
        void _21_LockStore();
        void _22_UnlockStore();
        void _23_GetCount(out int c);
        void _24_GetItemByIndex(int i, [MarshalAs(UnmanagedType.LPStruct)] Guid k, ref PropVariant v);
        void _25_CopyAllItems(IMFAttributes d);
    }

    [StructLayout(LayoutKind.Explicit, Size = 24)]
    public struct PropVariant
    {
        [FieldOffset(0)] public short vt;
        [FieldOffset(8)] public IntPtr ptr;
    }

    // ---- IMFMediaType : IMFAttributes, no extra methods needed ----
    [ComImport, Guid("44AE0FA8-EA31-4109-8D2E-4CAE4997C555"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFMediaType : IMFAttributes { }

    // ---- IMFMediaBuffer : IUnknown, 5 methods ----
    [ComImport, Guid("045FA593-8799-42b8-BC8D-8968C6453507"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFMediaBuffer
    {
        void Lock(out IntPtr buf, out int max, out int cur);
        void Unlock();
        void GetCurrentLength(out int len);
        void SetCurrentLength(int len);
        void GetMaxLength(out int max);
    }

    // ---- IMFSample : IMFAttributes, 14 own methods ----
    [ComImport, Guid("c40a00f2-b93a-4d80-ae8c-5a1c634f58e4"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFSample : IMFAttributes
    {
        void _26_GetSampleFlags(out int f);
        void _27_SetSampleFlags(int f);
        void _28_GetSampleTime(out long t);
        void _29_SetSampleTime(long t);
        void _30_GetSampleDuration(out long d);
        void _31_SetSampleDuration(long d);
        void _32_GetBufferCount(out int c);
        void _33_GetBufferByIndex(int i, out IMFMediaBuffer b);
        void _34_ConvertToContiguousBuffer(out IMFMediaBuffer b);
        [PreserveSig] int _35_AddBuffer(IMFMediaBuffer b);
        void _36_RemoveBufferByIndex(int i);
        void _37_RemoveAllBuffers();
        void _38_GetTotalLength(out int l);
        void _39_CopyToBuffer(IMFMediaBuffer b);
    }

    // ---- IMFSourceReader : IUnknown, 9 methods ----
    [ComImport, Guid("70ae66f2-c809-4e4f-8915-bdcb406b7993"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IMFSourceReader
    {
        [PreserveSig] int _1_GetStreamSelection(int i, out bool s);
        [PreserveSig] int _2_SetStreamSelection(int i, bool s);
        [PreserveSig] int _3_GetNativeMediaType(int i, int t, out IMFMediaType m);
        [PreserveSig] int _4_GetCurrentMediaType(int i, out IMFMediaType m);
        [PreserveSig] int _5_SetCurrentMediaType(int i, IntPtr r, IMFMediaType m);
        void _6_SetCurrentPosition([MarshalAs(UnmanagedType.LPStruct)] Guid f, [In] ref long p);
        [PreserveSig] int _7_ReadSample(int i, int c, out int ai, out int fl, out long ts, out IMFSample s);
        [PreserveSig] int _8_Flush(int i);
        [PreserveSig] int _9_GetServiceForStream(int i,
            [MarshalAs(UnmanagedType.LPStruct)] Guid gs,
            [MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IntPtr o);
    }

    // helper
    public static void GetFR(IMFMediaType mt, out int w, out int h)
    {
        ulong v; mt._6_GetUINT64(MF_MT_FRAME_SIZE, out v);
        w = (int)(v & 0xFFFFFFFF); h = (int)(v >> 32);
    }
}


// ====================================================================
//  VideoReader — 用 Media Foundation 逐帧读取
// ====================================================================
class VideoReader : IDisposable
{
    MF.IMFSourceReader _r;
    int _w, _h;
    double _fps, _dur;
    bool _rgbSet;
    int _skip; // 用于跳帧时记录间隔

    public int Width { get { return _w; } }
    public int Height { get { return _h; } }
    public double Fps { get { return _fps; } }
    public double DurationSec { get { return _dur; } }

    public VideoReader(string file, bool setRGB32)
    {
        int hr = MF.MFStartup(0x20070, MF.MFSTARTUP_NOSOCKET);
        if (hr != 0) throw new Exception("MFStartup: 0x" + hr.ToString("X8"));

        hr = MF.MFCreateSourceReaderFromURL(file, IntPtr.Zero, out _r);
        if (hr != 0) throw new Exception("打开视频失败: 0x" + hr.ToString("X8"));

        // 获取原始尺寸
        MF.IMFMediaType nat;
        hr = _r._3_GetNativeMediaType(MF.MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, out nat);
        if (hr == MF.S_OK)
        {
            MF.GetFR(nat, out _w, out _h);
            Marshal.ReleaseComObject(nat);
        }
        else { _w = 640; _h = 480; }

        // 设置 RGB32 输出
        if (setRGB32)
        {
            MF.IMFMediaType rt;
            hr = MF.MFCreateMediaType(out rt);
            if (hr == MF.S_OK)
            {
                rt._17_SetGUID(MF.MF_MT_MAJOR_TYPE, MF.MFMediaType_Video);
                rt._17_SetGUID(MF.MF_MT_SUBTYPE, MF.MFVideoFormat_RGB32);
                rt._15_SetUINT64(MF.MF_MT_FRAME_SIZE, ((ulong)_h << 32) | (uint)_w);
                hr = _r._5_SetCurrentMediaType(MF.MF_SOURCE_READER_FIRST_VIDEO_STREAM, IntPtr.Zero, rt);
                _rgbSet = (hr == MF.S_OK);
                Marshal.ReleaseComObject(rt);
            }
        }

        // 获取帧率
        _fps = 30.0;
        try
        {
            MF.IMFMediaType ct;
            if (MF.S_OK == _r._4_GetCurrentMediaType(MF.MF_SOURCE_READER_FIRST_VIDEO_STREAM, out ct))
            {
                ulong fv;
                ct._6_GetUINT64(MF.MF_MT_FRAME_RATE, out fv);
                uint n = (uint)(fv >> 32), d = (uint)(fv & 0xFFFFFFFF);
                if (d > 0) _fps = (double)n / d;
                Marshal.ReleaseComObject(ct);
            }
        }
        catch { }

        // 估算时长
        _dur = 10.0;
    }

    public Bitmap ReadFrame()
    {
        int ai, fl; long ts; MF.IMFSample s;
        int hr = _r._7_ReadSample(MF.MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, out ai, out fl, out ts, out s);
        if (hr != MF.S_OK || s == null) return null;

        try
        {
            MF.IMFMediaBuffer b;
            s._34_ConvertToContiguousBuffer(out b);
            IntPtr p; int max, cur;
            b.Lock(out p, out max, out cur);

            int w = _w > 0 ? _w : 640, h = _h > 0 ? _h : 480;
            int expect = w * h * 4;
            var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
            var d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);

            if (cur >= expect)
            {
                int stride = w * 4;
                for (int y = 0; y < h; y++)
                {
                    IntPtr src = IntPtr.Add(p, y * stride);
                    IntPtr dst = IntPtr.Add(d.Scan0, y * d.Stride);
                    CopyMemory(dst, src, (uint)stride);
                }
            }
            bmp.UnlockBits(d);
            b.Unlock();
            Marshal.ReleaseComObject(b);
            return bmp;
        }
        finally { Marshal.ReleaseComObject(s); }
    }

    [DllImport("kernel32.dll", EntryPoint = "RtlMoveMemory")]
    static extern void CopyMemory(IntPtr d, IntPtr s, uint n);

    public void Dispose()
    {
        if (_r != null) { Marshal.ReleaseComObject(_r); _r = null; }
        MF.MFShutdown();
    }
}


// ====================================================================
//  MainForm (界面逻辑)
// ====================================================================
public class MainForm : Form
{
    string _inFile = "", _outFile = "";
    int _vw, _vh; double _vd, _vf;
    Bitmap _firstBmp; bool _busy, _lockUI;

    TextBox tbIn, tbOut, tbSS, tbW, tbH, tbFps;
    ComboBox cbP, cbQ; CheckBox chkR;
    Label lblVI, lblSI, lblSt, lblOT;
    Button btnGo; ProgressBar pb;
    PictureBox pbIn, pbOut;
    Panel panR, panM;

    Font F10, F10B, F18B, F8, F12B;

    public MainForm()
    {
        F10 = new Font("Microsoft YaHei", 10f);
        F10B = new Font("Microsoft YaHei", 10f, FontStyle.Bold);
        F18B = new Font("Microsoft YaHei", 18f, FontStyle.Bold);
        F8 = new Font("Microsoft YaHei", 8f);
        F12B = new Font("Microsoft YaHei", 12f, FontStyle.Bold);
        ClientSize = new Size(900, 600);
        FormBorderStyle = FormBorderStyle.FixedSingle; MaximizeBox = false;
        BackColor = Color.White; StartPosition = FormStartPosition.CenterScreen;
        Text = "MP4 → GIF 转换器";
        BuildUI();
    }

    void AC(Control c) { Controls.Add(c); }

    Label MkL(string t, int x, int y, int w, int h, Color? c = null, Font f = null)
    {
        return new Label { Text = t, Bounds = new Rectangle(x, y, w, h), Font = f ?? F10,
            ForeColor = c ?? SystemColors.ControlText, BackColor = Color.White,
            TextAlign = ContentAlignment.MiddleLeft };
    }
    TextBox MkE(int x, int y, int w, int h, bool ro = false)
    {
        return new TextBox { Bounds = new Rectangle(x, y, w, h), Font = F10,
            ReadOnly = ro, BorderStyle = BorderStyle.FixedSingle };
    }
    Button MkB(string t, int x, int y, int w, int h, Action cb)
    {
        var b = new Button { Text = t, Bounds = new Rectangle(x, y, w, h), Font = F10,
            FlatStyle = FlatStyle.Flat };
        b.Click += (s, e) => cb(); return b;
    }

    void BuildUI()
    {
        SuspendLayout();
        int ew = 480, bx = 505, bw = 110;

        AC(new Label { Text = "MP4 → GIF 转换器", Font = F18B,
            Bounds = new Rectangle(15, 8, 620, 30), TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.White });
        AC(MkL("输入视频:", 15, 48, 70, 20));
        AC(tbIn = MkE(15, 68, ew, 26, true));
        AC(MkB("浏览...", bx, 66, bw, 28, BrowseIn));
        AC(MkL("输出GIF:", 15, 112, 70, 20));
        AC(tbOut = MkE(15, 132, ew, 26));
        tbOut.TextChanged += (s, e) => _outFile = tbOut.Text;
        AC(MkB("浏览...", bx, 130, bw, 28, BrowseOut));
        AC(lblVI = MkL("未加载视频", 15, 168, 620, 18, Color.Gray));
        AC(MkL(new string('─', 76), 15, 196, 620, 8, Color.FromArgb(176, 176, 176)));
        AC(new Label { Text = "尺寸设置", Font = F12B,
            Bounds = new Rectangle(15, 216, 80, 20), BackColor = Color.White });
        AC(MkL("预设:", 15, 250, 40, 18));
        cbP = new ComboBox { Bounds = new Rectangle(55, 247, 185, 25), Font = F10,
            DropDownStyle = ComboBoxStyle.DropDownList };
        cbP.Items.AddRange(new[] { "微信表情包 (300px)", "小尺寸 (200px)", "中等 (480px)",
            "大尺寸 (720px)", "原始尺寸", "自定义" });
        cbP.SelectedIndex = 0; cbP.SelectedIndexChanged += OnPreset;
        AC(cbP);
        chkR = new CheckBox { Text = "保持原始宽高比 (仅需设置最短边)", Checked = true,
            Font = F10, Bounds = new Rectangle(15, 286, 340, 24), BackColor = Color.White };
        chkR.CheckedChanged += OnRatio; AC(chkR);

        var rp = new Panel { Bounds = new Rectangle(15, 318, 620, 30), BackColor = Color.White };
        panR = new Panel { BackColor = Color.White, Visible = true };
        panR.Controls.Add(new Label { Text = "最短边:", Font = F10, BackColor = Color.White,
            Bounds = new Rectangle(15, 3, 55, 24), TextAlign = ContentAlignment.MiddleLeft });
        tbSS = new TextBox { Text = "300", Font = F10, TextAlign = HorizontalAlignment.Center,
            Bounds = new Rectangle(72, 3, 58, 24) };
        tbSS.TextChanged += (s, e) => { if (!_lockUI) { cbP.SelectedIndex = 5; UpdateSI(); } };
        panR.Controls.Add(tbSS);
        panR.Controls.Add(new Label { Text = "像素", Font = F10, BackColor = Color.White,
            Bounds = new Rectangle(134, 3, 35, 24), TextAlign = ContentAlignment.MiddleLeft });
        panM = new Panel { BackColor = Color.White, Visible = false };
        panM.Controls.Add(new Label { Text = "宽度:", Font = F10, BackColor = Color.White,
            Bounds = new Rectangle(15, 3, 42, 24), TextAlign = ContentAlignment.MiddleLeft });
        tbW = new TextBox { Font = F10, TextAlign = HorizontalAlignment.Center,
            Bounds = new Rectangle(60, 3, 60, 24) };
        tbW.TextChanged += (s, e) => UpdateSI(); panM.Controls.Add(tbW);
        panM.Controls.Add(new Label { Text = "高度:", Font = F10, BackColor = Color.White,
            Bounds = new Rectangle(130, 3, 42, 24), TextAlign = ContentAlignment.MiddleLeft });
        tbH = new TextBox { Font = F10, TextAlign = HorizontalAlignment.Center,
            Bounds = new Rectangle(174, 3, 60, 24) };
        tbH.TextChanged += (s, e) => UpdateSI(); panM.Controls.Add(tbH);
        panM.Controls.Add(new Label { Text = "像素", Font = F10, BackColor = Color.White,
            Bounds = new Rectangle(240, 3, 35, 24), TextAlign = ContentAlignment.MiddleLeft });
        rp.Controls.Add(panR); rp.Controls.Add(panM); AC(rp);

        AC(lblSI = new Label { Text = "输出尺寸: -- × --    预计大小: --", Font = F10B,
            ForeColor = Color.FromArgb(26, 26, 166),
            Bounds = new Rectangle(15, 354, 620, 22), BackColor = Color.White });
        AC(MkL(new string('─', 76), 15, 386, 620, 8, Color.FromArgb(176, 176, 176)));
        AC(MkL("画质:", 15, 410, 40, 18));
        cbQ = new ComboBox { Bounds = new Rectangle(55, 407, 100, 25), Font = F10,
            DropDownStyle = ComboBoxStyle.DropDownList };
        cbQ.Items.AddRange(new[] { "最佳", "高", "中等", "低" });
        cbQ.SelectedIndex = 0; cbQ.SelectedIndexChanged += (s, e) => UpdateSI(); AC(cbQ);
        AC(MkL("帧率:", 180, 410, 40, 18));
        AC(tbFps = new TextBox { Text = "15", Font = F10, TextAlign = HorizontalAlignment.Center,
            Bounds = new Rectangle(222, 407, 50, 25) });
        tbFps.TextChanged += (s, e) => UpdateSI();
        AC(MkL("fps", 276, 410, 25, 18));
        AC(MkL("最佳=256色+抖动 | 高=256色 | 中=128色 | 低=64色", 15, 438, 350, 14,
            Color.FromArgb(140, 140, 140), F8));

        btnGo = new Button { Text = "开始转换",
            Font = new Font("Microsoft YaHei", 14f, FontStyle.Bold),
            Bounds = new Rectangle(180, 465, 260, 38),
            BackColor = Color.FromArgb(38, 140, 38), ForeColor = Color.White,
            FlatStyle = FlatStyle.Flat };
        btnGo.FlatAppearance.BorderSize = 0; btnGo.Click += (s, e) => StartConv(); AC(btnGo);

        AC(pb = new ProgressBar { Bounds = new Rectangle(15, 515, 620, 12) });
        AC(lblSt = MkL("就绪", 15, 536, 620, 22, Color.FromArgb(102, 102, 102)));
        lblSt.TextAlign = ContentAlignment.MiddleCenter;

        int rx = 660, rw = 225;
        AC(new Label { Text = "输入视频", Font = F10B, Bounds = new Rectangle(rx, 8, rw, 22),
            TextAlign = ContentAlignment.MiddleCenter, BackColor = Color.White });
        pbIn = new PictureBox { Bounds = new Rectangle(rx + 5, 36, rw - 10, 160),
            BackColor = Color.FromArgb(238, 238, 238), SizeMode = PictureBoxSizeMode.Zoom };
        pbIn.Paint += PntBdr; AC(pbIn);
        AC(new Label { Text = "↓", Font = new Font("Microsoft YaHei", 12f),
            Bounds = new Rectangle(rx, 202, rw, 16), TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.White, ForeColor = Color.FromArgb(140, 140, 140) });
        lblOT = new Label { Text = "输出GIF", Font = F10B,
            Bounds = new Rectangle(rx, 224, rw, 22), TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.White }; AC(lblOT);
        pbOut = new PictureBox { Bounds = new Rectangle(rx + 5, 252, rw - 10, 160),
            BackColor = Color.FromArgb(238, 238, 238), SizeMode = PictureBoxSizeMode.Zoom };
        pbOut.Paint += PntBdr; AC(pbOut);
        ResumeLayout(false);
    }

    void PntBdr(object s, PaintEventArgs e) { ControlPaint.DrawBorder(e.Graphics,
        ((PictureBox)s).ClientRectangle, Color.FromArgb(170, 170, 170), ButtonBorderStyle.Solid); }

    // ================================================================
    void BrowseIn()
    {
        using (var d = new OpenFileDialog { Title = "选择输入视频",
            Filter = "视频|*.mp4;*.avi;*.mov;*.mkv;*.wmv|MP4|*.mp4|All|*.*" })
        { if (d.ShowDialog() != DialogResult.OK) return;
          _inFile = d.FileName; tbIn.Text = _inFile;
          _outFile = Path.ChangeExtension(_inFile, ".gif"); tbOut.Text = _outFile; LoadVI(); }
    }

    void LoadVI()
    {
        VideoReader vr = null;
        try
        {
            vr = new VideoReader(_inFile, true);
            _vw = vr.Width; _vh = vr.Height; _vf = vr.Fps; _vd = vr.DurationSec;
            if (_firstBmp != null) _firstBmp.Dispose();
            _firstBmp = vr.ReadFrame();
            lblVI.Text = string.Format("视频: {0}×{1} | {2:F1}秒 | {3:F1}fps", _vw, _vh, _vd, _vf);
            lblVI.ForeColor = SystemColors.ControlText;
            if (_firstBmp != null)
            { if (pbIn.Image != null) pbIn.Image.Dispose(); pbIn.Image = ScPv(_firstBmp, 215, 160); }
            if (cbP.SelectedIndex == 4)
            { _lockUI = true; tbW.Text = _vw.ToString(); tbH.Text = _vh.ToString(); _lockUI = false; }
        }
        catch (Exception ex)
        {
            _vw = _vh = 0; _vd = 0; _vf = 30;
            if (_firstBmp != null) { _firstBmp.Dispose(); _firstBmp = null; }
            if (pbIn.Image != null) { pbIn.Image.Dispose(); pbIn.Image = null; }
            lblVI.Text = "失败: " + ex.Message; lblVI.ForeColor = Color.Red;
        }
        finally { if (vr != null) vr.Dispose(); }
        UpdateSI();
    }

    void BrowseOut()
    {
        using (var d = new SaveFileDialog { Title = "保存 GIF", DefaultExt = ".gif",
            Filter = "GIF|*.gif" })
        { if (d.ShowDialog() == DialogResult.OK) { _outFile = d.FileName; tbOut.Text = _outFile; } }
    }

    // ================================================================
    void OnPreset(object s, EventArgs e)
    {
        _lockUI = true;
        switch (cbP.SelectedIndex)
        { case 0: chkR.Checked = true; tbSS.Text = "300"; break;
          case 1: chkR.Checked = true; tbSS.Text = "200"; break;
          case 2: chkR.Checked = true; tbSS.Text = "480"; break;
          case 3: chkR.Checked = true; tbSS.Text = "720"; break;
          case 4: chkR.Checked = false;
            if (_vw > 0) { tbW.Text = _vw.ToString(); tbH.Text = _vh.ToString(); } break; }
        TogUI(chkR.Checked); _lockUI = false; UpdateSI();
    }
    void OnRatio(object s, EventArgs e) { if (_lockUI) return; TogUI(chkR.Checked);
        cbP.SelectedIndex = 5; UpdateSI(); }
    void TogUI(bool m) { panM.Visible = !m; panR.Visible = m; }

    void UpdateSI()
    {
        int ow, oh;
        if (_vw == 0 || !GetSz(out ow, out oh))
        { lblSI.Text = "输出尺寸: -- × --    预计大小: --"; return; }
        double fps; if (!double.TryParse(tbFps.Text, out fps) || fps <= 0) fps = 15;
        double tf = _vd > 0 ? _vd * fps : 30;
        double[] cr = { 5.0, 6.5, 8.5, 11.0 };
        double r = cr[Math.Min(cbQ.SelectedIndex, 3)];
        string es = FmtSz((long)(ow * oh * tf / r));
        lblSI.Text = string.Format("输出尺寸: {0} × {1}    预计大小: {2}  ({3} 帧)",
            ow, oh, es, (int)Math.Round(tf));
    }

    bool GetSz(out int ow, out int oh)
    {
        ow = oh = 0;
        if (chkR.Checked)
        { int ss; if (!int.TryParse(tbSS.Text, out ss) || ss <= 0) return false;
          if (_vw <= _vh) { ow = ss; oh = (int)Math.Round(_vh * (ss / (double)_vw)); }
          else { oh = ss; ow = (int)Math.Round(_vw * (ss / (double)_vh)); } }
        else
        { if (!int.TryParse(tbW.Text, out ow) || ow <= 0) return false;
          if (!int.TryParse(tbH.Text, out oh) || oh <= 0) return false; }
        return true;
    }

    static string FmtSz(long b)
    { if (b < 1024) return b + " B"; if (b < 1048576) return (b / 1024.0).ToString("F1") + " KB";
      return (b / 1048576.0).ToString("F2") + " MB"; }

    static Bitmap ScPv(Bitmap s, int mw, int mh)
    { double sc = Math.Min((double)mw / s.Width, (double)mh / s.Height);
      var b = new Bitmap((int)(s.Width * sc), (int)(s.Height * sc));
      using (var g = Graphics.FromImage(b)) { g.DrawImage(s, 0, 0, b.Width, b.Height); } return b; }

    // ================================================================
    void StartConv()
    {
        if (_busy) return;
        if (string.IsNullOrEmpty(_inFile) || !File.Exists(_inFile))
        { MessageBox.Show("请选择有效的输入视频文件。", "输入错误"); return; }
        if (string.IsNullOrEmpty(_outFile))
        { MessageBox.Show("请指定输出 GIF 文件。", "输出错误"); return; }
        double fps; if (!double.TryParse(tbFps.Text, out fps) || fps <= 0 || fps > 60)
        { MessageBox.Show("帧率必须在 1 到 60 fps 之间。", "参数错误"); return; }
        int ow, oh; if (!GetSz(out ow, out oh))
        { MessageBox.Show("请设置有效的输出尺寸。", "参数错误"); return; }
        int nc; bool di; switch (Math.Min(cbQ.SelectedIndex, 3))
        { case 0: nc = 256; di = true; break; case 1: nc = 256; di = false; break;
          case 2: nc = 128; di = false; break; default: nc = 64; di = false; break; }

        _busy = true; btnGo.Enabled = false; btnGo.Text = "转换中...";
        lblSt.Text = "启动..."; lblSt.ForeColor = SystemColors.ControlText; pb.Value = 0;
        if (pbOut.Image != null) pbOut.Image.Dispose(); pbOut.Image = null;
        lblOT.Text = "输出GIF (转换中...)";
        var bg = new BackgroundWorker();
        bg.DoWork += (s, e) => DoConv(ow, oh, fps, nc, di);
        bg.RunWorkerCompleted += (s, e) => {
            if (e.Error != null) { lblSt.Text = "错误: " + e.Error.Message;
                lblSt.ForeColor = Color.Red; lblOT.Text = "输出GIF (失败)"; }
            btnGo.Enabled = true; btnGo.Text = "开始转换"; _busy = false; };
        bg.RunWorkerAsync();
    }

    void DoConv(int ow, int oh, double fps, int nc, bool di)
    {
        VideoReader vr = null;
        try
        {
            vr = new VideoReader(_inFile, true);
            double sf = vr.Fps > 0 ? vr.Fps : 30;
            int skip = Math.Max(1, (int)Math.Round(sf / fps));
            int est = Math.Max(1, (int)(vr.DurationSec * fps));
            var frames = new List<Bitmap>();
            int fi = 0;
            while (true)
            {
                var b = vr.ReadFrame();
                if (b == null) break;
                if (fi % skip != 0) { b.Dispose(); fi++; continue; }
                var rz = new Bitmap(ow, oh);
                using (var g = Graphics.FromImage(rz))
                { g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                  g.DrawImage(b, 0, 0, ow, oh); }
                b.Dispose(); frames.Add(rz); fi++;
                if (frames.Count % 5 == 0)
                { int pct = Math.Min(frames.Count * 100 / est, 100);
                  this.BeginInvoke(new Action(() => { pb.Value = pct;
                    lblSt.Text = string.Format("进度: {0}/{1}帧 ({2}%)", frames.Count, est, pct); })); }
            }
            if (frames.Count == 0) throw new Exception("未读取到帧");
            var dir = Path.GetDirectoryName(Path.GetFullPath(_outFile));
            if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
            SaveGif(_outFile, frames, (int)(1000 / fps));
            string asz = FmtSz(new FileInfo(_outFile).Length);
            string nm = Path.GetFileName(_outFile); int wc = frames.Count;
            this.BeginInvoke(new Action(() => { pb.Value = 100;
                lblSt.Text = string.Format("完成! {0}帧 {1}×{2} {3:F1}fps 大小:{4} → {5}", wc, ow, oh, fps, asz, nm);
                lblSt.ForeColor = Color.FromArgb(0, 102, 0);
                lblSI.Text = string.Format("输出尺寸: {0}×{1}    实际大小: {2}  ({3}帧)", ow, oh, asz, wc);
                lblSI.ForeColor = Color.FromArgb(0, 102, 0); lblOT.Text = "输出GIF ✓";
                if (_firstBmp != null) { var pv = new Bitmap(ow, oh);
                  using (var g = Graphics.FromImage(pv)) g.DrawImage(_firstBmp, 0, 0, ow, oh);
                  if (pbOut.Image != null) pbOut.Image.Dispose();
                  pbOut.Image = ScPv(pv, 215, 160); pv.Dispose(); }
                foreach (var f in frames) f.Dispose(); }));
        }
        finally { if (vr != null) vr.Dispose(); }
    }

    static void SaveGif(string path, List<Bitmap> frames, int delayMs)
    {
        if (frames.Count == 0) return;
        var enc = GetGifEnc();
        var ep = new EncoderParameters(1);
        ep.Param[0] = new EncoderParameter(Encoder.SaveFlag, (long)EncoderValue.MultiFrame);
        frames[0].Save(path, enc, ep);
        for (int i = 1; i < frames.Count; i++)
        { SetDelay(frames[i], delayMs);
          ep.Param[0] = new EncoderParameter(Encoder.SaveFlag, (long)EncoderValue.FrameDimensionTime);
          frames[0].SaveAdd(frames[i], ep); }
        ep.Param[0] = new EncoderParameter(Encoder.SaveFlag, (long)EncoderValue.Flush);
        frames[0].SaveAdd(ep);
    }

    static ImageCodecInfo GetGifEnc()
    { foreach (var c in ImageCodecInfo.GetImageEncoders()) if (c.MimeType == "image/gif") return c; return null; }

    static void SetDelay(Bitmap f, int ms)
    {
        short d = (short)Math.Max(1, ms / 10); byte[] v = BitConverter.GetBytes(d);
        var pi = (PropertyItem)FormatterServices.GetUninitializedObject(typeof(PropertyItem));
        var bf = BindingFlags.NonPublic | BindingFlags.Instance;
        var fi = typeof(PropertyItem).GetField("id", bf);
        if (fi != null) fi.SetValue(pi, 0x5100);
        fi = typeof(PropertyItem).GetField("type", bf);
        if (fi != null) fi.SetValue(pi, (short)3);
        fi = typeof(PropertyItem).GetField("len", bf);
        if (fi != null) fi.SetValue(pi, v.Length);
        fi = typeof(PropertyItem).GetField("value", bf);
        if (fi != null) fi.SetValue(pi, v);
        f.SetPropertyItem(pi);
    }
}

static class Program
{ [STAThread] static void Main() { Application.EnableVisualStyles();
    Application.SetCompatibleTextRenderingDefault(false); Application.Run(new MainForm()); } }
