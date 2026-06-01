/*
 * mp4_to_gif_v3.cpp  —  MP4 → GIF 转换器 (C++ Win32, 零依赖)
 * v3.0  2026-06-01
 *
 * 编译 (需要 Visual Studio Build Tools):
 *   cl /MT /O2 /Fe:mp4_to_gif_v3.exe mp4_to_gif_v3.cpp ^
 *      mfplat.lib mfreadwrite.lib mfuuid.lib gdiplus.lib ^
 *      comctl32.lib comdlg32.lib user32.lib gdi32.lib ole32.lib
 *
 * exe ~200 KB, 复制到任意 Win10/11 电脑即可运行.
 */

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>
#include <math.h>
#include <new>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <gdiplus.h>
using namespace Gdiplus;

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ====================================================================
//  Globals
// ====================================================================
static HWND g_hWnd;
static HINSTANCE g_hInst;
static HFONT g_hFont10, g_hFont10B, g_hFont18B, g_hFont8, g_hFont12B;

static WCHAR g_inFile[MAX_PATH], g_outFile[MAX_PATH];
static int g_vidW, g_vidH;
static double g_vidDur, g_vidFps;
static Bitmap *g_firstBmp;
static volatile LONG g_busy;
static BOOL g_lockUI;

static HWND g_tbIn, g_tbOut, g_tbSS, g_tbW, g_tbH, g_tbFps;
static HWND g_cbP, g_cbQ, g_chkR;
static HWND g_lblVI, g_lblSI, g_lblSt, g_lblOT;
static HWND g_btnGo, g_pb;
static HWND g_pnlR, g_pnlM, g_pbIn, g_pbOut;

static Bitmap *g_pbInBmp, *g_pbOutBmp;

// ====================================================================
//  Helpers
// ====================================================================
static HFONT MakeFont(int pt, BOOL bold)
{
    HDC hdc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);
    return CreateFontW(h, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                       0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH, L"Microsoft YaHei");
}

static HWND MakeCtrl(LPCWSTR cls, LPCWSTR txt, DWORD style,
                     int x, int y, int w, int h, HMENU id, HFONT font)
{
    HWND c = CreateWindowW(cls, txt, style | WS_CHILD | WS_VISIBLE,
                           x, y, w, h, g_hWnd, id, g_hInst, NULL);
    if (font) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}

static void UpdSizeInfo();

static void SetText(HWND c, LPCWSTR s) { SetWindowTextW(c, s); }
static void GetText(HWND c, WCHAR *buf, int len) { GetWindowTextW(c, buf, len); }

static double GetFps()
{
    WCHAR buf[32]; GetText(g_tbFps, buf, 32);
    double v = wcstod(buf, NULL);
    return (v > 0 && v <= 60) ? v : 15.0;
}

static BOOL GetOutSize(int *ow, int *oh)
{
    WCHAR buf[32]; *ow = *oh = 0;
    if (SendMessageW(g_chkR, BM_GETCHECK, 0, 0))
    {
        int ss;
        GetText(g_tbSS, buf, 32); ss = _wtoi(buf);
        if (ss <= 0) return FALSE;
        if (g_vidW <= g_vidH)
        { *ow = ss; *oh = (int)(g_vidH * ((double)ss / g_vidW) + 0.5); }
        else
        { *oh = ss; *ow = (int)(g_vidW * ((double)ss / g_vidH) + 0.5); }
    }
    else
    {
        GetText(g_tbW, buf, 32); *ow = _wtoi(buf);
        if (*ow <= 0) return FALSE;
        GetText(g_tbH, buf, 32); *oh = _wtoi(buf);
        if (*oh <= 0) return FALSE;
    }
    return TRUE;
}

static WCHAR *FmtSz(long b, WCHAR *buf)
{
    if (b < 1024) swprintf(buf, 32, L"%d B", (int)b);
    else if (b < 1048576) swprintf(buf, 32, L"%.1f KB", b / 1024.0);
    else swprintf(buf, 32, L"%.2f MB", b / 1048576.0);
    return buf;
}

static Bitmap *ScaleBmp(Bitmap *src, int mw, int mh)
{
    if (!src) return NULL;
    int sw = src->GetWidth(), sh = src->GetHeight();
    double s = min((double)mw / sw, (double)mh / sh);
    int nw = (int)(sw * s), nh = (int)(sh * s);
    Bitmap *b = new Bitmap(nw, nh);
    Graphics g(b);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.DrawImage(src, 0, 0, nw, nh);
    return b;
}

// ====================================================================
//  Media Foundation
// ====================================================================
static Bitmap *SampleToBitmap(IMFSample *sample, int w, int h);
static Bitmap *NV12ToRGB32(const BYTE *data, int w, int h, int stride);

