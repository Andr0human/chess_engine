
#ifndef VARRAY_H
#define VARRAY_H

#include <array>

template <typename T, std::size_t Nm>
class Varray {
  std::size_t Nc;
  std::array<T, Nm> _array;
 
  public:

  Varray() : Nc(0) {}

  void add(T val) noexcept
  { if (Nc < Nm) _array[Nc++] = val; }

  std::size_t
  size() const noexcept
  { return Nc; }

  std::size_t
  capacity() const noexcept
  { return Nm; }

  void
  clear() noexcept
  { Nc = 0; }

  T&
  operator[](std::size_t index) noexcept
  { return _array[index]; }

  const T&
  operator[](std::size_t index) const noexcept
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
};

#endif
