//
// Created by jay on 9/14/24.
//

#ifndef STRING_HELPERS_HPP
#define STRING_HELPERS_HPP

#include <string>
#include <sstream>

namespace jaydk {
inline std::string replace_each(const std::string &base, const std::string &from, const std::string &to) {
  std::stringstream strm;
  size_t pos = 0;
  while (pos < base.size()) {
    const auto next = base.find(from, pos);
    if (next == std::string::npos) {
      strm << base.substr(pos);
      break;
    }
    strm << base.substr(pos, next - pos) << to;
    pos = next + from.size();
  }

  return strm.str();
}
}

#endif //STRING_HELPERS_HPP