static void LoadVideo()
{
    if (g_firstBmp) { delete g_firstBmp; g_firstBmp = NULL; }
    if (g_pbInBmp) { delete g_pbInBmp; g_pbInBmp = NULL; }

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) { WCHAR b[64]; swprintf(b,64,L"MFStartup失败:0x%08X",hr); SetText(g_lblVI,b); return; }

    IMFSourceReader *reader = NULL;
    hr = MFCreateSourceReaderFromURL(g_inFile, NULL, &reader);
    if (FAILED(hr)) { WCHAR b[64]; swprintf(b,64,L"打开失败:0x%08X",hr); MFShutdown(); SetText(g_lblVI,b); return; }

    // 获取原生尺寸
    IMFMediaType *natType = NULL;
    reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &natType);
    if (natType) { UINT32 w=0,h=0; MFGetAttributeSize(natType,MF_MT_FRAME_SIZE,&w,&h);
                   g_vidW=(int)w; g_vidH=(int)h; natType->Release(); }
    else { g_vidW=640; g_vidH=480; }

    // 获取 FPS + 当前输出格式
    g_vidFps = 30.0;
    WCHAR fmtStr[64] = L"格式:?";
    IMFMediaType *curType = NULL;
    if (SUCCEEDED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &curType)))
    {
        GUID sub;
        if (SUCCEEDED(curType->GetGUID(MF_MT_SUBTYPE, &sub)))
        {
            if (sub.Data1 == 0x3231564E) wcscpy(fmtStr, L"NV12");
            else if (sub.Data1 == 0x32595559) wcscpy(fmtStr, L"YUY2");
            else if (sub.Data1 == 0x00000016) wcscpy(fmtStr, L"RGB32");
            else if (sub.Data1 == 0x56555949) wcscpy(fmtStr, L"IYUV");
            else swprintf(fmtStr, 64, L"FMT:0x%08X", sub.Data1);
        }
        UINT64 fv;
        if (SUCCEEDED(curType->GetUINT64(MF_MT_FRAME_RATE, &fv)))
        { UINT32 n=(UINT32)(fv>>32), d=(UINT32)(fv&0xFFFFFFFF);
          if (d>0) g_vidFps=(double)n/d; }
        curType->Release();
    }

    // 读第一帧
    WCHAR info[256];
    IMFSample *sample = NULL; DWORD fl,ai; LONGLONG ts;
    hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &ai, &fl, &ts, &sample);
    if (SUCCEEDED(hr) && sample)
    {
        DWORD bc=0; sample->GetBufferCount(&bc);
        IMFMediaBuffer *buf=NULL;
        if (bc>0) sample->GetBufferByIndex(0, &buf);
        DWORD cur=0, max=0;
        if (buf) { buf->Lock((BYTE**)&max, &max, &cur); buf->Unlock(); buf->Release(); }
        g_firstBmp = SampleToBitmap(sample, g_vidW, g_vidH);
        sample->Release();
        if (g_firstBmp)
        { g_pbInBmp = ScaleBmp(g_firstBmp, 215, 160);
          swprintf(info,256,L"视频:%dx%d %.1ffps %s OK",g_vidW,g_vidH,g_vidFps,fmtStr); }
        else
        { swprintf(info,256,L"视频:%dx%d %.1ffps %s buf=%d RGB=%d NV12=%d",
                   g_vidW,g_vidH,g_vidFps,fmtStr,(int)cur,g_vidW*g_vidH*4,g_vidW*g_vidH+g_vidW*g_vidH/2); }
    }
    else
    { swprintf(info,256,L"视频:%dx%d %.1ffps %s ReadSample:0x%08X",g_vidW,g_vidH,g_vidFps,fmtStr,hr); }
    SetText(g_lblVI, info);

    g_vidDur = 10.0;
    reader->Release();
    MFShutdown();

    if (g_chkR && SendMessageW(g_cbP, CB_GETCURSEL, 0, 0) == 4)
    { WCHAR b[16]; swprintf(b,16,L"%d",g_vidW); SetText(g_tbW,b);
      swprintf(b,16,L"%d",g_vidH); SetText(g_tbH,b); }

    InvalidateRect(g_pbIn, NULL, FALSE);
    UpdSizeInfo();
}

// ====================================================================
//  Conversion
// ====================================================================
struct ConvParams { int ow, oh, nc; double fps; BOOL dither; };

