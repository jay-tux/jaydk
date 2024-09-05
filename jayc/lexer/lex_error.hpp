//
// Created by jay on 9/4/24.
//

#ifndef LEX_ERROR_HPP
#define LEX_ERROR_HPP

#include <string>

#include "error_queue.hpp"

namespace jayc::lexer {
inline error incomplete_float(const location &pos) {
  return error{ pos, "Incomplete floating-point literal." };
}

inline error incomplete_hex(const location &pos) {
  return error{ pos, "Incomplete hexadecimal integer literal." };
}

inline error unterminated_char(const location &pos) {
  return error{ pos, "Unterminated character literal." };
}

inline error zero_width_char(const location &pos) {
  return error{ pos, "Zero-width character literal." };
}

inline error invalid_escape_sequence_char(const location &pos) {
  return error{ pos, "Invalid escape sequence in character literal." };
}

inline error char_literal_too_wide(const location &pos) {
  return error{ pos, "Character literal too wide." };
}

inline error unterminated_string(const location &pos) {
  return error{ pos, "Unterminated string literal." };
}

inline error invalid_escape_sequence_str(const location &pos) {
  return error{ pos, "Invalid escape sequence in string literal." };
}

inline error newline_in_string(const location &pos) {
  return error{ pos, "Newline in string literal." };
}

inline error unterminated_block_comment(const location &pos) {
  return error{ pos, "Unterminated block comment." };
}

inline error invalid_token(const char tok, const location &pos) {
  return error{ pos, std::string{"Invalid token`"} + tok + "`." };
}
}

#endif //LEX_ERROR_HPP
