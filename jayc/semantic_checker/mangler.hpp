//
// Created by jay on 9/14/24.
//

#ifndef MANGLER_HPP
#define MANGLER_HPP

namespace jayc::sem {
struct mangler {
  /*
   * Rules for mangling:
   * -> namespaces: ns$ns$...$ns
   * -> nested type: type#type#...#type
   * -> member function: func@ns$ns$...$ns$type#type#...#type
   */

  static std::string mangle_ns(const std::string &base, const std::string &append) {
    return base + "$" + append;
  }
};
}

#endif //MANGLER_HPP
