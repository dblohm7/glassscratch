#ifndef __ASPK_UNIQUEHANDLE_H
#define __ASPK_UNIQUEHANDLE_H

#include <memory>
#include <type_traits>
#define MAKE_UNIQUE_HANDLE(name, newExpr, closeFnPtr) \
  std::unique_ptr<std::remove_pointer<decltype(newExpr)>::type, decltype(closeFnPtr)> name(newExpr, closeFnPtr)

#endif // __ASPK_UNIQUEHANDLE_H

