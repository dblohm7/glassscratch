#ifndef __ASPK_DPISCALER_H
#define __ASPK_DPISCALER_H

#include <algorithm>
#include <array>
#include <initializer_list>
#include <memory>
#include <stdexcept>

#include <windows.h>

namespace aspk {

class DpiScaler
{
public:
  explicit DpiScaler(HWND aHwnd);
  DpiScaler();
  DpiScaler(const int aXScalePercent, const int aYScalePercent);

  int ScaleX(int aRaw) const
  {
    return (aRaw * mXScalePercent) / 100;
  }

  int ScaleY(int aRaw) const
  {
    return (aRaw * mYScalePercent) / 100;
  }

  bool operator!() const
  {
    return mXScalePercent == 0 || mYScalePercent == 0;
  }

  bool operator==(DpiScaler const &aOther) const
  {
    return mXScalePercent == aOther.mXScalePercent &&
           mYScalePercent == aOther.mYScalePercent;
  }

  int GetXScale() const { return mXScalePercent; }
  int GetYScale() const { return mYScalePercent; }

  void Invalidate(int aNewXDpi, int aNewYDpi);
  // Set to 100% scaling, effectively disabling
  inline void Disable()
  {
    Invalidate(NOMINAL_DPI, NOMINAL_DPI);
  }

  static std::shared_ptr<DpiScaler const> GetNominal()
  {
    return sNominal;
  }

private:
  void Init(HMONITOR aMonitor);
  void Init();

  int mXScalePercent;
  int mYScalePercent;

  static const int NOMINAL_DPI = 96;
  static std::shared_ptr<DpiScaler const> sNominal;
};

namespace detail {

enum Dimension
{
  eDimensionX = 0,
  eDimensionY,
  eDimensionCount
};

} // namespace detail

template <size_t N, detail::Dimension Dim = detail::eDimensionCount>
class ScaledValue
{
public:
  explicit ScaledValue(std::shared_ptr<DpiScaler const> aScaler)
    : mScaler(aScaler)
    , mRescale(true)
  {
    mValues.fill(0);
  }

  ScaledValue(std::shared_ptr<DpiScaler const> aScaler,
              RECT const &aRect)
    : mScaler(aScaler)
    , mRescale(true)
  {
    mValues = {aRect.left, aRect.top, aRect.right, aRect.bottom};
  }

  ScaledValue(std::shared_ptr<DpiScaler const> aScaler,
              std::initializer_list<int> const &aInitList)
    : mScaler(aScaler)
    , mRescale(true)
  {
    std::copy(aInitList.begin(), aInitList.end(), mValues.begin());
  }

  ScaledValue(std::shared_ptr<DpiScaler const> aScaler,
              std::array<int, N> &aInitData)
    : mScaler(aScaler)
    , mValues(aInitData)
    , mRescale(true)
  {
  }

  void SetNoRescale()
  {
    mRescale = false3;
  }

  int* ptr()
  {
    return mValues.data();
  }

  int const * ptr() const
  {
    return mValues.data();
  }

  inline std::array<int, N> const &
  GetValues() const
  {
    return mValues;
  }

  int&
  operator[](size_t aIndex)
  {
    return mValues.at(aIndex);
  }

  int const &
  operator[](size_t aIndex) const
  {
    return mValues.at(aIndex);
  }

  std::shared_ptr<DpiScaler const>
  GetScaler() const
  {
    return mScaler;
  }

  inline ScaledValue<N>
  ScaleTo(std::shared_ptr<DpiScaler const> const &aOtherScaler) const
  {
    if (!mRescale || mScaler == aOtherScaler) {
      return *this;
    }
    int ratios[] = {
      aOtherScaler->GetXScale() * 100 / mScaler->GetXScale(),
      aOtherScaler->GetYScale() * 100 / mScaler->GetYScale()
    };
    std::array<int, N> newValues(mValues);
    if (N == 4) {
      // Treat indexes 2 and 3 as exclusive
      RectExToIn(newValues);
    }
    int i = 0;
    std::for_each(newValues.begin(), newValues.end(), [&](int& a) {
      a *= ratios[i % 2];
      a = rounded_div(a, 100);
      ++i;
    });
    if (N == 4) {
      // Treat indexes 2 and 3 as exclusive
      RectInToEx(newValues);
    }
    return ScaledValue<N>(aOtherScaler, newValues);
  }

private:
  ScaledValue() = delete;
  static int rounded_div(int const aN, int const aD)
  {
    return ((aN < 0) ^ (aD < 0)) ?
             ((aN - aD / 2) / aD) :
             ((aN + aD / 2) / aD);
  }

