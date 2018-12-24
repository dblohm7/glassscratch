#ifndef __aspk_PaintContext_h
#define __aspk_PaintContext_h

#include <windows.h>

namespace aspk {

class PaintContext
{
public:
  explicit PaintContext(HWND aHwnd)
    : mHwnd(aHwnd)
    , mDC(NULL)
  {
    mDC = ::BeginPaint(aHwnd, &mPaintStruct);
  }

  ~PaintContext()
  {
    ::EndPaint(mHwnd, &mPaintStruct);
  }

  inline HDC
  GetDC() const
  {
    return mDC;
  }

  inline PAINTSTRUCT &
  GetPaintStruct()
  {
    return mPaintStruct;
  }

private:
  PaintContext(PaintContext const &) = delete;
  PaintContext& operator=(PaintContext const &) = delete;

private:
  HWND        mHwnd;
  PAINTSTRUCT mPaintStruct;
  HDC         mDC;
};

} // namespace aspk

#endif // __aspk_PaintContext_h

