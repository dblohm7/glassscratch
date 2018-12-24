#include "DpiScaler.h"

#include "odbs.h"
#include "UniqueHandle.h"

#include <ShellScalingApi.h>

namespace aspk {

typedef LRESULT (WINAPI* GetDpiForMonitorPtr)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*);

std::shared_ptr<DpiScaler const> DpiScaler::sNominal = std::make_shared<DpiScaler>(100, 100);

DpiScaler::DpiScaler(HWND aHwnd)
  : mXScalePercent(0)
  , mYScalePercent(0)
{
  HMONITOR monitor = ::MonitorFromWindow(aHwnd, MONITOR_DEFAULTTONEAREST);
  Init(monitor);
}

DpiScaler::DpiScaler()
  : mXScalePercent(0)
  , mYScalePercent(0)
{
  Init();
}

DpiScaler::DpiScaler(const int aXScalePercent,
                     const int aYScalePercent)
  : mXScalePercent(aXScalePercent)
  , mYScalePercent(aYScalePercent)
{
}

void
DpiScaler::Init(HMONITOR aMonitor)
{
  UINT x, y;
  // Try the Windows 8 monitor-aware way
  UniqueModule shcore(::LoadLibraryW(L"shcore.dll"));
  if (shcore) {
    GetDpiForMonitorPtr pGetDpiForMonitor = (GetDpiForMonitorPtr) GetProcAddress(shcore.get(), "GetDpiForMonitor");
    if (pGetDpiForMonitor) {
      if (SUCCEEDED(pGetDpiForMonitor(aMonitor, MDT_EFFECTIVE_DPI, &x, &y))) {
        Invalidate(x, y);
        return;
      }
    }
  }
  // Oh well, try the old system-wide way
  Init();
}

void
DpiScaler::Init()
{
  HDC dc = GetDC(NULL);
  if (!dc) {
    return;
  }
  UINT x = GetDeviceCaps(dc, LOGPIXELSX);
  UINT y = GetDeviceCaps(dc, LOGPIXELSY);
  ReleaseDC(NULL, dc);
  odbs(L"Device Caps says ", x * 100 / 96, L"%");
  Invalidate(x, y);
}

void
DpiScaler::Invalidate(int aNewXDpi, int aNewYDpi)
{
  mXScalePercent = (aNewXDpi * 100) / NOMINAL_DPI;
  mYScalePercent = (aNewYDpi * 100) / NOMINAL_DPI;
  odbs(L"DpiScaler::Invalidate X: ", mXScalePercent, L"%, Y: ", mYScalePercent, L"%");
}

} // namespace aspk

