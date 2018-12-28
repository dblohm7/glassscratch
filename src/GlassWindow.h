#ifndef __ASPK_GLASSWND_H
#define __ASPK_GLASSWND_H

#include <map>
#include <memory>
#include <string>

#include <windows.h>
#include <dwmapi.h>

#include "DpiScaler.h"
#include "ListView.h"

namespace aspk {

inline UINT RectWidth(RECT const &r)
{
  return r.right - r.left;
}

inline UINT RectHeight(RECT const &r)
{
  return r.bottom - r.top;
}

class GlassWindow;

class GlassMargins
{
public:
  explicit GlassMargins(MARGINS const &aMargins,
                        std::shared_ptr<DpiScaler const> aDpiScaler,
                        std::shared_ptr<DpiScaler const> aNcDpiScaler)
    : mFrameBorderXWidth(aNcDpiScaler)
    , mFrameBorderYWidth(aNcDpiScaler)
    , mCaptionWidth(aNcDpiScaler)
    , mMargins(aMargins)
    , mUserMargins(aMargins)
    , mDpiScaler(aDpiScaler)
    , mNcDpiScaler(aNcDpiScaler)
  {
  }

  void Invalidate(GlassWindow* aGlassWindow);

  operator PMARGINS()
  {
    return &mMargins;
  }

  inline ScaledDimensionX
  GetFrameBorderXWidth(std::shared_ptr<DpiScaler const> &aScaler) const
  {
    return mFrameBorderXWidth.ScaleTo(aScaler);
  }

  inline ScaledDimensionY
  GetFrameBorderYWidth(std::shared_ptr<DpiScaler const> &aScaler) const
  {
    return mFrameBorderYWidth.ScaleTo(aScaler);
  }

  inline ScaledDimensionY
  GetCaptionWidth(std::shared_ptr<DpiScaler const> &aScaler) const
  {
    return mCaptionWidth.ScaleTo(aScaler);
  }

  bool HasMargins() const
  {
    return HasMargins(mMargins);
  }

  bool HasUserMargins() const
  {
    return HasMargins(mUserMargins);
  }

  inline PMARGINS GetMargins()
  {
    return &mMargins;
  }

private:
  static bool HasMargins(MARGINS const & aMargins);

private:
  ScaledDimensionX                  mFrameBorderXWidth;
  ScaledDimensionY                  mFrameBorderYWidth;
  ScaledDimensionY                  mCaptionWidth;
  MARGINS                           mMargins;
  MARGINS                           mUserMargins;
  std::shared_ptr<DpiScaler const>  mDpiScaler;
  std::shared_ptr<DpiScaler const>  mNcDpiScaler;
};

class GlassWindow
{
public:
  class Params
  {
  public:
    enum Flags
    {
      eQuitOnDestroy = 1,
      eSolidGlass = 2,
      eAlwaysOnTop = 4,
      eFixedSize = 8,
      eVisualDebug = 16,
      eDefaultFlags = eQuitOnDestroy | eSolidGlass
    };
    Params();
    inline void SetTitleText(std::wstring const &aTitleText) { mTitleText = aTitleText; }
    inline void SetSize(int aWidth, int aHeight) { mWidth = aWidth; mHeight = aHeight; }
    void SetFlags(unsigned int aFlags);
    inline void SetMargins(MARGINS &aMargins) { mMargins = aMargins; }

    inline bool QuitOnDestroy() const { return mQuitOnDestroy; }
    inline wchar_t const * GetTitleText() const { return mTitleText.c_str(); }
    inline DWORD GetStyleToggles() const { return mStyleToggles; }
    inline DWORD GetExStyleToggles() const { return mExStyleToggles; }
    inline int GetWidth() const { return mWidth; }
    inline int GetHeight() const { return mHeight; }
    inline MARGINS const & GetMargins() const { return mMargins; }
    inline HBRUSH GetBackgroundBrush() const { return mBackgroundBrush; }
    inline bool IsVisualDebugMode() const { return mVisualDebug; }

  private:
    std::wstring    mTitleText;
    int             mWidth;
    int             mHeight;
    DWORD           mStyleToggles;
    DWORD           mExStyleToggles;
    MARGINS         mMargins;
    HBRUSH          mBackgroundBrush;
    bool            mQuitOnDestroy;
    bool            mVisualDebug;
    DWORD           mCaptionFlags;
  };

  GlassWindow(HINSTANCE aInstance, std::wstring const &aTitleText);
  GlassWindow(HINSTANCE aInstance, Params const &aParams);
  ~GlassWindow();

  void Show(int aShow);
  void Update();

  explicit operator bool() const
  {
    return !!mHwnd;
  }

  inline operator HWND()
  {
    return mHwnd;
  }

