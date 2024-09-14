//
// Created by jay on 9/14/24.
//

#ifndef VARIANT_HELPERS_HPP
#define VARIANT_HELPERS_HPP

#include <variant>
#include <algorithm>

#include "range_helpers.hpp"

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

template <typename V> struct is_variant : std::false_type {};
template <typename ... Ts> struct is_variant<std::variant<Ts...>> : std::true_type {};

template <typename T, typename V>
concept is_alternative_for = is_variant<V>::value && requires(const T &t, V &v)
{
  { v = t };
};

template <typename X, std::ranges::range R>
constexpr auto filter_is_as(R &&range) {
  return filter_map(range, [](const auto &x) { return is<X>(x); }, [](const auto &x) { return as<X>(x); });
}
}

#endif //VARIANT_HELPERS_HPP
