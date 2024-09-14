//
// Created by jay on 9/14/24.
//

#ifndef MANGLER_HPP
#define MANGLER_HPP

#include <string>

#include "util/string_helpers.hpp"

namespace jayc::sem {
struct mangler {
  /*
   * Rules for mangling:
   * -> namespaces: ns$ns$...$ns
   * -> types: ns$ns$...$ns%type
   * -> nested type: ns$ns$...$ns%type#type#...#type
   * -> contracts: ns$ns$...$ns/contract
   * -> free function: func@ns$ns$...$ns
   * -> member function: func@ns$ns$...$ns%type#type#...#type
   * -> global: ns$ns$...$ns&global
   */

  static std::string mangle_ns(const std::string &base, const std::string &append) {
    return base + "$" + append;
  }

  static std::string un_mangle_ns(const std::string &ns) {
    return jaydk::replace_each(ns, "$", "::");
  }

  static std::string mangle_initial_type(const std::string &base, const std::string &append) {
    return base + "%" + append;
  }

  static std::string mangle_nested_type(const std::string &base, const std::string &append) {
    return base + "#" + append;
  }

  static std::string mangle_contract(const std::string &base, const std::string &append) {
    return base + "/" + append;
  }

  static std::string mangle_function(const std::string &base, const std::string &append) {
    return append + "@" + base;
  }

  static std::string mangle_global(const std::string &base, const std::string &append) {
    return base + "&" + append;
  }
};
}

#endif //MANGLER_HPP
