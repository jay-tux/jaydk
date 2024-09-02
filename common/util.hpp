//
// Created by jay on 9/2/24.
//

#ifndef UTIL_HPP
#define UTIL_HPP

#include <variant>
#include <algorithm>

namespace jaydk {
template <typename X, typename ... Ts>
constexpr bool is(const std::variant<Ts...> &v) {
  return std::holds_alternative<X>(v);
}

template <typename X, std::ranges::range R>
constexpr bool count_of(R &&range) {
  return std::ranges::count_if(std::forward<R &&>(range), [](const auto &v) { return is<X>(v); });
}
}

#endif //UTIL_HPP
