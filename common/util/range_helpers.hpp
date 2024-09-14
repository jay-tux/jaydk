//
// Created by jay on 9/14/24.
//

#ifndef RANGE_HELPERS_HPP
#define RANGE_HELPERS_HPP

#include <ranges>

namespace jaydk {
template <std::ranges::range R, typename Filter, typename Map, typename T = std::ranges::range_value_t<R>>
requires(std::invocable<Filter, const T &> && std::convertible_to<std::invoke_result_t<Filter, const T &>, bool> && std::invocable<Map, const T &>)
constexpr auto filter_map(R &&range, Filter &&filter, Map &&map) {
  return range | std::views::filter(std::forward<Filter>(filter))
               | std::views::transform(std::forward<Map>(map));
}
}

#endif //RANGE_HELPERS_HPP