static DWORD WINAPI ConvThread(LPVOID pv)
{
    ConvParams *cp = (ConvParams*)pv;
    int ow = cp->ow, oh = cp->oh, nc = cp->nc;
    double fps = cp->fps; BOOL dither = cp->dither;
    delete cp;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr)) { PostMessageW(g_hWnd, WM_USER+1, (WPARAM)hr, 0); CoUninitialize(); return 1; }

    IMFSourceReader *reader = NULL;
    hr = MFCreateSourceReaderFromURL(g_inFile, NULL, &reader);
    if (FAILED(hr)) { MFShutdown(); PostMessageW(g_hWnd, WM_USER+1, (WPARAM)hr, 0); CoUninitialize(); return 1; }

    // 使用解码器默认输出格式

    double srcFps = g_vidFps > 0 ? g_vidFps : 30.0;
    int skip = max(1, (int)(srcFps / fps + 0.5));
    int estFrames = max(1, (int)(g_vidDur * fps));
    int w = g_vidW > 0 ? g_vidW : 640, h = g_vidH > 0 ? g_vidH : 480;

    // Collect frames
    Bitmap **frames = NULL;
    int nFrames = 0, capFrames = estFrames + 100;
    frames = new Bitmap*[capFrames];

    int fi = 0;
    while (nFrames < capFrames)
    {
        IMFSample *sample = NULL;
        DWORD fl, ai; LONGLONG ts;
        hr = reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &ai, &fl, &ts, &sample);
        if (FAILED(hr) || !sample) break;

        if (fi % skip != 0) { sample->Release(); fi++; continue; }

        Bitmap *src = SampleToBitmap(sample, w, h);
        sample->Release();
        if (src)
        {
            Bitmap *rz = new Bitmap(ow, oh);
            Graphics g(rz);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            g.DrawImage(src, 0, 0, ow, oh);
            delete src;
            frames[nFrames++] = rz;
        }
        fi++;

        if ((nFrames % 5) == 0)
        {
            WCHAR st[64]; swprintf(st, 64, L"进度: %d/~%d 帧 (%.0f%%)",
                                   nFrames, estFrames, 100.0*nFrames/estFrames);
            PostMessageW(g_hWnd, WM_USER+2, (WPARAM)(100*nFrames/estFrames), (LPARAM)_wcsdup(st));
        }
    }

    reader->Release();
    MFShutdown();

    if (nFrames == 0) { delete[] frames; PostMessageW(g_hWnd, WM_USER+1, (WPARAM)0xE0000001, 0); CoUninitialize(); return 1; }

    // Ensure output directory
    WCHAR dir[MAX_PATH]; wcscpy(dir, g_outFile);
    WCHAR *slash = wcsrchr(dir, L'\\');
    if (slash) { *slash = 0; CreateDirectoryW(dir, NULL); }

    // Save animated GIF
    CLSID gifClsid;
    CLSIDFromString(L"{557cf402-1a04-11d3-9a73-0000f81ef32e}", &gifClsid);

    // First frame
    EncoderParameters ep;
    ep.Count = 1;
    ep.Parameter[0].Guid = EncoderSaveFlag;
    ep.Parameter[0].Type = EncoderParameterValueTypeLong;
    ep.Parameter[0].NumberOfValues = 1;
    ep.Parameter[0].Value = (VOID*)(ULONG_PTR)EncoderValueMultiFrame;
    frames[0]->Save(g_outFile, &gifClsid, &ep);

    // Subsequent frames
    for (int i = 1; i < nFrames; i++)
    {
        // Set frame delay property (unit: 1/100 second, stored as 2-byte short)
        short delay = (short)max(1, (int)(1000.0 / fps / 10));
        PropertyItem pi;
        pi.id = 0x5100;
        pi.length = sizeof(short);
        pi.type = PropertyTagTypeShort;
        pi.value = &delay;
        frames[i]->SetPropertyItem(&pi);

        ep.Parameter[0].Guid = EncoderSaveFlag;
        ep.Parameter[0].Value = (VOID*)(ULONG_PTR)EncoderValueFrameDimensionTime;
        frames[0]->SaveAdd(frames[i], &ep);
    }

    // Flush
    ep.Parameter[0].Guid = EncoderSaveFlag;
    ep.Parameter[0].Value = (VOID*)(ULONG_PTR)EncoderValueFlush;
    frames[0]->SaveAdd(&ep);

    // Get actual size
    HANDLE hFile = CreateFileW(g_outFile, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, 0, NULL);
    LARGE_INTEGER sz;
    GetFileSizeEx(hFile, &sz);
    CloseHandle(hFile);

    WCHAR buf[256], asz[32];
    FmtSz(sz.QuadPart, asz);
    WCHAR *name = wcsrchr(g_outFile, L'\\');
    if (name) name++; else name = g_outFile;
    swprintf(buf, 256, L"完成! %d帧 %d×%d %.1ffps 大小:%s → %s",
             nFrames, ow, oh, fps, asz, name);

    // Show output preview
    if (g_firstBmp)
    {
        Bitmap *pv = new Bitmap(ow, oh);
        Graphics g2(pv);
        g2.DrawImage(g_firstBmp, 0, 0, ow, oh);
        g_pbOutBmp = ScaleBmp(pv, 215, 160);
        delete pv;
    }

    // Send result to UI
    PostMessageW(g_hWnd, WM_USER+3, nFrames, (LPARAM)_wcsdup(buf));

    // Cleanup
    for (int i = 0; i < nFrames; i++) delete frames[i];
    delete[] frames;
    CoUninitialize();
    return 0;
}

