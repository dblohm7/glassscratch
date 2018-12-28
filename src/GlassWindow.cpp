#include "GlassWindow.h"
#include "PaintContext.h"
#include "UniqueHandle.h"

#include <windows.h>

#include <gdiplus.h>
#include <windowsx.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <VersionHelpers.h>
#include <wtsapi32.h>

#include <assert.h>
#include <vector>

#include "odbs.h"

namespace aspk {

void
GlassMargins::Invalidate(GlassWindow* aGlassWindow)
{
  int paddedBorderWidth = GetSystemMetrics(SM_CXPADDEDBORDER);
  odbs(L"CXPADDEDBORDER: ", paddedBorderWidth);
  int yFrameWidth = GetSystemMetrics(SM_CYSIZEFRAME);
  odbs(L"CYSIZEFRAME: ", yFrameWidth);
  int yCaptionWidth = GetSystemMetrics(SM_CYCAPTION);
  odbs(L"CYCAPTION: ", yCaptionWidth);
  mCaptionWidth = ScaledDimensionY(mNcDpiScaler, yFrameWidth + paddedBorderWidth + yCaptionWidth);
  if (IsWindows10OrGreater()) {
    int xEdge = GetSystemMetrics(SM_CXEDGE);
    odbs(L"CXEDGE: ", xEdge);
    mFrameBorderXWidth = ScaledDimensionX(mNcDpiScaler, xEdge);
    int yEdge = GetSystemMetrics(SM_CYEDGE);
    odbs(L"CYEDGE: ", yEdge);
    mFrameBorderYWidth = ScaledDimensionY(mNcDpiScaler, yEdge);
  } else {
    int xFrameWidth = GetSystemMetrics(SM_CXSIZEFRAME);
    odbs(L"CXSIZEFRAME: ", xFrameWidth);
    mFrameBorderXWidth = ScaledDimensionX(mNcDpiScaler, xFrameWidth + paddedBorderWidth);
    mFrameBorderYWidth = ScaledDimensionY(mNcDpiScaler, yFrameWidth + paddedBorderWidth);
  }

  odbs(L"GlassMargins::Invalidate mFrameBorderXWidth: ", mFrameBorderXWidth.GetValue(),
       L", mFrameBorderYWidth: ", mFrameBorderYWidth.GetValue(),
       L", mCaptionWidth: ", mCaptionWidth.GetValue());

  if (HasUserMargins()) {
    return;
  }

#ifdef RENDER_CAPTION_IN_CLIENT
  mMargins.cyTopHeight = mCaptionWidth.ScaleTo(mDpiScaler).GetValue();
#endif

  odbs(L"mMargins.cyTopHeight: ", mMargins.cyTopHeight);
  if (!IsWindows10OrGreater()) {
    // Margins are one pixel, but the borders are actually 2
    // mMargins.cyTopHeight += 1;
    mMargins.cxLeftWidth = 1;
    mMargins.cxRightWidth = 1;
    mMargins.cyBottomHeight = 1;
  }
}

/* static */ bool
GlassMargins::HasMargins(MARGINS const & aMargins)
{
  return aMargins.cxLeftWidth != 0 ||
         aMargins.cxRightWidth != 0 ||
         aMargins.cyTopHeight != 0 ||
         aMargins.cyBottomHeight != 0;
}

GlassWindow::Params::Params()
  : mWidth(640)
  , mHeight(480)
  , mStyleToggles(0)
  , mExStyleToggles(0)
  , mMargins{}
  , mBackgroundBrush((HBRUSH)(COLOR_WINDOW + 1))
  , mQuitOnDestroy(false)
  , mVisualDebug(false)
{
}

void
GlassWindow::Params::SetFlags(unsigned int aFlags)
{
  if ((aFlags & eSolidGlass) != 0 && IsWindows10OrGreater()) {
    // Basically unsupported on Win10
    aFlags &= ~eSolidGlass;
  }

  if (aFlags & eAlwaysOnTop) {
    mExStyleToggles |= WS_EX_TOPMOST;
  }

  if (aFlags & eFixedSize) {
    mStyleToggles |= WS_THICKFRAME;
  }

  if (aFlags & eSolidGlass) {
    mMargins = {-1, -1, -1, -1};
  }

  {
    // TODO: Only do this if we're compositing
    /* We do NOT want WS_EX_CLIENTEDGE for glass */
    mExStyleToggles |= WS_EX_CLIENTEDGE;
    if (!IsWindows10OrGreater()) {
      mBackgroundBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
    }
  }

  if (aFlags & eQuitOnDestroy) {
    mQuitOnDestroy = true;
  }

  if (aFlags & eVisualDebug) {
    mVisualDebug = true;
  }
}

wchar_t const GlassWindow::kClassName[] = L"ASPKGlassWindowClass";

GlassWindow::GlassWindow(HINSTANCE aInstance, std::wstring const &aTitleText)
  : mInstance(aInstance)
  , mHwnd(NULL)
  , mWTSRegistered(false)
  , mQuitOnDestroy(false)
  , mDebug(false)
  , mPrintfBufLen(0)
{
  MARGINS margins = {0};
  Init(aTitleText, 0, 0, 640, 480, margins, (HBRUSH)(COLOR_WINDOW + 1));
}

GlassWindow::GlassWindow(HINSTANCE aInstance, GlassWindow::Params const &aParams)
  : mInstance(aInstance)
  , mHwnd(NULL)
  , mWTSRegistered(false)
  , mQuitOnDestroy(aParams.QuitOnDestroy())
  , mDebug(aParams.IsVisualDebugMode())
  , mPrintfBufLen(0)
{
  Init(aParams.GetTitleText(),
       aParams.GetStyleToggles(),
       aParams.GetExStyleToggles(),
       aParams.GetWidth(),
       aParams.GetHeight(),
       aParams.GetMargins(),
       aParams.GetBackgroundBrush());
}

void
GlassWindow::Init(std::wstring const &aTitleText,
                  DWORD aStyleToggles,
                  DWORD aExStyleToggles,
                  int aWidth,
                  int aHeight,
                  MARGINS const & aMargins,
                  HBRUSH aBackgroundBrush)
{
  WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wc.lpfnWndProc = &GlassWindow::WndProc;
  wc.hInstance = mInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  if (mDebug) {
    wc.hbrBackground = CreateSolidBrush(RGB(0xFF, 0, 0));
  } else {
    wc.hbrBackground = aBackgroundBrush;
  }
  wc.lpszClassName = kClassName;

  if (!RegisterClassExW(&wc)) {
    return;
  }

  DWORD styles = WS_OVERLAPPEDWINDOW | WS_POPUPWINDOW;
  styles ^= aStyleToggles;
  DWORD exStyles = WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW;
  exStyles ^= aExStyleToggles;

  auto context = std::make_pair(this, &aMargins);
  mHwnd = CreateWindowExW(exStyles,
                          kClassName,
                          aTitleText.c_str(),
                          styles,
                          CW_USEDEFAULT,
                          CW_USEDEFAULT,
                          aWidth,
                          aHeight,
                          NULL,
                          NULL,
                          mInstance,
                          &context);

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
GlassWindow::MaybeCreateListView(const size_t aNumCols)
{
  if (mListView) {
    return;
  }

  mListView = std::make_unique<ListView>(*this);
  if (!(*mListView)) {
    return;
  }

  for (size_t i = 0; i < aNumCols; ++i) {
    bool ok = mListView->InsertColumn();
    assert(ok);
  }
}

void
GlassWindow::Printf(const wchar_t* aFmt, ...)
{
  va_list argptr;
  va_start(argptr, aFmt);

  int result = -1;

  if (mPrintfBufLen) {
    result = _vsnwprintf_s(mPrintfBuf.get(), mPrintfBufLen, _TRUNCATE, aFmt,
                           argptr);
  }

  if (result == -1) {
    mPrintfBufLen = _vscwprintf(aFmt, argptr) + 1;
    mPrintfBuf.reset(new wchar_t[mPrintfBufLen]);

    _vsnwprintf_s(mPrintfBuf.get(), mPrintfBufLen, _TRUNCATE, aFmt, argptr);
  }

  if (mListView || wcschr(mPrintfBuf.get(), L'\t')) {
    // Send to ListView

    // First, split mPrintfBuf
    std::vector<wchar_t*> items;
    wchar_t* context = nullptr;
    wchar_t* tok = wcstok_s(mPrintfBuf.get(), L"\t", &context);
    while (tok) {
      items.push_back(tok);
      tok = wcstok_s(nullptr, L"\t", &context);
    }

    MaybeCreateListView(items.size());

    for (auto&& item : items) {
      bool ok = mListView->InsertCell(item);
      assert(ok);
    }
  } else {
    ::InvalidateRect(mHwnd, nullptr, TRUE);
  }

  va_end(argptr);
}

void
GlassWindow::RefreshFrame(HWND hwnd)
{
  GlassWindow* instance = reinterpret_cast<GlassWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
               SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  // Need to force a WM_ERASEBKGND
  InvalidateRect(hwnd, nullptr, TRUE);
}

void
GlassWindow::RefreshDwmInfo(HWND hwnd)
{
  GlassWindow* instance = reinterpret_cast<GlassWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (instance->mMargins->HasMargins()) {
    HRESULT hr = DwmExtendFrameIntoClientArea(hwnd, instance->mMargins->GetMargins());
    if (FAILED(hr)) {
      odbs(L"DwmExtendFrameIntoClientArea failed");
    }
    DWMNCRENDERINGPOLICY ncPolicy = DWMNCRP_ENABLED;
    hr = DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &ncPolicy,
                               sizeof(ncPolicy));
    if (FAILED(hr)) {
      odbs(L"DwmSetWindowAttribute failed");
    }

    DWM_BLURBEHIND dwmBlur = {
      DWM_BB_ENABLE | DWM_BB_TRANSITIONONMAXIMIZED,
      TRUE,
      NULL,
      TRUE
    };
    hr = DwmEnableBlurBehindWindow(hwnd, &dwmBlur);
    if (FAILED(hr)) {
      odbs(L"DwmEnableBlurBehindWindow failed");
    }
  }

  RefreshFrame(hwnd);
}

void
GlassWindow::OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL minimized)
{
  // TODO ASK: Only call RefreshDwmInfo on first activation
  RefreshDwmInfo(hwnd);
}

