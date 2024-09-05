//
// Created by jay on 9/2/24.
//

#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include "token_stream.hpp"

namespace jayc::lexer {
token read_token(std::istream &strm, location &curr);

template <typename Stream> requires(std::derived_from<Stream, std::istream>)
class lexer {
public:
  explicit lexer(const std::string &file, Stream &&strm)
      : input{std::move(strm)}, curr{file, 1, 1} {}

  token operator()() { return read_token(input, curr); }

  [[nodiscard]] inline bool eof() const { return input.eof() || !input.good(); }
  [[nodiscard]] constexpr location pos() const { return curr; }

private:
  Stream input;
  location curr{};
};

token_stream<lexer<std::ifstream>> lex(const std::string &file);
token_stream<lexer<std::stringstream>> lex_source(const std::string &source);
}

#endif //LEXER_HPP
