//
// Created by jay on 9/5/24.
//

#ifndef PARSE_ERROR_HPP
#define PARSE_ERROR_HPP

#include <string>

#include "error_queue.hpp"
#include "lexer/token_output.hpp"

namespace jayc::parser {
inline error expect(const std::string &what, const lexer::token &got) {
  return error{got.pos, "Expected " + what + ", got " + token_type(got)};
}

inline error expect_decl(const lexer::token &got) { return expect("declaration", got); }

inline error expect_identifier(const lexer::token &got) { return expect("identifier", got); }

inline error untyped_field(const location &pos) { return error{pos, "Untyped field in struct is not allowed."}; }

inline error ns_in_struct(const location &pos) {
  return error{pos, "Declaration of namespace in struct is not allowed."};
}

inline error extension_in_struct(const location &pos) {
  return error{pos, "Declaration of extension function in struct is not allowed."};
}

inline error else_no_if(const location &pos) { return error{pos, "Else without preceding if-statement."}; }
} // namespace jayc::parser


#endif // PARSE_ERROR_HPP