static void StartConv()
{
    if (InterlockedCompareExchange(&g_busy, 1, 0)) return;
    if (wcslen(g_inFile) == 0 || GetFileAttributesW(g_inFile) == INVALID_FILE_ATTRIBUTES)
    { MessageBoxW(g_hWnd, L"请选择有效的输入视频文件。", L"输入错误", MB_OK);
      InterlockedExchange(&g_busy, 0); return; }
    if (wcslen(g_outFile) == 0)
    { MessageBoxW(g_hWnd, L"请指定输出 GIF 文件。", L"输出错误", MB_OK);
      InterlockedExchange(&g_busy, 0); return; }

    double fps = GetFps();
    int ow, oh;
    if (!GetOutSize(&ow, &oh))
    { MessageBoxW(g_hWnd, L"请设置有效的输出尺寸。", L"参数错误", MB_OK);
      InterlockedExchange(&g_busy, 0); return; }

    int nc; BOOL dit;
    switch (SendMessageW(g_cbQ, CB_GETCURSEL, 0, 0))
    { case 0: nc = 256; dit = TRUE; break; case 1: nc = 256; dit = FALSE; break;
      case 2: nc = 128; dit = FALSE; break; default: nc = 64; dit = FALSE; break; }

    EnableWindow(g_btnGo, FALSE);
    SetText(g_btnGo, L"转换中...");
    SetText(g_lblSt, L"启动...");
    SendMessageW(g_pb, PBM_SETPOS, 0, 0);
    if (g_pbOutBmp) { delete g_pbOutBmp; g_pbOutBmp = NULL; }
    SetText(g_lblOT, L"输出GIF (转换中...)");
    InvalidateRect(g_pbOut, NULL, FALSE);

    ConvParams *cp = new ConvParams();
    cp->ow = ow; cp->oh = oh; cp->fps = fps;
    cp->nc = nc; cp->dither = dit;

    HANDLE h = CreateThread(NULL, 0, ConvThread, cp, 0, NULL);
    CloseHandle(h);
}

// ====================================================================
//  NV12 → RGB32 转换 (BT.601, 固定点整数运算)
// ====================================================================
static Bitmap *NV12ToRGB32(const BYTE *data, int w, int h, int stride)
{
    Bitmap *bmp = new Bitmap(w, h, PixelFormat32bppARGB);
    BitmapData bd;
    bmp->LockBits(&Rect(0, 0, w, h), ImageLockModeWrite, PixelFormat32bppARGB, &bd);
    const BYTE *Y  = data;
    const BYTE *UV = data + stride * h;
    for (int y = 0; y < h; y++)
    {
        BYTE *dst = (BYTE*)bd.Scan0 + y * bd.Stride;
        int uvRow = y / 2;
        for (int x = 0; x < w; x++)
        {
            int yVal = Y[y * stride + x];
            int uVal = UV[uvRow * stride + (x & ~1)];
            int vVal = UV[uvRow * stride + (x & ~1) + 1];
            int C = yVal - 16, D = uVal - 128, E = vVal - 128;
            int R = (298 * C + 409 * E + 128) >> 8;
            int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
            int B = (298 * C + 516 * D + 128) >> 8;
            if (R < 0) R = 0; if (R > 255) R = 255;
            if (G < 0) G = 0; if (G > 255) G = 255;
            if (B < 0) B = 0; if (B > 255) B = 255;
            dst[x * 4 + 0] = (BYTE)B;
            dst[x * 4 + 1] = (BYTE)G;
            dst[x * 4 + 2] = (BYTE)R;
            dst[x * 4 + 3] = 255;
        }
    }
    bmp->UnlockBits(&bd);
    return bmp;
}