  std::shared_ptr<DpiScaler const>  mScaler;
  std::array<int, N>                mValues;
  bool                              mRescale;
};

template<detail::Dimension Dim>
class ScaledValue<1, Dim>
{
public:
  explicit ScaledValue(std::shared_ptr<DpiScaler const> aScaler)
    : mScaler(aScaler)
    , mValue(0)
    , mRescale(true)
  {
  }

  ScaledValue(std::shared_ptr<DpiScaler const> aScaler, int const &aInit)
    : mScaler(aScaler)
    , mValue(aInit)
    , mRescale(true)
  {
  }

  void SetNoRescale()
  {
    mRescale = false;
  }

  inline int
  GetValue() const
  {
    return mValue;
  }

  std::shared_ptr<DpiScaler const>
  GetScaler() const
  {
    return mScaler;
  }

  inline ScaledValue<1, Dim>
  ScaleTo(std::shared_ptr<DpiScaler const> const &aOtherScaler) const
  {
    if (!mRescale || mScaler == aOtherScaler) {
      return *this;
    }
    int ratio = Dim == detail::eDimensionX ?
                  (aOtherScaler->GetXScale() * 100 / mScaler->GetXScale()) :
                  (aOtherScaler->GetYScale() * 100 / mScaler->GetYScale());
    // int newValue = mValue * ratio / 100;
    int newValue = rounded_div(mValue * ratio, 100);
    return ScaledValue<1, Dim>(aOtherScaler, newValue);
  }

  inline ScaledValue<1, Dim>
  operator+(ScaledValue<1, Dim> const &aRight) const
  {
    ScaledValue<1, Dim> rhs(aRight);
    if (mScaler != rhs.mScaler) {
      rhs = aRight.ScaleTo(mScaler);
    }
    return ScaledValue<1, Dim>(mScaler, mValue + rhs.mValue);
  }

  inline ScaledValue<1, Dim>
  operator-(ScaledValue<1, Dim> const &aRight) const
  {
    ScaledValue<1, Dim> rhs(aRight);
    if (mScaler != rhs.mScaler) {
      rhs = aRight.ScaleTo(mScaler);
    }
    return ScaledValue<1, Dim>(mScaler, mValue - rhs.mValue);
  }

private:
  ScaledValue() = delete;
  static int rounded_div(int const aN, int const aD)
  {
    return ((aN < 0) ^ (aD < 0)) ?
             ((aN - aD / 2) / aD) :
             ((aN + aD / 2) / aD);
  }

  std::shared_ptr<DpiScaler const>  mScaler;
  int                               mValue;
  bool                              mRescale;
};

typedef ScaledValue<1, detail::eDimensionX> ScaledDimensionX;
typedef ScaledValue<1, detail::eDimensionY> ScaledDimensionY;
typedef ScaledValue<2> ScaledPoint;
typedef ScaledValue<4> ScaledRect;

inline ScaledDimensionX
RectWidth(ScaledRect const &aRect)
{
  return ScaledDimensionX(aRect.GetScaler(), aRect[2] - aRect[0]);
}

inline ScaledDimensionY
RectHeight(ScaledRect const &aRect)
{
  return ScaledDimensionY(aRect.GetScaler(), aRect[3] - aRect[1]);
}

inline void
RectExToIn(std::array<int, 4>& aRect)
{
  aRect[2] -= 1;
  aRect[3] -= 1;
}

inline void
RectInToEx(std::array<int, 4>& aRect)
{
  aRect[2] += 1;
  aRect[3] += 1;
}

template<detail::Dimension Dim>
inline void
ScalarExToIn(ScaledValue<1, Dim> &aEx)
{
  aEx[0] -= 1;
}

template<detail::Dimension Dim>
inline void
ScalarInToEx(ScaledValue<1, Dim> &aIn)
{
  aIn[0] += 1;
}

} // namespace aspk

inline RECT*
operator&(aspk::ScaledRect &aRect)
{
  return reinterpret_cast<RECT*>(aRect.ptr());
}

inline RECT const *
operator&(aspk::ScaledRect const &aRect)
{
  return reinterpret_cast<RECT const *>(aRect.ptr());
}

#endif // __ASPK_DPISCALER_H

