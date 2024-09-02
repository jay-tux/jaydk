//
// Created by jay on 9/2/24.
//

#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <fstream>
#include "token_stream.hpp"

namespace jayc::lexer {
class lexer {
public:
  explicit lexer(const std::string &file, std::ifstream &&strm)
      : input{std::move(strm)}, curr{file, 1, 0} {}

  token operator()();
  [[nodiscard]] inline bool eof() const { return input.eof() || !input.good(); }
  [[nodiscard]] constexpr location pos() const { return curr; }

private:
  std::ifstream input;
  location curr{};
};

token_stream<lexer> lex(const std::string &file);
}

#endif //LEXER_HPP
