//
// Created by jay on 9/14/24.
//

#ifndef REF_HELPERS_HPP
#define REF_HELPERS_HPP

#include <functional>
#include <optional>

namespace jaydk {
template <typename T>
using opt_ref = std::optional<std::reference_wrapper<T>>;

template <typename T>
constexpr opt_ref<T> mk_opt_ref(T &t) { return std::optional{std::reference_wrapper{t}}; }

struct ref{};

template <typename X>
std::reference_wrapper<X> operator|(X &x, ref) { return std::reference_wrapper{x}; }
}

#endif //REF_HELPERS_HPP
