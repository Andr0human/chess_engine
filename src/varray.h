
#ifndef VARRAY_H
#define VARRAY_H

#include <array>
#include <cstddef>

using std::array;

template <typename T, size_t Nm>
class Varray {
  size_t Nc;
  array<T, Nm> _array;
 
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

  void
  addKillerMove(T val)
  {
    _array[1] = _array[0];
    _array[0] = val;
  }

  bool
  search(T val)
  { return _array[0] == val || _array[1] == val; }
};

#endif