ScaledRect
GlassWindow::GetFrameInfo(unsigned int aFlags)
{
  // WS_CAPTION implies WS_BORDER, so we need to switch caption off without killing border as well
  DWORD style = GetWindowLong(*this, GWL_STYLE) & ~(WS_CAPTION ^ WS_BORDER);
  DWORD exStyle = GetWindowLong(*this, GWL_EXSTYLE);
  if (IsWindows10OrGreater() && !(aFlags & eFIIncludeSizeFrame)) {
    // For measurement purposes we should hide WS_THICKFRAME
    style &= ~WS_THICKFRAME;
  }
  // TODO: Use this technique for setting margins on Win10
  ScaledRect frameRect(GetNcDpiScaler());
  AdjustWindowRectEx(&frameRect, style, FALSE, exStyle);
  if (aFlags & eFINoRescale) {
    return frameRect;
  }
  return frameRect.ScaleTo(GetDpiScaler());
}

void
GlassWindow::GetClientRectInset(RECT &aRect)
{
  if (IsWindows10OrGreater()) {
    GlassMargins const & margins = GetMargins();
    std::shared_ptr<DpiScaler const> scaler = GetDpiScaler();
    aRect.left += margins.GetFrameBorderXWidth(scaler).GetValue();
    aRect.right -= margins.GetFrameBorderXWidth(scaler).GetValue();
    aRect.bottom -= margins.GetFrameBorderYWidth(scaler).GetValue();
  }
}

