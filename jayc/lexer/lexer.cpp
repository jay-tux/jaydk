//
// Created by jay on 9/2/24.
//

#include "lexer.hpp"

#include <optional>

#include "error.hpp"

using jayc::location;
using jayc::logger;
using namespace jayc::lexer;

token next_token(std::ifstream &strm, location &curr);

location move_forward(location &old, const int lines, const int cols) {
  location copy = old;
  old.line += lines;
  old.col += cols;
  return copy;
}

token lexer::operator()() { return next_token(input, curr); }

token_stream<lexer> jayc::lexer::lex(const std::string &file) {
  std::ifstream strm{file};

  if(!strm.is_open() || !strm.good()) {
    logger << error{ location{file, 1, 0}, "Failed to open file `" + file + "`." };
    throw unrecoverable{};
  }

  return token_stream{lexer{file, std::move(strm)}};
}
