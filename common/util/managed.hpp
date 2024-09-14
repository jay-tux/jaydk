//
// Created by jay on 9/14/24.
//

#ifndef MANAGED_HPP
#define MANAGED_HPP

#include <memory>

namespace jaydk {
template <typename T>
class managed {
public:
  constexpr managed() = default;
  constexpr explicit managed(T *ptr) : ptr{ptr} {}
  inline explicit managed(const T &t) : ptr{new T{t}} {}
  inline explicit managed(T &&t) : ptr{new T{std::forward<T &&>(t)}} {}
  inline managed(const managed<T> &other) { *this = other; }
  inline managed(managed<T> &&other) noexcept { *this = std::move(other); }

  template <typename ... Args>
  inline explicit managed(Args &&... args) : ptr{new T{std::forward<Args>(args)...}} {}

  inline managed &operator=(const managed<T> &other) {
    if(this == &other) return *this;
    delete ptr;
    if(other.ptr == nullptr) ptr = nullptr;
    else ptr = new T{*other.ptr};
    return *this;
  }

  inline managed &operator=(managed<T> &&other) noexcept {
    std::swap(ptr, other.ptr);
    return *this;
  }

  // ReSharper disable once CppNonExplicitConversionOperator
  constexpr operator bool() { return ptr != nullptr; } // NOLINT(*-explicit-constructor)
  constexpr T &operator*() { return *ptr; }
  constexpr const T &operator*() const { return *ptr; }
  constexpr T *operator->() { return ptr; }
  constexpr const T *operator->() const { return ptr; }

  inline ~managed() { delete ptr; }
private:
  T *ptr = nullptr;
};

template <typename T>
managed<T> alloc(const T &t) { return managed(t); }
template <typename T, typename ... Args>
managed<T> alloc(Args &&... args) { return managed<T>{std::forward<Args>(args)...}; }
}

#endif //MANAGED_HPP