  GlassMargins const & GetMargins() const
  {
    return *mMargins;
  }

  std::shared_ptr<DpiScaler const> GetDpiScaler() const
  {
    return mDpiScaler;
  }

  std::shared_ptr<DpiScaler const> GetNcDpiScaler() const
  {
    return mNcDpiScaler;
  }

  bool GetWindowRect(ScaledRect &aRect);
  bool GetClientRect(ScaledRect &aRect);
  bool ScreenToWindow(ScaledRect &aRect);
  bool ScreenToClient(ScaledRect &aRect);
  bool ConvertMouseCoords(LPARAM &aParam);

  enum FrameInfoFlags : unsigned int
  {
    eFIDefault = 0,
    eFINoRescale = 1,
    eFIIncludeSizeFrame = 2,
  };
  ScaledRect GetFrameInfo(unsigned int aFlags);

  bool WindowToClient(ScaledRect &aRect);

  HICON GetCaptionIcon() const;

  void DrawDebugRect(HDC aDc, RECT const &aRect, COLORREF aColor);

  inline bool IsMaximized() const
  {
    return !!::IsZoomed(mHwnd);
  }

  inline bool IsEnabled() const
  {
    return !!::IsWindowEnabled(mHwnd);
  }

  inline bool IsActive() const
  {
    return ::GetActiveWindow() == mHwnd;
  }

  inline bool IsVisible() const
  {
    return !!::IsWindowVisible(mHwnd);
  }

  inline bool IsDebugMode() const
  {
    return mDebug;
  }

  inline HINSTANCE GetInstance() const
  {
    return mInstance;
  }

  static inline bool IsRemote()
  {
    return ::GetSystemMetrics(SM_REMOTESESSION);
  }

  void Printf(const wchar_t* aFmt, ...);

protected:
  virtual void OnPaint(HDC aDc);
  virtual void OnDestroy();
  virtual void OnSessionChange(WPARAM aSessionChangeEvent);

protected:
  // Types

protected:
  // Instance Variables
  HINSTANCE                       mInstance;
  HWND                            mHwnd;
  BOOL                            mWTSRegistered;
  std::shared_ptr<DpiScaler>      mDpiScaler;
  std::shared_ptr<DpiScaler>      mNcDpiScaler;
  std::unique_ptr<GlassMargins>   mMargins;
  bool                            mQuitOnDestroy;
  bool                            mDebug;

  std::unique_ptr<wchar_t[]>      mPrintfBuf;
  int                             mPrintfBufLen;
  std::unique_ptr<ListView>       mListView;

private:
  // Member functions
  void Init(std::wstring const &aTitleText, DWORD aStyleToggles,
            DWORD aExStyleToggles, int aWidth, int aHeight,
            MARGINS const & aMargins, HBRUSH aBackgroundBrush);
  void OnCreate(HWND aHwnd, MARGINS const & aMargins);
  void OnErase(HDC aDc, RECT const &aRect);
  void OnThemeChanged();
  void GetClientRectInset(RECT &aRect);
  void MaybeCreateListView(const size_t aNumCols);

private:
  // Static Functions
  static void RefreshFrame(HWND hwnd);
  static void RefreshDwmInfo(HWND hwnd);
  static void OnActivate(HWND hwnd, UINT state, HWND hwndActDeact, BOOL minimized);
  static BOOL OnNcActivate(HWND hwnd, BOOL activate, HWND other, BOOL minimized);
  static void OnEnable(HWND hwnd, BOOL enable);
  static void OnSessionChange(HWND hwnd, WPARAM aSessionChangeEvent);
  static void OnSize(HWND hwnd, UINT state, int cx, int cy);
  static void OnShowWindow(HWND hwnd, BOOL show, UINT status);
  static LRESULT OnNcHitTest(HWND hwnd, int x, int y);
  static LRESULT NcWndProc(HWND aHwnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam, bool& aHandled);
  static BOOL OnCreate(HWND hwnd, LPCREATESTRUCT lpcs);
  static void OnDestroy(HWND hwnd);
  static void OnDpiChanged(HWND hwnd, UINT newXDpi, UINT newYDpi, RECT const &newScaledWindowRect);
  static BOOL OnEraseBackground(HWND aHwnd, HDC aDc);
  static void OnNcDestroy(HWND hwnd);
  static void OnPaint(HWND hwnd);
  static LRESULT CALLBACK WndProc(HWND aHwnd, UINT aMsg, WPARAM aWParam, LPARAM aLParam);

private:
  // Constants
  static wchar_t const kClassName[];
  static wchar_t const kGlassWindowKey[];
};

} // namespace aspk

#endif // __ASPK_GLASSWND_H

