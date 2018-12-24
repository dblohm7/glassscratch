#include "GlassWindowApp.h"

#include <windows.h>

#include <commctrl.h>
#include <gdiplus.h>

namespace aspk {

GlassWindowApp::GlassWindowApp()
  : mInitOk(false)
  , mGdiPlusToken(0)
{
  INITCOMMONCONTROLSEX icc = { sizeof(icc),
                               ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES };

  BOOL commCtrlOk = ::InitCommonControlsEx(&icc);
  if (!commCtrlOk) {
    return;
  }

  Gdiplus::GdiplusStartupInput startupInput;
  Gdiplus::Status gdiplusStatus = Gdiplus::GdiplusStartup(&mGdiPlusToken,
                                                          &startupInput,
                                                          nullptr);
  mInitOk = gdiplusStatus == Gdiplus::Ok;
}

GlassWindowApp::~GlassWindowApp()
{
  if (mInitOk) {
    Gdiplus::GdiplusShutdown(mGdiPlusToken);
  }
}

int
GlassWindowApp::Run()
{
  MSG msg;

  while (::GetMessage(&msg, nullptr, 0, 0)) {
    // We don't check for bRet == -1 here because that is only returned for
    // invalid parameters to GetMessage().
    // See https://blogs.msdn.microsoft.com/oldnewthing/20130322-00/?p=4873
    ::TranslateMessage(&msg);
    if (!::CallMsgFilter(&msg, 0)) {
      ::DispatchMessage(&msg);
    }
  }

  return 0;
}

} // namespace aspk

