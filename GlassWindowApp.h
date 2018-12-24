#ifndef __ASPK_GLASSWINDOWAPP_H
#define __ASPK_GLASSWINDOWAPP_H

#include <windows.h>

namespace aspk {

class GlassWindowApp
{
public:
  GlassWindowApp();
  ~GlassWindowApp();

  explicit operator bool() { return mInitOk; }
  int Run();

private:
  GlassWindowApp(const GlassWindowApp&) = delete;
  GlassWindowApp(GlassWindowApp&&) = delete;
  GlassWindowApp& operator=(const GlassWindowApp&) = delete;
  GlassWindowApp& operator=(GlassWindowApp&&) = delete;

private:
  bool      mInitOk;
  ULONG_PTR mGdiPlusToken;
};

} // namespace aspk

#endif // __ASPK_GLASSWINDOWAPP_H

