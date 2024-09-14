//
// Created by jay on 9/14/24.
//

#ifndef OPTIONAL_HELPERS_HPP
#define OPTIONAL_HELPERS_HPP

#include <optional>

namespace jaydk {
template <typename F, typename X> requires(std::invocable<F, X>)
constexpr std::optional<std::invoke_result_t<F, X>> operator|(const std::optional<X> &opt, F &&f) {
  if(opt.has_value()) return std::optional{f(opt.value())};
  return std::nullopt;
}

namespace internal_ {
template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<std::optional<T>> : std::true_type {};
}

template <typename F, typename X> requires(std::invocable<F, const X &> && internal_::is_optional<std::invoke_result_t<F, const X &>>::value)
constexpr std::invoke_result_t<F, const X &> operator>>(const std::optional<X> &opt, F &&f) {
  if(opt.has_value()) return f(*opt);
  return std::nullopt;
}

struct maybe{};

template <typename X>
constexpr std::optional<X> operator|(const X &x, maybe) { return x; }

template <typename X>
constexpr std::optional<X> operator||(const std::optional<X> &orig, const std::optional<X> &other) {
  if(orig.has_value()) return orig;
  return other;
}

template <typename X, typename Y>
std::optional<std::pair<X, Y>> merge(const std::optional<X> &x, const std::optional<Y> &y) {
  if(!x.has_value()) return std::nullopt;
  if(!y.has_value()) return std::nullopt;
  return std::pair{*x, *y};
}

template <typename T>
class heap_opt {
public:
  constexpr heap_opt() = default;
  inline heap_opt(const T &t) : ptr{new T{t}} {}
  inline heap_opt(T &&t) : ptr{new T{std::forward<T &&>(t)}} {}
  constexpr heap_opt(std::nullopt_t) {}
  inline heap_opt(const heap_opt &other) { *this = other; }
  inline heap_opt(heap_opt &&other) noexcept { *this = std::move(other); }

  inline heap_opt &operator=(const heap_opt<T> &other) {
    if(this == &other) return *this;
    delete ptr;
    if(other.ptr == nullptr) ptr = nullptr;
    else ptr = new T{*other.ptr};
    return *this;
  }

  inline heap_opt &operator=(heap_opt<T> &&other) noexcept {
    std::swap(ptr, other.ptr);
    return *this;
  }

  inline heap_opt &operator=(const T &other) {
    delete ptr;
    ptr = new T{other};
    return *this;
  }

  inline heap_opt &operator=(T &&other) {
    delete ptr;
    ptr = new T{std::forward<T &&>(other)};
    return *this;
  }

  inline heap_opt &operator=(std::nullopt_t) {
    delete ptr;
    ptr = nullptr;
    return *this;
  }

  [[nodiscard]] constexpr bool has_value() const { return ptr != nullptr; }
  constexpr T &value() { return *ptr; }
  constexpr const T &value() const { return *ptr; }

  constexpr operator bool() const { return ptr != nullptr; }
  constexpr T &operator*() { return *ptr; }
  constexpr const T &operator*() const { return *ptr; }
  constexpr T *operator->() { return ptr; }
  constexpr const T *operator->() const { return ptr; }

  constexpr bool operator==(std::nullopt_t) const { return ptr == nullptr; }
  constexpr bool operator==(const heap_opt &other) const {
    if(ptr == nullptr) return other.ptr == nullptr;
    if(other.ptr == nullptr) return false;
    return *ptr == *other.ptr;
  }

  ~heap_opt() { delete ptr; }

private:
  T *ptr = nullptr;
};
}

#endif //OPTIONAL_HELPERS_HPP
