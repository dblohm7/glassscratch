#include "ListView.h"

#include "GlassWindow.h"

#include <commctrl.h>

static const int kMinColWidth = 100;

namespace aspk {

ListView::ListView(GlassWindow& aParent)
  : mHwnd(nullptr)
  , mNextColIndex(0)
  , mNumColumns(mNextColIndex)
  , mCurRow(0)
  , mCurCol(0)
{
  ScaledRect clientRect(aParent.GetDpiScaler());
  if (!aParent.GetClientRect(clientRect)) {
    return;
  }

  DWORD winStyles = WS_CHILD | WS_BORDER | WS_VISIBLE;
  DWORD lvStyles = LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_NOSORTHEADER;
  DWORD lvExStyles = LVS_EX_FULLROWSELECT |
                     LVS_EX_GRIDLINES;

  if (!aParent.IsRemote()) {
    lvExStyles |= LVS_EX_DOUBLEBUFFER;
  }

  RECT* rc = &clientRect;
  mHwnd = ::CreateWindowExW(0, WC_LISTVIEW, L"",
                            winStyles | lvStyles, 0, 0,
                            rc->right - rc->left, rc->bottom - rc->top, aParent,
                            nullptr, aParent.GetInstance(), nullptr);
  if (!mHwnd) {
    return;
  }

  ::SetWindowTheme(mHwnd, L"Explorer", nullptr);

  ListView_SetExtendedListViewStyle(mHwnd, lvExStyles);
}

ListView::~ListView()
{
  if (mHwnd) {
    ::DestroyWindow(mHwnd);
  }
}

bool
ListView::InsertColumn(const wchar_t* aText)
{
  LVCOLUMNW colDesc = { LVCF_FMT | LVCF_MINWIDTH, LVCFMT_LEFT };
  if (aText) {
    colDesc.mask |= LVCF_TEXT;
    colDesc.pszText = const_cast<LPWSTR>(aText);

    LONG_PTR style = ::GetWindowLongPtr(mHwnd, GWL_STYLE);
    ::SetWindowLongPtr(mHwnd, GWL_STYLE, style & ~LVS_NOCOLUMNHEADER);
  }

  colDesc.cxMin = kMinColWidth;

  int newIndex = ListView_InsertColumn(mHwnd, mNextColIndex, &colDesc);
  if (newIndex < 0) {
    return false;
  }

  mNextColIndex = newIndex + 1;
  return true;
}

void
ListView::IncrementCurRowCol(const bool aHasNewline)
{
  if (!mNumColumns) {
    return;
  }

  if (aHasNewline) {
    mCurCol = 0;
  } else {
    mCurCol = (mCurCol + 1) % mNumColumns;
  }

  if (!mCurCol) {
    ++mCurRow;
  }
}

void
ListView::ResizeColumns()
{
  if (!mNumColumns) {
    return;
  }

  RECT rect;
  if (!::GetWindowRect(mHwnd, &rect)) {
    return;
  }

  int colWidth = (rect.right - rect.left) / mNumColumns;

  for (int i = 0; i < mNumColumns; ++i) {
    ListView_SetColumnWidth(mHwnd, i, colWidth);
  }
}

bool
ListView::InsertCell(const wchar_t* aText)
{
  std::wstring strText;
  if (aText) {
    strText = aText;
  }

  const bool hasNewline = strText.empty() ? false : strText.back() == L'\n';
  if (hasNewline) {
    strText.pop_back();
  }

  LVITEMW lvItem = {};
  lvItem.iItem = mCurRow;

  if (!strText.empty()) {
    lvItem.mask |= LVIF_TEXT;
    lvItem.pszText = const_cast<LPWSTR>(strText.c_str());
  }

  if (!mCurCol) {
    int newIndex = ListView_InsertItem(mHwnd, &lvItem);
    if (newIndex < 0) {
      return false;
    }

    if (!newIndex) {
      // We inserted our first row. Let's set column sizes
      ResizeColumns();
    }

    mCurRow = newIndex;
    IncrementCurRowCol(hasNewline);
    return true;
  }

  lvItem.iSubItem = mCurCol;

  BOOL ok = ListView_SetItem(mHwnd, &lvItem);
  if (!ok) {
    return false;
  }

  IncrementCurRowCol(hasNewline);
  return true;
}

void
ListView::Resize(int aCx, int aCy)
{
  ::MoveWindow(mHwnd, 0, 0, aCx, aCy, TRUE);
  ResizeColumns();
}

} // namespace aspk

