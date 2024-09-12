//
// Created by jay on 9/2/24.
//

#ifndef UTIL_HPP
#define UTIL_HPP

#include <memory>
#include <variant>
#include <algorithm>
#include <optional>

namespace jaydk {
template <typename X, typename ... Ts>
constexpr bool is(const std::variant<Ts...> &v) {
  return std::holds_alternative<X>(v);
}

template <typename X, typename ... Ts>
constexpr X &as(std::variant<Ts...> &v) { return std::get<X>(v); }
template <typename X, typename ... Ts>
constexpr const X &as(const std::variant<Ts...> &v) { return std::get<X>(v); }

template <typename X, std::ranges::range R>
constexpr bool count_of(R &&range) {
  return std::ranges::count_if(std::forward<R &&>(range), [](const auto &v) { return is<X>(v); });
}

template <typename T>
class managed {
public:
  constexpr managed() = default;
  constexpr explicit managed(T *ptr) : ptr{ptr} {}
  inline explicit managed(const T &t) : ptr{new T{t}} {}
  inline explicit managed(T &&t) : ptr{new T{std::forward<T &&>(t)}} {}
  inline managed(const managed<T> &other) { *this = other; }
  inline managed(managed<T> &&other) noexcept { *this = std::move(other); }

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

template <typename V> struct is_variant : std::false_type {};
template <typename ... Ts> struct is_variant<std::variant<Ts...>> : std::true_type {};

template <typename T, typename V>
concept is_alternative_for = is_variant<V>::value && requires(const T &t, V &v)
{
  { v = t };
};

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
using opt_ref = std::optional<std::reference_wrapper<T>>;

template <typename T>
constexpr opt_ref<T> mk_opt_ref(T &t) { return std::optional{std::reference_wrapper{t}}; }

template <typename T>
class heap_opt {
public:
  constexpr heap_opt() = default;
  inline heap_opt(const T &t) /*requires(std::copy_constructible<T>)*/ : ptr{new T{t}} {}
  inline heap_opt(T &&t) /*requires(std::move_constructible<T>)*/ : ptr{new T{std::forward<T &&>(t)}} {}
  constexpr heap_opt(std::nullopt_t) {}
  inline heap_opt(const heap_opt &other) /*requires(std::copy_constructible<T>)*/ { *this = other; }
  inline heap_opt(heap_opt &&other) noexcept /*requires(std::move_constructible<T>)*/ { *this = std::move(other); }

  inline heap_opt &operator=(const heap_opt<T> &other) /*requires(std::copy_constructible<T>)*/ {
    if(this == &other) return *this;
    delete ptr;
    if(other.ptr == nullptr) ptr = nullptr;
    else ptr = new T{*other.ptr};
    return *this;
  }

  inline heap_opt &operator=(heap_opt<T> &&other) noexcept /*requires(std::move_constructible<T>)*/ {
    std::swap(ptr, other.ptr);
    return *this;
  }

  inline heap_opt &operator=(const T &other) /*requires(std::copy_constructible<T>)*/ {
    delete ptr;
    ptr = new T{other};
    return *this;
  }

  inline heap_opt &operator=(T &&other) /*requires(std::move_constructible<T>)*/ {
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

#endif //UTIL_HPP