static Bitmap *SampleToBitmap(IMFSample *sample, int w, int h)
{
    Bitmap *result = NULL;
    DWORD bufCount = 0;
    sample->GetBufferCount(&bufCount);
    if (bufCount == 0) return NULL;

    IMFMediaBuffer *buf = NULL;
    sample->GetBufferByIndex(0, &buf);
    if (!buf) return NULL;

    BYTE *p = NULL; DWORD maxLen = 0, curLen = 0;
    buf->Lock(&p, &maxLen, &curLen);

    if (w <= 0) w = 640;
    if (h <= 0) h = 480;

    // 检查是否是 RGB32 (buf 大小 = w*h*4)
    if ((int)curLen >= w * h * 4)
    {
        // 直接 RGB32 拷贝
        result = new Bitmap(w, h, PixelFormat32bppARGB);
        BitmapData bd;
        result->LockBits(&Rect(0, 0, w, h), ImageLockModeWrite, PixelFormat32bppARGB, &bd);
        int s = w * 4;
        for (int y = 0; y < h; y++)
            memcpy((BYTE*)bd.Scan0 + y * bd.Stride, p + y * s, s);
        result->UnlockBits(&bd);
    }
    else if ((int)curLen >= w * h + w * h / 2)
    {
        // NV12 格式, 手动转换
        result = NV12ToRGB32(p, w, h, w);
    }
    // 其他格式暂不支持, 返回 NULL

    buf->Unlock();
    buf->Release();
    return result;
}

// ====================================================================
//  Preview subclass proc
// ====================================================================
static LRESULT CALLBACK PreviewProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR)
{
    if (m == WM_PAINT)
    {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps);
        RECT r; GetClientRect(h, &r);
        Bitmap *bmp = (id == 1) ? g_pbInBmp : g_pbOutBmp;
        if (bmp) { Graphics g(hdc); g.DrawImage(bmp, 0, 0, r.right, r.bottom); }
        else { FillRect(hdc, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH)); }
        EndPaint(h, &ps); return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

