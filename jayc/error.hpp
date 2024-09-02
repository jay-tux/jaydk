//
// Created by jay on 9/2/24.
//

#ifndef ERROR_HPP
#define ERROR_HPP

#include <stdexcept>

namespace jayc {
struct unrecoverable final : std::runtime_error {
  unrecoverable() : std::runtime_error("Unrecoverable error.") {}

  using runtime_error::runtime_error;
};
}

#endif //ERROR_HPP
