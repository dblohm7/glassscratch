#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <ShellScalingApi.h>
#include <sstream>
#include <string>

using namespace std;

HWND g_hwndMain;
HINSTANCE g_hInstance;

UINT TOPEXTENDWIDTH;
UINT BOTTOMEXTENDWIDTH;
UINT LEFTEXTENDWIDTH;
UINT RIGHTEXTENDWIDTH;

inline UINT RectWidth(RECT const &r)
{
  return r.right - r.left;
}

inline UINT RectHeight(RECT const &r)
{
  return r.bottom - r.top;
}

class DpiScaler
{
public:
  explicit DpiScaler(HWND hwnd)
    : mXScalePercent(0)
    , mYScalePercent(0)
  {
    UINT x, y;
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &x, &y))) {
      Invalidate(x, y);
    }
  }
  UINT ScaleX(UINT aRaw) const
  {
    return (aRaw * mXScalePercent) / 100U;
  }
  UINT ScaleY(UINT aRaw) const
  {
    return (aRaw * mYScalePercent) / 100U;
  }
  bool operator!() const
  {
    return mXScalePercent == 0 || mYScalePercent == 0;
  }
  UINT GetXScale() const { return mXScalePercent; }
  UINT GetYScale() const { return mYScalePercent; }
  void Invalidate(UINT aNewXDpi, UINT aNewYDpi)
  {
    mXScalePercent = (aNewXDpi * 100U) / NOMINAL_DPI;
    mYScalePercent = (aNewYDpi * 100U) / NOMINAL_DPI;
  }
private:
  UINT mXScalePercent;
  UINT mYScalePercent;

  static const UINT NOMINAL_DPI = 96;
};

DpiScaler* gDpiScaler;

struct GlassMargins : public MARGINS
{
  enum GlassOptions
  {
    eUseSystemBorder,
    eUseSolidSheet
  };

  explicit GlassMargins(GlassOptions aGlassOptions, DpiScaler const &aDpiScaler)
  {
    int xFrameWidth = GetSystemMetrics(SM_CXSIZEFRAME);
    cxFrame = aDpiScaler.ScaleX(xFrameWidth);
    int yFrameWidth = GetSystemMetrics(SM_CYSIZEFRAME);
    cyFrame = aDpiScaler.ScaleY(yFrameWidth);
    int yCaptionWidth = GetSystemMetrics(SM_CYCAPTION);
    cyCaption = aDpiScaler.ScaleY(yCaptionWidth);
    switch (aGlassOptions) {
      case eUseSolidSheet:
        cxLeftWidth = -1;
        cxRightWidth = -1;
        cyTopHeight = -1;
        cyBottomHeight = -1;
        break;
      case eUseSystemBorder:
      default:
        cxLeftWidth = cxFrame;
        cxRightWidth = cxFrame;
        cyTopHeight = cyFrame + cyCaption;
        cyBottomHeight = cyFrame;
        break;
    }
  }

  int cxFrame;
  int cyFrame;
  int cyCaption;
};

GlassMargins* gGlassMargins;

void
RefreshFrame(HWND hwnd)
{
  RECT client;
  GetWindowRect(hwnd, &client);
  SetWindowPos(hwnd, NULL, client.left, client.top,
               RectWidth(client), RectHeight(client), SWP_FRAMECHANGED);
}

static const char kGlassMargins[] = "GlassMargins";

BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
{
  gDpiScaler = new DpiScaler(hwnd);
  gGlassMargins = new GlassMargins(GlassMargins::eUseSolidSheet, *gDpiScaler);
  SetProp(hwnd, kGlassMargins, (HANDLE)gGlassMargins);
  ostringstream oss;
  oss << ((void*&)hwnd);
  SetWindowText(hwnd, oss.str().c_str());
  RefreshFrame(hwnd);
  return TRUE;
}

void OnDestroy(HWND hwnd)
{
  RemoveProp(hwnd, kGlassMargins);
  PostQuitMessage(0);
}

