//
// Created by jay on 9/5/24.
//

#ifndef PARSE_ERROR_HPP
#define PARSE_ERROR_HPP

#include <string>

#include "lexer/token_output.hpp"
#include "error_queue.hpp"

namespace jayc::parser {
  inline error expect(const std::string &what, const lexer::token &got) {
    return error{ got.pos, "Expected " + what + ", got " + token_type(got) };
  }

  inline error expect_decl(const lexer::token &got) {
    return expect("declaration (`namespace`, `struct`, `fun`, or `var`)", got);
  }

  inline error expect_identifier(const lexer::token &got) {
    return expect("identifier", got);
  }
}

#endif //PARSE_ERROR_HPP
