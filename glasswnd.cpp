#include "glasswnd.h"

#include <windowsx.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <ShellScalingApi.h>
#include <sstream>

namespace aspk {

typedef LRESULT (WINAPI* GetDpiForMonitorPtr)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*);

DpiScaler::DpiScaler(HWND aHwnd)
  : mXScalePercent(0)
  , mYScalePercent(0)
{
  UINT x, y;
  // Try the Windows 8 monitor-aware way
  HMODULE shcore = LoadLibraryW(L"shcore.dll");
  if (shcore) {
    GetDpiForMonitorPtr pGetDpiForMonitor = (GetDpiForMonitorPtr) GetProcAddress(shcore, "GetDpiForMonitor");
    if (pGetDpiForMonitor) {
      HMONITOR monitor = MonitorFromWindow(aHwnd, MONITOR_DEFAULTTONEAREST);
      if (SUCCEEDED(pGetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &x, &y))) {
        Invalidate(x, y);
        return;
      }
    }
    FreeLibrary(shcore);
  }
  // Oh well, try the old system-wide way
  HDC dc = GetDC(NULL);
  if (!dc) {
    return;
  }
  x = GetDeviceCaps(dc, LOGPIXELSX);
  y = GetDeviceCaps(dc, LOGPIXELSY);
  ReleaseDC(NULL, dc);
  Invalidate(x, y);
}

wchar_t const GlassWindow::kClassName[] = L"ASPKGlassWindowClass";
wchar_t const GlassWindow::kGlassWindowKey[] = L"ASPKGlassWindowProp";

GlassWindow::GlassWindow(HINSTANCE aInstance, std::wstring const &aTitleText)
  : mHwnd(NULL)
{
  WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &GlassWindow::WndProc;
  wc.hInstance = aInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.lpszClassName = kClassName;

  if (!RegisterClassExW(&wc)) {
    return;
  }

  mHwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE, /* We do NOT want WS_EX_CLIENTEDGE */
                          kClassName,
                          aTitleText.c_str(),
                          WS_OVERLAPPEDWINDOW | WS_POPUPWINDOW,
                          0,
                          0,
                          640,
                          480,
                          NULL,
                          NULL,
                          aInstance,
                          this);
}

GlassWindow::~GlassWindow()
{
}

void
GlassWindow::Show(int aShow)
{
  ShowWindow(mHwnd, aShow);
}

void
GlassWindow::Update()
{
  UpdateWindow(mHwnd);
}

void
GlassWindow::RefreshFrame(HWND hwnd)
{
  RECT client;
  GetWindowRect(hwnd, &client);
  SetWindowPos(hwnd, NULL, client.left, client.top,
               RectWidth(client), RectHeight(client), SWP_FRAMECHANGED);
}

void
GlassWindow::RefreshDwmInfo(HWND hwnd)
{
  GlassWindow* instance = (GlassWindow*)GetPropW(hwnd, kGlassWindowKey);
  HRESULT hr = DwmExtendFrameIntoClientArea(hwnd, instance->mMargins.get());
  if (FAILED(hr)) {
    MessageBox(hwnd, L"Error", L"DwmExtendFrameIntoClientArea failed", MB_OK | MB_ICONEXCLAMATION);
  }
  DWMNCRENDERINGPOLICY ncPolicy = DWMNCRP_ENABLED;
  hr = DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncPolicy,
                             sizeof(ncPolicy));
  if (FAILED(hr)) {
    MessageBox(hwnd, L"Error", L"DwmSetWindowAttribute failed", MB_OK | MB_ICONEXCLAMATION);
  }
  RefreshFrame(hwnd);
}

void
GlassWindow::OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL minimized)
{
  RefreshDwmInfo(hwnd);
}

void
GlassWindow::OnSize(HWND hwnd, UINT state, int cx, int cy)
{
  RefreshFrame(hwnd);
}

LRESULT
GlassWindow::OnNcHitTest(HWND hwnd, int x, int y)
{
  GlassWindow* instance = (GlassWindow*)GetPropW(hwnd, kGlassWindowKey);
  auto& margins = instance->mMargins;

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

LRESULT
GlassWindow::NcWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                       bool &handled)
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
GlassWindow::OnCreate(HWND aHwnd)
{
  mHwnd = aHwnd;
  SetPropW(aHwnd, kGlassWindowKey, (HANDLE)this);
  mDpiScaler = std::make_unique<DpiScaler>(aHwnd);
  mMargins = std::make_unique<GlassMargins>(GlassMargins::eUseSolidSheet, *mDpiScaler);
}

BOOL
GlassWindow::OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
{
  GlassWindow* instance = (GlassWindow*)lpcs->lpCreateParams;
  instance->OnCreate(hwnd);
  std::wostringstream oss;
  oss << ((void*&)hwnd);
  SetWindowTextW(hwnd, oss.str().c_str());
  RefreshFrame(hwnd);
  return TRUE;
}

void
GlassWindow::OnDestroy(HWND hwnd)
{
  PostQuitMessage(0);
}

void
GlassWindow::OnNcDestroy(HWND hwnd)
{
  RemovePropW(hwnd, kGlassWindowKey);
}

void
GlassWindow::DrawCaption(HWND hwnd, HDC hdc)
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
        GlassWindow* instance = (GlassWindow*)GetPropW(hwnd, kGlassWindowKey);
        auto& dpiScaler = instance->mDpiScaler;
        RECT rcPaint = rcClient;
        rcPaint.top += dpiScaler->ScaleX(8);
        rcPaint.right -= dpiScaler->ScaleX(125);
        rcPaint.left += dpiScaler->ScaleX(8);
        rcPaint.bottom = dpiScaler->ScaleX(50);

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
GlassWindow::OnPaint(HWND hwnd)
{
  HDC hdc;
  PAINTSTRUCT ps;
  hdc = BeginPaint(hwnd, &ps);
  DrawCaption(hwnd, hdc);
  EndPaint(hwnd, &ps);
}

void
GlassWindow::OnDpiChanged(HWND hwnd, UINT newXDpi, UINT newYDpi, RECT const &newScaledWindowRect)
{
  GlassWindow* instance = (GlassWindow*)GetPropW(hwnd, kGlassWindowKey);
  instance->mDpiScaler->Invalidate(newXDpi, newYDpi);
  SetWindowPos(hwnd, HWND_TOP,
               newScaledWindowRect.left,
               newScaledWindowRect.top,
               RectWidth(newScaledWindowRect),
               RectHeight(newScaledWindowRect),
               SWP_NOZORDER | SWP_NOACTIVATE);
  RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

LRESULT CALLBACK
GlassWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  bool handled = false;
  LRESULT lResult = NcWndProc(hwnd, uMsg, wParam, lParam, handled);
  if (handled) {
    return lResult;
  }
  switch (uMsg) {
    HANDLE_MSG(hwnd, WM_CREATE, OnCreate);
    HANDLE_MSG(hwnd, WM_DESTROY, OnDestroy);
    case WM_DPICHANGED:
      OnDpiChanged(hwnd, LOWORD(wParam), HIWORD(wParam), *((RECT*)lParam));
      return 0;
    HANDLE_MSG(hwnd, WM_NCDESTROY, OnNcDestroy);
    HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
    default:
      break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace aspk

