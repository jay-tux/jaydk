//
// Created by jay on 9/2/24.
//

#include "lexer.hpp"

#include <optional>

#include "error.hpp"

using jayc::location;
using jayc::logger;
using namespace jayc::lexer;

token_stream<lexer<std::ifstream>> jayc::lexer::lex(const std::string &file) {
  std::ifstream strm{file};

  if(!strm.is_open() || !strm.good()) {
    logger << error{ location{file, 1, 0}, "Failed to open file `" + file + "`." };
    throw unrecoverable{};
  }

  return token_stream{lexer{file, std::move(strm)}};
}

token_stream<lexer<std::stringstream>> jayc::lexer::lex_source(const std::string &source) {
  return token_stream{lexer{"<inline script>", std::stringstream{source}}};
}