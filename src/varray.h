
#ifndef VARRAY_H
#define VARRAY_H

#include <array>
#include <cstddef>
#include <utility>

using std::array;

template <typename T, size_t Nm>
class Varray {
  size_t Nc;
  array<T, Nm> _array;

  public:

  // Deliberately user-provided: a defaulted ctor would make `Varray<...> x{}`
  // zero-fill all Nm slots. Slots >= Nc are never read.
  Varray() : Nc(0) {}

  void
  push(T val) noexcept
  { if (Nc < Nm) _array[Nc++] = val; }

  // Insert at the front, shifting the rest right. When full, the last element
  // is dropped.
  void
  pushFront(T val) noexcept
  {
    static_assert(Nm >= 1, "pushFront needs at least one slot");
    if (Nc < Nm) ++Nc;
    for (size_t i = Nc - 1; i > 0; --i) _array[i] = _array[i - 1];
    _array[0] = val;
  }

  size_t
  size() const noexcept
  { return Nc; }

  size_t
  capacity() const noexcept
  { return Nm; }

  void
  clear() noexcept
  { Nc = 0; }

  void
  popBack() noexcept
  { if (Nc > 0) --Nc; }

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

  bool
  contains(const T& val) const noexcept
  {
    for (const T& elem : *this)
      if (elem == val) return true;
    return false;
  }
};

#endif
