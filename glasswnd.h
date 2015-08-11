#ifndef __ASPK_GLASSWND_H
#define __ASPK_GLASSWND_H

#include <memory>
#include <string>

#include <windows.h>
#include <dwmapi.h>

namespace aspk {

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
  explicit DpiScaler(HWND hwnd);

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

class GlassWindow
{
public:
  GlassWindow(HINSTANCE aInstance, std::wstring const &aTitleText);
  ~GlassWindow();

  void Show(int aShow);
  void Update();

  inline operator HWND()
  {
    return mHwnd;
  }

private:
  // Member functions
  void OnCreate(HWND aHwnd);

private:
  // Instance Variables
  HWND                          mHwnd;
  std::unique_ptr<DpiScaler>    mDpiScaler;
  std::unique_ptr<GlassMargins> mMargins;

private:
  // Static Functions
  static void RefreshFrame(HWND hwnd);
  static void RefreshDwmInfo(HWND hwnd);
  static void OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL minimized);
  static void OnSize(HWND hwnd, UINT state, int cx, int cy);
  static LRESULT OnNcHitTest(HWND hwnd, int x, int y);
  static LRESULT NcWndProc(HWND aHwnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam, bool& aHandled);
  static BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpcs);
  static void OnDestroy(HWND hwnd);
  static void OnNcDestroy(HWND hwnd);
  static void DrawCaption(HWND hwnd, HDC hdc);
  static void OnPaint(HWND hwnd);
  static void OnDpiChanged(HWND hwnd, UINT newXDpi, UINT newYDpi, RECT const &newScaledWindowRect);
  static LRESULT CALLBACK WndProc(HWND aHwnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam);

private:
  // Constants
  static wchar_t const kClassName[];
  static wchar_t const kGlassWindowKey[];
};

} // namespace aspk

inline HWND operatorHWND(aspk::GlassWindow* aGlassWindow)
{
  return (HWND)static_cast<aspk::GlassWindow&>(*aGlassWindow);
}

#endif // __ASPK_GLASSWND_H

