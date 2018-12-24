#ifndef __ASPK_LISTVIEW_H
#define __ASPK_LISTVIEW_H

#include <windows.h>

namespace aspk {

class GlassWindow;

class ListView
{
public:
  explicit ListView(GlassWindow& aParent);
  ~ListView();

  bool InsertColumn(const wchar_t* aText = nullptr);
  bool InsertCell(const wchar_t* aText);
  int GetNumColumns() const { return mNumColumns; }
  void Resize(int aCx, int aCy);

  explicit operator bool() const { return !!mHwnd; }
  operator HWND() { return mHwnd; }

private:
  void IncrementCurRowCol(const bool aHasNewline);
  void ResizeColumns();

private:
  HWND  mHwnd;
  int   mNextColIndex;
  int&  mNumColumns;  // Synonym of mNextColIndex
  int   mCurRow;
  int   mCurCol;
};

} // namespace aspk

#endif // __ASPK_LISTVIEW_H