bool
GlassWindow::WindowToClient(ScaledRect &aRect)
{
  RECT windowRect;
  ::GetWindowRect(mHwnd, &windowRect);
  RECT clientInset(windowRect);
  GetClientRectInset(clientInset);
  RECT diffRect = {
    windowRect.left - clientInset.left,
    windowRect.top - clientInset.top,
    windowRect.right - clientInset.right,
    windowRect.bottom - clientInset.bottom,
  };
  OffsetRect(&aRect, diffRect.left, diffRect.top);
  return true;
}

bool
GlassWindow::ConvertMouseCoords(LPARAM &aParam)
{
  POINT pt = {GET_X_LPARAM(aParam), GET_Y_LPARAM(aParam)};
  SetLastError(ERROR_SUCCESS);
  if (!MapWindowPoints(NULL, mHwnd, &pt, 1) &&
      GetLastError() != ERROR_SUCCESS) {
    odbs(L"ConvertMouseCoords failed");
    return false;
  }
  aParam = MAKELONG(pt.x, pt.y);
  return true;
}

LRESULT
GlassWindow::NcWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                       bool &handled)
{
  GlassWindow* instance = reinterpret_cast<GlassWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  LRESULT lresult = 0;

  handled = !!DwmDefWindowProc(hwnd, uMsg, wParam, lParam, &lresult);

  switch (uMsg) {
    case WM_ACTIVATE:
      lresult = HANDLE_WM_ACTIVATE(hwnd, wParam, lParam, OnActivate);
      break;
    case WM_DWMNCRENDERINGCHANGED:
      RefreshFrame(hwnd);
      break;
    case WM_THEMECHANGED: {
      if (instance) {
        instance->OnThemeChanged();
      }
      break;
    }
    default:
      break;
  }
  return lresult;
}

