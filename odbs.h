#ifndef __aspk_odbs_h
#define __aspk_odbs_h

#include <sstream>
#include <windows.h>

namespace aspk {

namespace detail {

template <typename T>
std::wostringstream& xodbs(std::wostringstream &oss, T&& aArg)
{
  oss << aArg;
  return oss;
}

inline std::wostringstream& operator<<(std::wostringstream& oss, RECT const &aRect)
{
  oss << L"Left: " << aRect.left
      << L", Top: " << aRect.top
      << L", Right: " << aRect.right
      << L", Bottom: " << aRect.bottom;
  return oss;
}

template <typename Car, typename ...Cdr>
std::wostringstream& xodbs(std::wostringstream &oss, Car&& aArg, Cdr&&... aRest)
{
  oss << aArg;
  return xodbs(oss, std::forward<Cdr>(aRest)...);
}

} // namespace detail

template <typename ...Args>
void
odbs(Args&&... aArgs)
{
  std::wostringstream oss;
  detail::xodbs(oss, std::forward<Args>(aArgs)...) << std::endl;
  OutputDebugStringW(oss.str().c_str());
}

} // namespace aspk

#endif // __aspk_odbs_h

