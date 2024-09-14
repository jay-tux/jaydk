//
// Created by jay on 9/14/24.
//

#ifndef SEMANTIC_ERROR_HPP
#define SEMANTIC_ERROR_HPP

#include <string>
#include <stdexcept>
#include <utility>

#include "mangler.hpp"
#include "error_queue.hpp"

namespace jayc::sem {
class semantic_error final : public std::runtime_error {
public:
  semantic_error(const std::string &message, location loc) : std::runtime_error(message), loc{std::move(loc)} {}

  [[nodiscard]] const char *what() const noexcept override {
    return (std::string("Semantic error : ") + std::runtime_error::what() + " at " + to_string(loc)).c_str();
  }

  friend error_queue &operator<<(error_queue &q, const semantic_error &e) {
    return q << error{ e.loc, e.what() };
  }

  [[nodiscard]] const location &at() const { return loc; }

  static semantic_error redefine_ns(const std::string &path, const std::string &name, const location &at) {
    return {
      "Redefinition of namespace " + mangler::un_mangle_ns(path) + "::" + name + " ) as a different kind of symbol",
      at
    };
  }

  static semantic_error redefine_X(const std::string &path, const std::string &orig, const std::string &name, const location &orig_decl, const location &at) {
    return {
      "Redefinition of " + orig + " " + mangler::un_mangle_ns(path) + "::" + name + " (declared at " + to_string(orig_decl) +
      ") as a different kind of symbol",
      at
    };
  }

private:
  location loc;
};
}

#endif //SEMANTIC_ERROR_HPP