void
GlassWindow::OnCreate(HWND aHwnd, MARGINS const & aMargins)
{
  mHwnd = aHwnd;
  SetWindowLongPtrW(aHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

  mWTSRegistered =
    ::WTSRegisterSessionNotification(aHwnd, NOTIFY_FOR_THIS_SESSION);

  BufferedPaintInit();
  mDpiScaler = std::make_shared<DpiScaler>(mHwnd);
  POINT pt = {0, 0};

  static auto pGetThreadDpiAwarenessContext =
    reinterpret_cast<decltype(&::GetThreadDpiAwarenessContext)>(
      ::GetProcAddress(::GetModuleHandle(L"user32.dll"),
                       "GetThreadDpiAwarenessContext"));

  if (pGetThreadDpiAwarenessContext &&
      pGetThreadDpiAwarenessContext() == DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) {
    mNcDpiScaler = mDpiScaler;
  } else {
    mNcDpiScaler = std::make_shared<DpiScaler>();
  }

  odbs(L"mNcDpiScaler is running at ", mNcDpiScaler->GetXScale(), L"%");
  mMargins = std::make_unique<GlassMargins>(aMargins, mDpiScaler, mNcDpiScaler);
  mMargins->Invalidate(this);
  odbs(L"GetThemeAppProperties: ", GetThemeAppProperties());
  RefreshFrame(aHwnd);
}

BOOL
GlassWindow::OnCreate(HWND hwnd, LPCREATESTRUCT lpcs)
{
  typedef std::pair<GlassWindow*,MARGINS const *>* ContextType;
  ContextType context = (ContextType)lpcs->lpCreateParams;
  context->first->OnCreate(hwnd, *context->second);
  return TRUE;
}

void
GlassWindow::OnPaint(HDC aDc)
{
  if (mListView || !mPrintfBuf) {
    return;
  }

  ScaledRect clientRect(GetDpiScaler());
  if (!GetClientRect(clientRect)) {
    return;
  }

  // TODO: Adjust client rect with some padding
  // TODO: a11y for window text

  UniqueThemeHandle theme(::OpenThemeData(mHwnd, L"CompositedWindow::Window"));

  LOGFONT logFont;
  if (FAILED(GetThemeSysFont(theme.get(), TMT_MSGBOXFONT, &logFont))) {
    return;
  }

  UniqueGdiHandle font(::CreateFontIndirect(&logFont));
  if (!font) {
    return;
  }

  SelectObject(aDc, (HGDIOBJ)font.get());

  DTTOPTS dttOpts = { sizeof(DTTOPTS) };
  dttOpts.dwFlags = DTT_COMPOSITED;
  odbs(L"Text rect: ", *reinterpret_cast<RECT*>(clientRect.ptr()));
  HRESULT hr = DrawThemeTextEx(theme.get(), aDc, 0, 0, mPrintfBuf.get(),
                               wcslen(mPrintfBuf.get()), DT_LEFT, &clientRect,
                               &dttOpts);
  if (FAILED(hr)) {
    return;
  }
}

void
GlassWindow::OnDestroy()
{
  BufferedPaintUnInit();

  if (mWTSRegistered) {
    if (::WTSUnRegisterSessionNotification(mHwnd)) {
      mWTSRegistered = false;
    }
  }

  if (mQuitOnDestroy) {
    PostQuitMessage(0);
  }
}

void
GlassWindow::OnDestroy(HWND hwnd)
{
  GlassWindow* instance = reinterpret_cast<GlassWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  instance->OnDestroy();
}

void
GlassWindow::OnNcDestroy(HWND hwnd)
{
}

void
GlassWindow::DrawDebugRect(HDC aDc, RECT const &aRect, COLORREF aColor)
{
  if (mDebug) {
    HBRUSH hbr = CreateSolidBrush(aColor);
    FrameRect(aDc, &aRect, hbr);
    DeleteObject(hbr);
  }
}

void
GlassWindow::OnErase(HDC aDc, RECT const &aRect)
{
  odbs(L"GlassWindow::OnErase -- ", aRect);
  RECT clientEraseRect(aRect);
  if (!IsWindows10OrGreater() || IsRectEmpty(&clientEraseRect)) {
    return;
  }

  WNDCLASSEX clsEx = { sizeof(clsEx) };
  GetClassInfoEx(mInstance, kClassName, &clsEx);

  // Using GDI+ here so that the background is alpha-aware

  HBRUSH bgBrush = clsEx.hbrBackground;
  // If the window class specifies a system brush color, convert it to a real
  // HBRUSH so that we can query it.
  if (bgBrush <= ((HBRUSH)(COLOR_MENUBAR + 1))) {
    bgBrush = GetSysColorBrush(((intptr_t)bgBrush) - 1);
  }
  // Get the color of the GDI HBRUSH
  LOGBRUSH logBrush = {0};
  int dataLen = GetObject(bgBrush, sizeof(logBrush), &logBrush);
  if (dataLen != sizeof(logBrush)) {
    odbs(L"GetObject failed");
    return;
  }
  odbs(L"clientEraseRect: ", clientEraseRect);
  if (logBrush.lbStyle != BS_SOLID) {
    odbs(L"Unsupported background brush style");
    return;
  }
  // Now draw with GDI+
  Gdiplus::Graphics gfx(aDc);
  Gdiplus::Color bgColor;
  bgColor.SetFromCOLORREF(logBrush.lbColor);
  Gdiplus::SolidBrush solidBrush(bgColor);
  Gdiplus::Rect gdipBgRect(clientEraseRect.left,
                           clientEraseRect.top,
                           RectWidth(clientEraseRect),
                           RectHeight(clientEraseRect));
  gfx.FillRectangle(&solidBrush, gdipBgRect);
  // FillRect(aDc, &clientEraseRect, bgBrush);
}

void
GlassWindow::OnPaint(HWND hwnd)
{
  GlassWindow* instance = reinterpret_cast<GlassWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  bool local = !IsRemote();
  PaintContext paintContext(hwnd);
  PAINTSTRUCT &ps = paintContext.GetPaintStruct();
  HDC hdc = paintContext.GetDC();

  if (BufferedPaintRenderAnimation(hwnd, hdc)) {
    return;
  }

  HPAINTBUFFER buffer = NULL;
  HDC paintDC = hdc;
#ifndef NO_BUFFERED_PAINT
  if (local) {
    buffer = BeginBufferedPaint(hdc, &ps.rcPaint, BPBF_TOPDOWNDIB, nullptr, &paintDC);
  }
#endif
  if (ps.fErase) {
    instance->OnErase(paintDC, ps.rcPaint);
  }
  instance->OnPaint(paintDC);
#ifndef NO_BUFFERED_PAINT
  if (local && buffer) {
    EndBufferedPaint(buffer, TRUE);
  }
#endif
}

void
GlassWindow::OnDpiChanged(HWND hwnd, UINT newXDpi, UINT newYDpi, RECT const &newScaledWindowRect)
{
  GlassWindow* instance = reinterpret_cast<GlassWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  instance->mDpiScaler->Invalidate(newXDpi, newYDpi);
  instance->mMargins->Invalidate(instance);
  SetWindowPos(hwnd, HWND_TOP,
               newScaledWindowRect.left,
               newScaledWindowRect.top,
               RectWidth(newScaledWindowRect),
               RectHeight(newScaledWindowRect),
               SWP_NOZORDER | SWP_NOACTIVATE);
  RefreshDwmInfo(hwnd);
  RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
}

void
GlassWindow::OnThemeChanged()
{
}

BOOL
GlassWindow::OnEraseBackground(HWND aHwnd, HDC aDc)
{
  return FALSE;
}

void
GlassWindow::OnSize(HWND aHwnd, UINT aState, int aCx, int aCy)
{
  GlassWindow* instance = reinterpret_cast<GlassWindow*>(GetWindowLongPtrW(aHwnd, GWLP_USERDATA));
  if (!instance || !instance->mListView) {
    return;
  }

  instance->mListView->Resize(aCx, aCy);
}

void
GlassWindow::OnSessionChange(HWND aHwnd, WPARAM aSessionChangeEvent)
{
  GlassWindow* instance = reinterpret_cast<GlassWindow*>(GetWindowLongPtrW(aHwnd, GWLP_USERDATA));
  if (!instance) {
    return;
  }

  instance->OnSessionChange(aSessionChangeEvent);
}

void
GlassWindow::OnSessionChange(WPARAM aSessionChangeEvent)
{
  switch (aSessionChangeEvent) {
    case WTS_CONSOLE_CONNECT:
    case WTS_REMOTE_CONNECT:
      // TODO ASK: The above two items should invoke a method for toggling
      //           double-buffering
    case WTS_SESSION_UNLOCK:
      OnSessionReconnect();
      ::InvalidateRect(mHwnd, nullptr, TRUE);
      Update();
      break;
    case WTS_CONSOLE_DISCONNECT:
    case WTS_REMOTE_DISCONNECT:
    case WTS_SESSION_LOCK:
      OnSessionDisconnect();
      break;
    default:
      break;
  }
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
    HANDLE_MSG(hwnd, WM_ERASEBKGND, OnEraseBackground);
    HANDLE_MSG(hwnd, WM_NCDESTROY, OnNcDestroy);
    HANDLE_MSG(hwnd, WM_PAINT, OnPaint);
    HANDLE_MSG(hwnd, WM_SIZE, OnSize);
    case WM_WTSSESSION_CHANGE:
      OnSessionChange(hwnd, wParam);
      return 0;
    default:
      break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool
GlassWindow::GetWindowRect(ScaledRect &aRect)
{
  /*
  if (!::GetClientRect(mHwnd, &aRect)) {
    return false;
  }
  aRect.top = mDpiScaler->ScaleY(aRect.top);
  aRect.bottom = mDpiScaler->ScaleY(aRect.bottom);
  aRect.left = mDpiScaler->ScaleX(aRect.left) + mMargins->GetFrameBorderXWidth(scaler);
  aRect.right = mDpiScaler->ScaleX(aRect.right) - mMargins->GetFrameBorderXWidth(scaler);
  return true;
  */
  if (!::GetWindowRect(mHwnd, &aRect)) {
    return false;
  }
  return true;
}

bool
GlassWindow::GetClientRect(ScaledRect &aRect)
{
  if (!::GetClientRect(mHwnd, &aRect)) {
    return false;
  }
  /*
  std::shared_ptr<DpiScaler const> scaler = GetDpiScaler();
  aRect.top = mDpiScaler->ScaleY(aRect.top) + mMargins->GetFrameBorderYWidth(scaler) + mMargins->GetCaptionWidth(scaler);
  aRect.bottom = mDpiScaler->ScaleY(aRect.bottom) - mMargins->GetFrameBorderYWidth(scaler);
  aRect.left = mDpiScaler->ScaleX(aRect.left) + mMargins->GetFrameBorderXWidth(scaler);
  aRect.right = mDpiScaler->ScaleX(aRect.right) - mMargins->GetFrameBorderXWidth(scaler);
  */
  return true;
}

bool
GlassWindow::ScreenToWindow(ScaledRect &aRect)
{
  SetLastError(ERROR_SUCCESS);
  if (!MapWindowPoints(NULL, mHwnd, (LPPOINT)&aRect, 2) &&
      GetLastError() != ERROR_SUCCESS) {
    odbs(L"MapWindowPoints failed");
    return false;
  }
  return true;
}

bool
GlassWindow::ScreenToClient(ScaledRect &aRect)
{
  bool result = ScreenToWindow(aRect);
  if (!result) {
    return result;
  }
  result = WindowToClient(aRect);
  return result;
}

} // namespace aspk

