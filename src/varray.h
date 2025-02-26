
#ifndef VARRAY_H
#define VARRAY_H

#include <array>
#include <climits>
#include <cstddef>

template <typename T, size_t Nm>
class Varray {
  size_t Nc;
  std::array<T, Nm> _array;

  public:

  Varray() : Nc(0) {}

  void add(T val) noexcept
  { if (Nc < Nm) _array[Nc++] = val; }

  size_t
  size() const noexcept
  { return Nc; }

  size_t
  capacity() const noexcept
  { return Nm; }

  void
  clear() noexcept
  { Nc = 0; }

  T&
  operator[](size_t index) noexcept
  { return _array[index]; }

  const T&
  operator[](size_t index) const noexcept
  { return _array[index]; }

  const T&
  back() const noexcept
  { return _array[Nc - 1]; }

  T*
  begin() noexcept
  { return _array.begin(); }

  T*
  end() noexcept
  { return _array.begin() + Nc; }

  const T*
  begin() const noexcept
  { return _array.begin(); }

  const T*
  end() const noexcept
  { return _array.begin() + Nc; }

  void addSorted(T val)
  {
    if (Nc >= Nm) return;

    size_t start = 0;

    while (start < Nc && _array[start] < val)
      start++;

    for (size_t i = Nc; i > start; --i)
      _array[i] = _array[i - 1];

    _array[start] = val;
    ++Nc;
  }

  bool searchSorted(T val)
  {
    if (Nc == 0) return false;
    if (val < _array.front()) return false;

    size_t s = 0, e = Nc - 1;
    while (s <= e)
    {
      size_t mid = s + (e - s) / 2;
      if (_array[mid] == val) return true;
      if (_array[mid] < val) s = mid + 1;
      else e = mid - 1;
    }

    return false;
  }
};

#endif