void
RefreshDwmInfo(HWND hwnd)
{
  GlassMargins* margins = (GlassMargins*)GetProp(hwnd, kGlassMargins);
  HRESULT hr = DwmExtendFrameIntoClientArea(hwnd, margins);
  if (FAILED(hr)) {
    MessageBox(hwnd, "Error", "DwmExtendFrameIntoClientArea failed", MB_OK | MB_ICONEXCLAMATION);
  }
  DWMNCRENDERINGPOLICY ncPolicy = DWMNCRP_ENABLED;
  hr = DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncPolicy,
                             sizeof(ncPolicy));
  if (FAILED(hr)) {
    MessageBox(hwnd, "Error", "DwmSetWindowAttribute failed", MB_OK | MB_ICONEXCLAMATION);
  }
  RefreshFrame(hwnd);
}

void
OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL minimized)
{
  RefreshDwmInfo(hwnd);
}

LRESULT
OnNcHitTest(HWND hwnd, int x, int y)
{
  GlassMargins* margins = (GlassMargins*)GetProp(hwnd, kGlassMargins);

  RECT windowRect;
  GetWindowRect(hwnd, &windowRect);

  RECT frameRect = {0};
  AdjustWindowRectEx(&frameRect, WS_OVERLAPPEDWINDOW & ~WS_CAPTION, FALSE,
                     WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);

  UINT row = 1;
  UINT col = 1;
  bool onResizeBorder = false;

  if (y >= windowRect.top && y < windowRect.top + margins->cyFrame + margins->cyCaption) {
    onResizeBorder = y < (windowRect.top - frameRect.top);
    row = 0;
  } else if (y < windowRect.bottom && y >= windowRect.bottom - margins->cyFrame) {
    row = 2;
  }

  if (x >= windowRect.left && x < windowRect.left + margins->cxFrame) {
    col = 0;
  } else if (x < windowRect.right && x >= windowRect.right - margins->cyFrame) {
    col = 2;
  }

  LRESULT grid[3][3] = {
    {HTTOPLEFT, onResizeBorder ? HTTOP : HTCAPTION, HTTOPRIGHT},
    {HTLEFT, HTNOWHERE, HTRIGHT},
    {HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT}
  };

  return grid[row][col];
}

void
OnSize(HWND hwnd, UINT state, int cx, int cy)
{
  RefreshFrame(hwnd);
}

LRESULT
NcWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool &handled)
{
  LRESULT lresult = 0;
  handled = !!DwmDefWindowProc(hwnd, uMsg, wParam, lParam, &lresult);
  switch (uMsg) {
    case WM_ACTIVATE:
      handled = true;
      lresult = HANDLE_WM_ACTIVATE(hwnd, wParam, lParam, OnActivate);
      break;
    HANDLE_MSG(hwnd, WM_SIZE, OnSize);
#ifndef NOCALCSIZE
    case WM_NCCALCSIZE:
      if (wParam) {
        handled = true;
        lresult = 0;
      }
      break;
#endif
#ifndef NOHITTEST
    case WM_NCHITTEST:
      if (lresult == 0) {
        lresult = HANDLE_WM_NCHITTEST(hwnd, wParam, lParam, OnNcHitTest);
        if (lresult != HTNOWHERE) {
          handled = true;
        }
      }
      break;
#endif
    default:
      break;
  }
  return lresult;
}

