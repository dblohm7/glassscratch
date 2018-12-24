#ifndef __ASPK_UNIQUEHANDLE_H
#define __ASPK_UNIQUEHANDLE_H

#include <memory>
#include <type_traits>

#include <windows.h>
#include <uxtheme.h>

namespace aspk {
namespace detail {

struct KernelHandleDeleter
{
  using pointer = HANDLE;

  void operator()(pointer aPtr)
  {
    ::CloseHandle(aPtr);
  }
};

struct GdiHandleDeleter
{
  using pointer = HGDIOBJ;

  void operator()(pointer aPtr)
  {
    ::DeleteObject(aPtr);
  }
};

struct ThemeHandleDeleter
{
  using pointer = HTHEME;

  void operator()(pointer aPtr)
  {
    ::CloseThemeData(aPtr);
  }
};

struct ModuleDeleter
{
  using pointer = HMODULE;

  void operator()(pointer aPtr)
  {
    ::FreeLibrary(aPtr);
  }
};

} // namespace detail

#define DECLARE_UNIQUE_HANDLE_TYPE(name, handle_type, deleter_type) \
  using name = std::unique_ptr<std::remove_pointer<handle_type>::type, detail::deleter_type>

DECLARE_UNIQUE_HANDLE_TYPE(UniqueKernelHandle, HANDLE, KernelHandleDeleter);
DECLARE_UNIQUE_HANDLE_TYPE(UniqueGdiHandle, HGDIOBJ, GdiHandleDeleter);
DECLARE_UNIQUE_HANDLE_TYPE(UniqueThemeHandle, HTHEME, ThemeHandleDeleter);
DECLARE_UNIQUE_HANDLE_TYPE(UniqueModule, HMODULE, ModuleDeleter);

#undef DECLARE_UNIQUE_HANLDLE_TYPE

} // namespace aspk

#endif // __ASPK_UNIQUEHANDLE_H

