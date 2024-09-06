//
// Created by jay on 9/4/24.
//

#include <iostream>
#include <unordered_map>
#include <utility>

#include "token_stream.hpp"
#include "lexer.hpp"
#include "lex_error.hpp"

using namespace jayc;
using namespace jayc::lexer;

namespace {
const std::unordered_map<std::string, keyword> keywords = {
  {"fun", keyword::FUN}, {"var", keyword::VAR}, {"if", keyword::IF},
  {"else", keyword::ELSE}, {"for", keyword::FOR}, {"while", keyword::WHILE},
  {"do", keyword::DO}, {"return", keyword::RETURN}, {"break", keyword::BREAK},
  {"continue", keyword::CONTINUE}, {"namespace", keyword::NAMESPACE},
  {"struct", keyword::STRUCT}
};

inline char peek(std::istream &strm) {
  return static_cast<char>(strm.peek());
}

constexpr bool is_whitespace(const char c) {
  return c == ' ' || c == '\n' || c == '\t' || c == '\v';
}

inline void extract(std::istream &strm, location &curr, const bool is_newline = false) {
  strm.get();
  curr.col++;
  if(is_newline) {
    curr.col = 1;
    curr.line++;
  }
}

inline uint64_t read_hex(std::istream &strm, location &curr) {
  uint64_t res = 0;
  while(!strm.eof()) {
    switch(peek(strm)) {
      case '0': res = res * 16 + 0; break;
      case '1': res = res * 16 + 1; break;
      case '2': res = res * 16 + 2; break;
      case '3': res = res * 16 + 3; break;
      case '4': res = res * 16 + 4; break;
      case '5': res = res * 16 + 5; break;
      case '6': res = res * 16 + 6; break;
      case '7': res = res * 16 + 7; break;
      case '8': res = res * 16 + 8; break;
      case '9': res = res * 16 + 9; break;
      case 'a': case 'A': res = res * 16 + 10; break;
      case 'b': case 'B': res = res * 16 + 11; break;
      case 'c': case 'C': res = res * 16 + 12; break;
      case 'd': case 'D': res = res * 16 + 13; break;
      case 'e': case 'E': res = res * 16 + 14; break;
      case 'f': case 'F': res = res * 16 + 15; break;
      default: return res;
    }
    extract(strm, curr);
  }

  return res;
}

inline token read_decimal(std::istream &strm, const location &start, location &curr, const std::string &buf) {
  std::string buffer = buf;
  char next = peek(strm);
  while(isdigit(next)) {
    extract(strm, curr);
    buffer += next;
    next = peek(strm);
  }

  if(next == '.') {
    // -> float or double
    extract(strm, curr);
    buffer += next;
    next = peek(strm);

    if(!isdigit(next)) {
      logger << incomplete_float(curr);
      return token{invalid_ignored{}, start};
    }

    while(isdigit(next)) {
      extract(strm, curr);
      buffer += next;
      next = peek(strm);
    }

    if(next == 'f' || next == 'F') {
      extract(strm, curr);
      return token{literal{std::stof(buffer)}, start};
    }
    return token{literal{std::stod(buffer)}, start};
  }
  // -> integer
  return token{literal{std::stol(buffer)}, start};
}

inline token numerical_no_sign(std::istream &strm, location &curr) {
  const location start = curr;
  if(char next = peek(strm); next == '0') {
    extract(strm, curr);
    next = peek(strm);
    if(next == 'x' || next == 'X') {
      // -> hexadecimal literal
      extract(strm, curr);
      next = peek(strm);
      if(isxdigit(next)) return token{ literal{read_hex(strm, curr)}, start };
      logger << incomplete_hex(curr);
      return token{invalid_ignored{}, start};
    }
    if(next == '.') {
      extract(strm, curr);
      if(!isdigit(peek(strm))) {
        logger << incomplete_float(curr);
        return token{invalid_ignored{}, start};
      }
      return read_decimal(strm, start, curr, "0.");
    }
    if(isdigit(next)) {
      return read_decimal(strm, start, curr, ""); // leading 0 doesn't matter
    }

    return token{ literal<int64_t>{0}, start };
  }
  return read_decimal(strm, start, curr, "");
}

inline token id_kw_bool(std::istream &strm, location &curr) {
  const location start = curr;
  std::string buf;
  char next = peek(strm);
  while(isalnum(next) || next == '_') {
    extract(strm, curr);
    buf += next;
    next = peek(strm);
  }

  if(buf == "true") return token{literal{true}, start};
  if(buf == "false") return token{literal{false}, start};
  if(const auto it = keywords.find(buf); it != keywords.end()) return token{it->second, start};
  return token{identifier{std::move(buf)}, start};
}

inline token char_lit(std::istream &strm, location &curr) {
  const location start = curr;
  extract(strm, curr);

  char next = peek(strm);
  char lit;
  if(strm.eof()) {
    logger << unterminated_char(curr);
    return token{invalid_ignored{}, start};
  }
  if(next == '\'') {
    logger << zero_width_char(curr);
    return token{invalid_ignored{}, start};
  }
  if(next == '\\') {
    extract(strm, curr);
    switch(peek(strm)) {
      case '\\': lit = '\\'; break;
      case 'n': lit = '\n'; break;
      case 'r': lit = '\r'; break;
      case 't': lit = '\v'; break;
      case '0': lit = '\0'; break;
      case '\'': lit = '\''; break;
      default: {
        logger << invalid_escape_sequence_char(curr);
        lit = '\255';
        break;
      }
    }
  }
  else {
    lit = next;
  }

  extract(strm, curr);
  if(peek(strm) != '\'') {
    extract(strm, curr);
    next = peek(strm);
    while(next != '\'') {
      extract(strm, curr);
      next = peek(strm);
    }
    logger << char_literal_too_wide(curr);
    return token{invalid_ignored{}, start};
  }
  extract(strm, curr);
  return lit == '\255' ? token{invalid_ignored{}, start} : token{literal{lit}, start};
}

inline token string_lit(std::istream &strm, location &curr) {
  const location start = curr;
  std::string buf;
  extract(strm, curr);

  char next = peek(strm);
  bool escaped = false;
  while(next != '"' || escaped) {
    if(strm.eof()) {
      logger << unterminated_string(curr);
      return token{invalid_ignored{}, start};
    }

    if(escaped) {
      escaped = false;
      switch(next) {
        case '\\': buf += '\\'; break;
        case 'n': case '\n': buf += '\n'; break;
        case 'r': buf += '\r'; break;
        case 't': buf += '\v'; break;
        case '0': buf += '\0'; break;
        case '\"': buf += '\"'; break;
        default: {
          logger << invalid_escape_sequence_str(curr);
        }
      }
    }
    else {
      switch(next) {
        case '\n': {
          logger << newline_in_string(curr);
          return token{invalid_ignored{}, start};
        }
        case '\\': {
          escaped = true;
          break;
        }
        default: {
          buf += next;
          break;
        }
      }
    }

    extract(strm, curr);
    next = peek(strm);
  }

  extract(strm, curr);

  return token{literal{std::move(buf)}, start};
}

inline token starts_with_sign(std::istream &strm, location &curr) {
  const location start = curr;
  const char sign = peek(strm);
  extract(strm, curr);

  if(strm.eof()) {
    return token {sign == '+' ? symbol::PLUS : symbol::MINUS, start};
  }

  const char next = peek(strm);
  if(isdigit(next)) return read_decimal(strm, start, curr, std::string{sign});
  if(sign == '+') {
    if(next == '+') {
      extract(strm, curr);
      return token{symbol::INCREMENT, start};
    }
    if(next == '=') {
      extract(strm, curr);
      return token{symbol::PLUS_ASSIGN, start};
    }
    return token{symbol::PLUS, start};
  }
  // else -> sign == '-'
  if(next == '-') {
    extract(strm, curr);
    return token{symbol::DECREMENT, start};
  }
  if(next == '=') {
    extract(strm, curr);
    return token{symbol::MINUS_ASSIGN, start};
  }
  return token{symbol::MINUS, start};
}

inline void skip_line_comment(std::istream &strm, location &curr) {
  extract(strm, curr); // extract second '/' of '//'
  while(!strm.eof() && peek(strm) != '\n') extract(strm, curr);
  extract(strm, curr, true); // eat newline
}

inline void skip_block_comment(std::istream &strm, location &curr) {
  size_t depth = 1;
  bool was_star = false;
  bool was_slash = false;

  while(depth > 0) {
    if(strm.eof()) {
      logger << unterminated_block_comment(curr);
      return;
    }

    const char next = peek(strm);
    extract(strm, curr, next == '\n');

    if(next == '/') {
      if(was_star) { // '*/'
        was_star = false;
        depth--;
      }
      else { // '/'
        was_slash = true;
      }
    }
    else if(next == '*') {
      if(was_slash) { // '/*'
        was_slash = false;
        depth++;
      }
      else { // '*'
        was_star = true;
      }
    }
    else { // not important
      was_star = false;
      was_slash = false;
    }
  }
}

inline token div_or_comment(std::istream &strm, location &curr) {
  const location start = curr;
  extract(strm, curr); // current is '/'

  switch(peek(strm)) {
    case '/': {
      skip_line_comment(strm, curr);
      break;
    }
    case '*': {
      skip_block_comment(strm, curr);
      break;
    }
    case '=': {
      extract(strm, curr);
      return token{symbol::DIVIDE_ASSIGN, start};
    }
    default: return token{symbol::DIVIDE, start};
  }

  return token{invalid_ignored{}, start};
}

inline token multi_matcher(
  const symbol no_match, std::initializer_list<std::pair<char, symbol>> matchers, std::istream &strm,
  const location &start, location &curr
) {
  const char next = peek(strm);
  for(const auto &[c, s]: matchers) {
    if(next == c) {
      extract(strm, curr);
      return token{s, start};
    }
  }
  return token{no_match, start};
}

inline token symbol_token(std::istream &strm, location &curr) {
  const location start = curr;
  const char fst = peek(strm); // will never be +, -, or /
  extract(strm, curr);

  switch(fst) {
    // no case '+' (also no '++')
    // no case '-' (also no '--')
    case '*': return multi_matcher(symbol::MULTIPLY, {{'=', symbol::MULTIPLY_ASSIGN}}, strm, start, curr);
    // no case '/' (also no '//' or '/*')
    case '%': return multi_matcher(symbol::MODULO, {{'=', symbol::MODULO_ASSIGN}}, strm, start, curr);
    case '=': return multi_matcher(symbol::ASSIGN, {{'=', symbol::EQUALS}}, strm, start, curr);
    case '!': return multi_matcher(symbol::NOT, {{'=', symbol::NOT_EQUALS}}, strm, start, curr);
    case '<': return multi_matcher(
      symbol::LESS_THAN,
      {{'=', symbol::LESS_THAN_EQUALS}, {'<', symbol::SHIFT_LEFT}},
      strm, start, curr
    );
    case '>': return multi_matcher(
      symbol::GREATER_THAN,
      {{'=', symbol::GREATER_THAN_EQUALS}, {'>', symbol::SHIFT_RIGHT}},
      strm, start, curr
    );
    case '&': return multi_matcher(symbol::BIT_AND, {{'&', symbol::AND}, {'=', symbol::BIT_AND_ASSIGN}}, strm, start, curr);
    case '|': return multi_matcher(symbol::BIT_OR, {{'|', symbol::OR}, {'=', symbol::BIT_OR_ASSIGN}}, strm, start, curr);
    case '~': return token{symbol::BIT_NEG, start};
    case '^': return multi_matcher(symbol::XOR, {{'=', symbol::XOR_ASSIGN}}, strm, start, curr);
    case '.': return token{symbol::DOT, start};
    case ':': return multi_matcher(symbol::COLON, {{':', symbol::NAMESPACE}}, strm, start, curr);
    case '(': return token{symbol::PAREN_OPEN, start};
    case ')': return token{symbol::PAREN_CLOSE, start};
    case '[': return token{symbol::BRACKET_OPEN, start};
    case ']': return token{symbol::BRACKET_CLOSE, start};
    case '{': return token{symbol::BRACE_OPEN, start};
    case '}': return token{symbol::BRACE_CLOSE, start};
    case ',': return token{symbol::COMMA, start};
    case ';': return token{symbol::SEMI, start};
    case '?': return token{symbol::QUESTION, start};
    default: {
      logger << invalid_token(fst, start);
      return token{invalid_ignored{}, start};
    }
  }
}
}

token jayc::lexer::read_token(std::istream &strm, location &curr) {
  char next = peek(strm);
  while(is_whitespace(next) && !strm.eof()) {
    extract(strm, curr, next == '\n');
    next = peek(strm);
  }

  if(strm.eof()) return token{eof{}, curr};

  if(isdigit(next)) return numerical_no_sign(strm, curr); // definitely numerical
  if(isalpha(next) || next == '_') return id_kw_bool(strm, curr); // identifier or keyword or bool literal
  if(next == '\'') return char_lit(strm, curr); // character literal
  if(next == '"') return string_lit(strm, curr); // string literal
  if(next == '+' || next == '-') return starts_with_sign(strm, curr); // either symbol or numerical
  if(next == '/') return div_or_comment(strm, curr); // divide or comment
  return symbol_token(strm, curr);
}