// ====================================================================
//  Window Procedure
// ====================================================================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hWnd = hwnd;
        // ---- Fonts ----
        g_hFont10  = MakeFont(10, FALSE);
        g_hFont10B = MakeFont(10, TRUE);
        g_hFont18B = MakeFont(18, TRUE);
        g_hFont8   = MakeFont(8, FALSE);
        g_hFont12B = MakeFont(12, TRUE);

        int ew = 480, bx = 505, bw = 110;

        // Title
        MakeCtrl(L"STATIC", L"MP4 → GIF 转换器", SS_CENTER | SS_CENTERIMAGE,
                 15, 8, 620, 30, NULL, g_hFont18B);

        // Input file
        MakeCtrl(L"STATIC", L"输入视频:", SS_CENTERIMAGE,
                 15, 48, 70, 20, NULL, g_hFont10);
        g_tbIn = MakeCtrl(L"EDIT", L"", WS_BORDER | ES_READONLY | ES_AUTOHSCROLL,
                          15, 68, ew, 26, NULL, g_hFont10);
        MakeCtrl(L"BUTTON", L"浏览...", BS_PUSHBUTTON,
                 bx, 66, bw, 28, (HMENU)100, g_hFont10);

        // Output file
        MakeCtrl(L"STATIC", L"输出GIF:", SS_CENTERIMAGE,
                 15, 112, 70, 20, NULL, g_hFont10);
        g_tbOut = MakeCtrl(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                           15, 132, ew, 26, NULL, g_hFont10);
        MakeCtrl(L"BUTTON", L"浏览...", BS_PUSHBUTTON,
                 bx, 130, bw, 28, (HMENU)101, g_hFont10);

        // Video info
        g_lblVI = MakeCtrl(L"STATIC", L"未加载视频", SS_CENTERIMAGE,
                           15, 168, 620, 18, NULL, g_hFont10);

        // Separator 1
        MakeCtrl(L"STATIC", L"────────────────────────────────────────────"
                 L"────────────────────────────────────",
                 SS_CENTERIMAGE, 15, 196, 620, 8, NULL, g_hFont10);

        // Size Settings header
        MakeCtrl(L"STATIC", L"尺寸设置", SS_CENTERIMAGE,
                 15, 216, 80, 20, NULL, g_hFont12B);

        // Preset
        MakeCtrl(L"STATIC", L"预设:", SS_CENTERIMAGE,
                 15, 250, 40, 18, NULL, g_hFont10);
        g_cbP = MakeCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                         55, 247, 185, 200, (HMENU)200, g_hFont10);
        {
            LPCWSTR items[] = { L"微信表情包 (300px)", L"小尺寸 (200px)",
                L"中等 (480px)", L"大尺寸 (720px)", L"原始尺寸", L"自定义" };
            for (int i = 0; i < 6; i++)
                SendMessageW(g_cbP, CB_ADDSTRING, 0, (LPARAM)items[i]);
            SendMessageW(g_cbP, CB_SETCURSEL, 0, 0);
        }

        // Ratio checkbox
        g_chkR = MakeCtrl(L"BUTTON", L"保持原始宽高比 (仅需设置最短边)",
                          BS_AUTOCHECKBOX, 15, 286, 340, 24, (HMENU)300, g_hFont10);
        SendMessageW(g_chkR, BM_SETCHECK, BST_CHECKED, 0);

        // Size input row
        HWND rp = MakeCtrl(L"STATIC", L"", WS_CLIPCHILDREN,
                           15, 318, 620, 30, NULL, NULL);

        g_pnlR = MakeCtrl(L"STATIC", L"", WS_CLIPCHILDREN,
                          0, 0, 620, 30, NULL, NULL);
        SetParent(g_pnlR, rp);
        { HWND t; t = MakeCtrl(L"STATIC", L"最短边:", SS_CENTERIMAGE,
                 15, 3, 55, 24, NULL, g_hFont10); SetParent(t, g_pnlR); }
        g_tbSS = MakeCtrl(L"EDIT", L"300", WS_BORDER | ES_CENTER,
                          72, 3, 58, 24, NULL, g_hFont10);
        SetParent(g_tbSS, g_pnlR);
        { HWND t; t = MakeCtrl(L"STATIC", L"像素", SS_CENTERIMAGE,
                 134, 3, 35, 24, NULL, g_hFont10); SetParent(t, g_pnlR); }

        g_pnlM = MakeCtrl(L"STATIC", L"", WS_CLIPCHILDREN,
                          0, 0, 620, 30, NULL, NULL);
        SetParent(g_pnlM, rp);
        ShowWindow(g_pnlM, SW_HIDE);
        { HWND t; t = MakeCtrl(L"STATIC", L"宽度:", SS_CENTERIMAGE,
                 15, 3, 42, 24, NULL, g_hFont10); SetParent(t, g_pnlM); }
        g_tbW = MakeCtrl(L"EDIT", L"", WS_BORDER | ES_CENTER,
                         60, 3, 60, 24, NULL, g_hFont10);
        SetParent(g_tbW, g_pnlM);
        { HWND t; t = MakeCtrl(L"STATIC", L"高度:", SS_CENTERIMAGE,
                 130, 3, 42, 24, NULL, g_hFont10); SetParent(t, g_pnlM); }
        g_tbH = MakeCtrl(L"EDIT", L"", WS_BORDER | ES_CENTER,
                         174, 3, 60, 24, NULL, g_hFont10);
        SetParent(g_tbH, g_pnlM);
        { HWND t; t = MakeCtrl(L"STATIC", L"像素", SS_CENTERIMAGE,
                 240, 3, 35, 24, NULL, g_hFont10); SetParent(t, g_pnlM); }

        // Size info
        g_lblSI = MakeCtrl(L"STATIC", L"输出尺寸: -- × --    预计大小: --",
                           SS_CENTERIMAGE, 15, 354, 620, 22, NULL, g_hFont10B);

        // Separator 2
        MakeCtrl(L"STATIC", L"────────────────────────────────────────────"
                 L"────────────────────────────────────",
                 SS_CENTERIMAGE, 15, 386, 620, 8, NULL, g_hFont10);

        // Quality
        MakeCtrl(L"STATIC", L"画质:", SS_CENTERIMAGE,
                 15, 410, 40, 18, NULL, g_hFont10);
        g_cbQ = MakeCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST,
                         55, 407, 100, 200, (HMENU)201, g_hFont10);
        {
            LPCWSTR qs[] = { L"最佳", L"高", L"中等", L"低" };
            for (int i = 0; i < 4; i++)
                SendMessageW(g_cbQ, CB_ADDSTRING, 0, (LPARAM)qs[i]);
            SendMessageW(g_cbQ, CB_SETCURSEL, 0, 0);
        }

        // FPS
        MakeCtrl(L"STATIC", L"帧率:", SS_CENTERIMAGE,
                 180, 410, 40, 18, NULL, g_hFont10);
        g_tbFps = MakeCtrl(L"EDIT", L"15", WS_BORDER | ES_CENTER,
                           222, 407, 50, 25, NULL, g_hFont10);
        MakeCtrl(L"STATIC", L"fps", SS_CENTERIMAGE,
                 276, 410, 25, 18, NULL, g_hFont10);

        MakeCtrl(L"STATIC", L"最佳=256色+抖动 | 高=256色 | 中=128色 | 低=64色",
                 SS_CENTERIMAGE, 15, 438, 350, 14, NULL, g_hFont8);

        // Convert button
        g_btnGo = MakeCtrl(L"BUTTON", L"开始转换", BS_PUSHBUTTON,
                           180, 465, 260, 38, (HMENU)400, g_hFont10);
        // Color the button green
        // (Win32 doesn't easily support colored buttons without owner-draw)

        // Progress bar
        g_pb = MakeCtrl(L"msctls_progress32", L"", 0,
                        15, 515, 620, 12, NULL, NULL);
        SendMessageW(g_pb, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

        // Status
        g_lblSt = MakeCtrl(L"STATIC", L"就绪", SS_CENTER | SS_CENTERIMAGE,
                           15, 536, 620, 22, NULL, g_hFont10);

        // ---- Right panel previews ----
        int rx = 660, rw = 225;
        MakeCtrl(L"STATIC", L"输入视频", SS_CENTER | SS_CENTERIMAGE,
                 rx, 8, rw, 22, NULL, g_hFont10B);
        g_pbIn = MakeCtrl(L"STATIC", L"", 0,
                          rx+5, 36, rw-10, 160, NULL, NULL);
        SetWindowSubclass(g_pbIn, PreviewProc, 1, 0);

        MakeCtrl(L"STATIC", L"↓", SS_CENTER | SS_CENTERIMAGE,
                 rx, 202, rw, 16, NULL,
                 CreateFontW(18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                             0,0,0,0,L"Microsoft YaHei"));

        g_lblOT = MakeCtrl(L"STATIC", L"输出GIF", SS_CENTER | SS_CENTERIMAGE,
                           rx, 224, rw, 22, NULL, g_hFont10B);
        g_pbOut = MakeCtrl(L"STATIC", L"", 0,
                           rx+5, 252, rw-10, 160, NULL, NULL);
        SetWindowSubclass(g_pbOut, PreviewProc, 2, 0);

        return 0;
    }

    case WM_COMMAND:
    {
        WORD id = LOWORD(wp);
        if (id == 100) // Browse input
        {
            OPENFILENAMEW ofn = { sizeof(ofn) };
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"视频文件\0*.mp4;*.avi;*.mov;*.mkv;*.wmv\0MP4\0*.mp4\0All\0*.*\0";
            ofn.lpstrFile = g_inFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameW(&ofn))
            {
                SetText(g_tbIn, g_inFile);
                wcscpy(g_outFile, g_inFile);
                WCHAR *dot = wcsrchr(g_outFile, L'.');
                if (dot) *dot = 0;
                wcscat(g_outFile, L".gif");
                SetText(g_tbOut, g_outFile);
                LoadVideo();
            }
        }
        else if (id == 101) // Browse output
        {
            OPENFILENAMEW ofn = { sizeof(ofn) };
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"GIF\0*.gif\0";
            ofn.lpstrFile = g_outFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"gif";
            if (GetSaveFileNameW(&ofn))
                SetText(g_tbOut, g_outFile);
        }
        else if (id == 200) // Preset changed
        {
            if (HIWORD(wp) == CBN_SELCHANGE)
            {
                g_lockUI = TRUE;
                int sel = (int)SendMessageW(g_cbP, CB_GETCURSEL, 0, 0);
                switch (sel)
                {
                case 0: SendMessageW(g_chkR, BM_SETCHECK, BST_CHECKED, 0);
                        SetText(g_tbSS, L"300"); break;
                case 1: SendMessageW(g_chkR, BM_SETCHECK, BST_CHECKED, 0);
                        SetText(g_tbSS, L"200"); break;
                case 2: SendMessageW(g_chkR, BM_SETCHECK, BST_CHECKED, 0);
                        SetText(g_tbSS, L"480"); break;
                case 3: SendMessageW(g_chkR, BM_SETCHECK, BST_CHECKED, 0);
                        SetText(g_tbSS, L"720"); break;
                case 4: SendMessageW(g_chkR, BM_SETCHECK, BST_UNCHECKED, 0);
                    if (g_vidW > 0)
                    { WCHAR b[16]; swprintf(b,16,L"%d",g_vidW); SetText(g_tbW,b);
                      swprintf(b,16,L"%d",g_vidH); SetText(g_tbH,b); } break;
                }
                BOOL m = (SendMessageW(g_chkR, BM_GETCHECK,0,0) == BST_CHECKED);
                ShowWindow(g_pnlM, m ? SW_HIDE : SW_SHOW);
                ShowWindow(g_pnlR, m ? SW_SHOW : SW_HIDE);
                g_lockUI = FALSE;
                UpdSizeInfo();
            }
        }
        else if (id == 300) // Ratio changed
        {
            if (!g_lockUI)
            {
                BOOL m = (SendMessageW(g_chkR, BM_GETCHECK,0,0) == BST_CHECKED);
                ShowWindow(g_pnlM, m ? SW_HIDE : SW_SHOW);
                ShowWindow(g_pnlR, m ? SW_SHOW : SW_HIDE);
                SendMessageW(g_cbP, CB_SETCURSEL, 5, 0);
                UpdSizeInfo();
            }
        }
        else if (id == 201) // Quality changed
        { if (HIWORD(wp) == CBN_SELCHANGE) UpdSizeInfo(); }
        else if (id == 400) // Convert
        { StartConv(); }

        // Text change notifications from edit controls
        if (HIWORD(wp) == EN_CHANGE)
        {
            if ((HWND)lp == g_tbW || (HWND)lp == g_tbH ||
                (HWND)lp == g_tbFps || (HWND)lp == g_tbSS)
                UpdSizeInfo();
            if ((HWND)lp == g_tbOut)
                GetText(g_tbOut, g_outFile, MAX_PATH);
            if (!g_lockUI && (HWND)lp == g_tbSS)
                SendMessageW(g_cbP, CB_SETCURSEL, 5, 0);
        }
        return 0;
    }

    case WM_USER+1: // Conversion error
    {
        WCHAR buf[64]; swprintf(buf, 64, L"转换失败: 0x%08X", (int)wp);
        SetText(g_lblSt, buf);
        SetText(g_lblOT, L"输出GIF (失败)");
        EnableWindow(g_btnGo, TRUE);
        SetText(g_btnGo, L"开始转换");
        InterlockedExchange(&g_busy, 0);
        return 0;
    }

    case WM_USER+2: // Progress update
        SendMessageW(g_pb, PBM_SETPOS, (WPARAM)wp, 0);
        free((void*)lp);
        return 0;

    case WM_USER+3: // Conversion done
    {
        int nFrames = (int)wp;
        WCHAR *msg = (WCHAR*)lp;
        SetText(g_lblSt, msg);
        free(msg);
        SetText(g_lblOT, L"输出GIF ✓");
        EnableWindow(g_btnGo, TRUE);
        SetText(g_btnGo, L"开始转换");
        InvalidateRect(g_pbOut, NULL, FALSE);
        InterlockedExchange(&g_busy, 0);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }

    case WM_DESTROY:
        if (g_firstBmp) delete g_firstBmp;
        if (g_pbInBmp) delete g_pbInBmp;
        if (g_pbOutBmp) delete g_pbOutBmp;
        DeleteObject(g_hFont10);
        DeleteObject(g_hFont10B);
        DeleteObject(g_hFont18B);
        DeleteObject(g_hFont8);
        DeleteObject(g_hFont12B);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void UpdSizeInfo()
{
    int ow, oh;
    if (g_vidW == 0 || !GetOutSize(&ow, &oh))
    { SetText(g_lblSI, L"输出尺寸: -- × --    预计大小: --"); return; }
    double fps = GetFps();
    double tf = g_vidDur > 0 ? g_vidDur * fps : 30.0;
    double cr[] = { 5.0, 6.5, 8.5, 11.0 };
    int qi = (int)SendMessageW(g_cbQ, CB_GETCURSEL, 0, 0);
    if (qi < 0) qi = 0; if (qi > 3) qi = 3;
    double r = cr[qi];
    WCHAR sz[32], buf[128];
    FmtSz((long)(ow * oh * tf / r), sz);
    swprintf(buf, 128, L"输出尺寸: %d × %d    预计大小: %s  (%d 帧)",
             ow, oh, sz, (int)(tf + 0.5));
    SetText(g_lblSI, buf);
}

// ====================================================================
//  WinMain
// ====================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    g_hInst = hInst;

    // Init common controls
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    // Init GDI+
    GdiplusStartupInput gsi;
    ULONG_PTR gpt;
    GdiplusStartup(&gpt, &gsi, NULL);

    // Init COM
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Register window class
    WNDCLASSW wc = {};
    wc.hInstance = hInst;
    wc.lpszClassName = L"MP4ToGifV3";
    wc.lpfnWndProc = WndProc;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // Create window (add non-client area to desired client size)
    RECT rc = {0, 0, 900, 600};
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_hWnd = CreateWindowW(L"MP4ToGifV3", L"MP4 → GIF 转换器",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top,
                           NULL, NULL, hInst, NULL);
    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    GdiplusShutdown(gpt);
    return 0;
}