void
DrawCaption(HWND hwnd, HDC hdc)
{
  RECT rcClient;
  GetClientRect(hwnd, &rcClient);

  HTHEME hTheme = OpenThemeData(NULL, L"CompositedWindow::Window");
  if (hTheme) {
    HDC hdcPaint = CreateCompatibleDC(hdc);
    if (hdcPaint) {
      int cx = RectWidth(rcClient);
      int cy = RectHeight(rcClient);

      // Define the BITMAPINFO structure used to draw text.
      // Note that biHeight is negative. This is done because
      // DrawThemeTextEx() needs the bitmap to be in top-to-bottom
      // order.
      BITMAPINFO dib = { 0 };
      dib.bmiHeader.biSize            = sizeof(BITMAPINFOHEADER);
      dib.bmiHeader.biWidth           = cx;
      dib.bmiHeader.biHeight          = -cy;
      dib.bmiHeader.biPlanes          = 1;
      dib.bmiHeader.biBitCount        = 32;
      dib.bmiHeader.biCompression     = BI_RGB;

      HBITMAP hbm = CreateDIBSection(hdc, &dib, DIB_RGB_COLORS, NULL, NULL, 0);
      if (hbm) {
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcPaint, hbm);

        // Setup the theme drawing options.
        DTTOPTS DttOpts = {sizeof(DTTOPTS)};
        DttOpts.dwFlags = DTT_COMPOSITED | DTT_GLOWSIZE;
        DttOpts.iGlowSize = 15;

        // Select a font.
        LOGFONTW lgFont;
        HFONT hFontOld = NULL;
        if (SUCCEEDED(GetThemeSysFont(hTheme, TMT_CAPTIONFONT, &lgFont))) {
          HFONT hFont = CreateFontIndirectW(&lgFont);
          hFontOld = (HFONT) SelectObject(hdcPaint, hFont);
        }

        // Draw the title.
        RECT rcPaint = rcClient;
        rcPaint.top += gDpiScaler->ScaleX(8);
        rcPaint.right -= gDpiScaler->ScaleX(125);
        rcPaint.left += gDpiScaler->ScaleX(8);
        rcPaint.bottom = gDpiScaler->ScaleX(50);

        wchar_t text[256] = {0};
        GetWindowTextW(hwnd, text, 255);
        DrawThemeTextEx(hTheme, hdcPaint, 0, 0, text, -1,
                        DT_LEFT | DT_WORD_ELLIPSIS, &rcPaint, &DttOpts);

        // Blit text to the frame.
        BitBlt(hdc, 0, 0, cx, cy, hdcPaint, 0, 0, SRCCOPY);

        SelectObject(hdcPaint, hbmOld);
        if (hFontOld) {
          SelectObject(hdcPaint, hFontOld);
        }
        DeleteObject(hbm);
      }
      DeleteDC(hdcPaint);
    }
    CloseThemeData(hTheme);
  }
}

void
OnPaint(HWND hwnd)
{
  HDC hdc;
  PAINTSTRUCT ps;
  hdc = BeginPaint(hwnd, &ps);
  DrawCaption(hwnd, hdc);
  EndPaint(hwnd, &ps);
}

void
OnDpiChanged(HWND hwnd, UINT newXDpi, UINT newYDpi, RECT const &newScaledWindowRect)
{
  gDpiScaler->Invalidate(newXDpi, newYDpi);
  SetWindowPos(hwnd, HWND_TOP,
               newScaledWindowRect.left,
               newScaledWindowRect.top,
               RectWidth(newScaledWindowRect),
               RectHeight(newScaledWindowRect),
               SWP_NOZORDER | SWP_NOACTIVATE);
  RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  bool handled = false;
  LRESULT lResult = NcWndProc(hwnd, uMsg, wParam, lParam, handled);
  if (handled) {
    return lResult;
  }
  switch (uMsg) {
    HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
    HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
    case WM_DPICHANGED:
      OnDpiChanged(hwnd, LOWORD(wParam), HIWORD(wParam), *((RECT*)lParam));
      return 0;
    default:
      break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
{
  g_hInstance = hInstance;

  if (FAILED(SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE))) {
    return 1;
  }

  WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  // wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = "FooClass";

  if (!RegisterClassEx(&wc)) {
    return 1;
  }

  HWND hwnd = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, /* We do NOT want WS_EX_CLIENTEDGE */
                             "FooClass",
                             "Scratch",
                             WS_OVERLAPPEDWINDOW | WS_POPUPWINDOW,
                             0,
                             0,
                             640,
                             480,
                             NULL,
                             NULL,
                             hInstance,
                             NULL);
  if (!hwnd) {
    return 1;
  }
  g_hwndMain = hwnd;

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  MSG msg;
  BOOL bRet;
  while (bRet = GetMessage(&msg, NULL, 0, 0)) {
    if (bRet == -1) {
      return 1;
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  return (DWORD)(msg.wParam);
}

