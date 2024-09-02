//
// Created by jay on 9/2/24.
//

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <utility>
#include "token_stream.hpp"

using namespace jayc;
using namespace jayc::lexer;

std::unordered_map<std::string, keyword> keywords = {
  {"fun", keyword::FUN}, {"var", keyword::VAR}, {"if", keyword::IF},
  {"else", keyword::ELSE}, {"for", keyword::FOR}, {"while", keyword::WHILE},
  {"do", keyword::DO}, {"return", keyword::RETURN}, {"break", keyword::BREAK},
  {"continue", keyword::CONTINUE}, {"namespace", keyword::NAMESPACE},
  {"struct", keyword::STRUCT}
};

inline token numerical(const char first, std::ifstream &strm, location &curr) {
  const location start = curr;
  char next = static_cast<char>(strm.peek());
  if(next == 'x' || next == 'X') {
    strm.get(); curr.col++; // eat x/X
    // -> hex unsigned integer
    std::string buf;
    next = static_cast<char>(strm.peek());
    do {
      strm.get(); curr.col++; // eat digit, keep it around
      buf += next;
      next = static_cast<char>(strm.peek());
    } while(isxdigit(next) && !strm.eof());

    return token{literal{std::stoul(buf, nullptr, 16)}, start};
  }

  std::string buf;
  buf += first;
  if(isdigit(next)) {
    do {
      buf += next; strm.get(); curr.col++; // eat digit, keep it around
      next = static_cast<char>(strm.peek());
    } while(isdigit(next) && !strm.eof());
  }

  if(next == '.') {
    // -> float or double
    buf += next; strm.get(); curr.col++; // eat dot, keep it around

    next = static_cast<char>(strm.peek());
    if(isdigit(next)) {
      // -> actual value
      do {
        buf += next; strm.get(); curr.col++; // eat digit, keep it around
        next = static_cast<char>(strm.peek());
      } while(isdigit(next) && !strm.eof());

      // -> got all the digits, check if next is 'f' or 'F'
      if(next == 'f' || next == 'F') {
        strm.get(); curr.col++; // eat f/F
        return token{literal{std::stof(buf)}, start};
      }
      return token{literal{std::stod(buf)}, start};
    }
    // -> incomplete float or double: [0-9]\+.
    logger << error{ curr, std::string("Unexpected token `") + next + "` (expected digit)." };
    return token{invalid_ignored{}, start};
  }
  // -> integer
  return token{ literal{std::stol(buf)}, start };
}

inline token keyword_identifier(const char first, std::ifstream &strm, location &curr) {
  std::string buffer;
  buffer += first;
  const location start = curr;
  char next = static_cast<char>(strm.peek());
  while(isalnum(next) || next == '_') {
    buffer += next; strm.get(); curr.col++; // eat & keep
    next = static_cast<char>(strm.peek());
  }

  if(buffer == "true") return token{literal{true}, start};
  if(buffer == "false") return token{literal{false}, start};

  const auto it = keywords.find(buffer);
  if(it == keywords.end()) return token{identifier{buffer}, start};
  return token{it->second, start};
}

inline token escaped_char_literal(std::ifstream &strm, location start, location &curr) {
  char discard = static_cast<char>(strm.get()); curr.col++; // eat and keep
  char actual;
  switch(discard) {
    case '\'': actual = '\''; break;
    case '"': actual = '\"'; break;
    case '?': actual = '\?'; break;
    case '\\': actual = '\\'; break;
    case 'a': actual = '\a'; break;
    case 'b': actual = '\b'; break;
    case 'f': actual = '\f'; break;
    case 'n': actual = '\n'; break;
    case 'r': actual = '\r'; break;
    case 't': actual = '\t'; break;
    case 'v': actual = '\v'; break;
    case '0': actual = '\0'; break;

    default: {
      logger << error { std::move(start), "Invalid escape sequence in character literal." };
      actual = '!'; // (invalid ...)
    }
  }

  bool extra = false;
  discard = static_cast<char>(strm.get()); curr.col++; // eat
  while(discard != '\'') {
    discard = static_cast<char>(strm.get()); curr.col++; // eat
    extra = true;
  }

  if(extra) {
    logger << error { std::move(start), "Character literal too wide." };
    return token{invalid_ignored{}, start};
  }

  if(actual == '!') return token{ invalid_ignored{}, start };

  return token{literal{actual}, start};
}

inline token char_literal(std::ifstream &strm, location &curr) {
  const location start = curr;
  switch(strm.peek()) {
    case '\'': {
      // invalid case
      strm.get(); curr.col++; // eat it
      logger << error{ start, "Invalid 0-width character literal." };
      return token{invalid_ignored{}, start};
    }

    case '\\': {
      return escaped_char_literal(strm, start, curr);
    }

    default: {
      const char res = static_cast<char>(strm.get()); curr.col++; // eat and keep
      if(strm.peek() == '\'') {
        strm.get(); curr.col++; // eat it
        return token{literal{res}, start};
      }
      else {
        logger << error{ start, "Invalid character literal." };
        while(strm.peek() != '\'') {
          strm.get(); curr.col++; // eat them all!
        }
        strm.get(); curr.col++; // eat it
        return token{invalid_ignored{}, start};
      }
    }
  }
}

inline token string_literal(std::ifstream &strm, location &curr) {
  const location start = curr;
  bool escaped = false;
  bool was_escaped = false;
  bool is_error = false;
  std::string buffer;
  char last;

  do {
    if(strm.eof()) {
      logger << error{ start, "Unterminated string literal." };
      return token{invalid_ignored{}, start};
    }

    last = static_cast<char>(strm.get()); curr.col++; // eat and keep
    was_escaped = false;

    if(escaped) {
      switch(last) {
        case '\'': buffer += '\''; break;
        case '"': buffer += '\"'; break;
        case '?': buffer += '\?'; break;
        case '\\': buffer += '\\'; break;
        case 'a': buffer += '\a'; break;
        case 'b': buffer += '\b'; break;
        case 'f': buffer += '\f'; break;
        case 'n': buffer += '\n'; break;
        case 'r': buffer += '\r'; break;
        case 't': buffer += '\t'; break;
        case 'v': buffer += '\v'; break;
        case '0': buffer += '\0'; break;
        case '\n': buffer += '\n'; break;
        default: {
          logger << error{ curr, "Invalid escape sequence in string literal." };
          is_error = true;
        }
      }
      escaped = false;
      was_escaped = true;
    }
    else if(last == '\\') escaped = true;
    else if(last == '\n') {
      logger << error{ curr, "Unescaped newline in string literal." };
      is_error = true;
      curr.col = 0; curr.line++;
    }
    else if(last != '"') buffer += last;
  } while(last != '"' || was_escaped);

  if(is_error) return token{invalid_ignored{}, start};
  return token{literal{buffer}, start};
}

template <bool eat>
constexpr token get(const symbol s, std::ifstream &strm, const location &start, location &curr) {
  if constexpr(eat) { strm.get(); curr.col++; }
  return token{s, start};
}

inline token either(const symbol no_match, const char check, const symbol match, std::ifstream &strm, location &curr) {
  const location start = curr;
  if(strm.peek() == check) return get<true>(match, strm, start, curr);
  return get<false>(no_match, strm, start, curr);
}

inline token either_or(const symbol no_match, const char check, const symbol match, const char check2, const symbol match2, std::ifstream &strm, location &curr) {
  const location start = curr;
  const char temp = strm.peek();
  if(temp == check) return get<true>(match, strm, start, curr);
  if(temp == check2) return get<true>(match2, strm, start, curr);
  return get<false>(no_match, strm, start, curr);
}

inline token line_comment(std::ifstream &strm, location &curr) {
  // first up -> remove second / from //
  strm.get(); curr.col++;

  while(strm.peek() != '\n' && !strm.eof()) {
    const char c = static_cast<char>(strm.get()); curr.col++;
    if(c == '\n') {
      std::cout << "(newline)\n";
      curr.line++;
      curr.col = 0;
    }
  }

  return { invalid_ignored{}, curr };
}

inline token block_comment(std::ifstream &strm, location &curr) {
  size_t depth = 1;
  bool last_was_star = false;
  bool last_was_slash = false;

  while(!strm.eof() && depth != 0) {
    const char c = static_cast<char>(strm.get()); curr.col++;

    switch(c) {
      case '*': {
        if(last_was_slash) {
          depth++;
          last_was_slash = false;
        }
        else last_was_star = true;

        break;
      }
      case '/': {
        if(last_was_star) {
          depth--;
          last_was_star = false;
        }
        else last_was_slash = true;

        break;
      }

      case '\n': {
        curr.col = 0;
        curr.line++;

        last_was_slash = last_was_star = false;
        break;
      }

      default: {
        last_was_slash = last_was_star = false;
        break;
      }
    }
  }

  if(strm.eof() && depth != 0) {
    logger << error{ curr, "Unterminated block comment." };
  }

  return { invalid_ignored{}, curr };
}

inline token symbol_token(const char first, std::ifstream &strm, location &curr) {
  const location start = curr;
  switch(first) {
    case '!': return either(symbol::NOT, '=', symbol::NOT_EQUALS, strm, curr);
    case '%': return {symbol::MODULO, start};
    case '&': return either(symbol::BIT_AND, '&', symbol::AND, strm, curr);
    case '(': return { symbol::PAREN_OPEN, start };
    case ')': return { symbol::PAREN_CLOSE, start };
    case '*': return { symbol::MULTIPLY, start };
    case '+': return either(symbol::PLUS, '+', symbol::INCREMENT, strm, curr);
    case ',': return { symbol::COMMA, start };
    case '-': return either(symbol::MINUS, '-', symbol::DECREMENT, strm, curr);
    case '.': return { symbol::DOT, start };
    case '/': {
      const char next = static_cast<char>(strm.peek());
      if(next == '/') return line_comment(strm, curr);
      if(next == '*') return block_comment(strm, curr);
      return { symbol::DIVIDE, start };
    }
    case ':': return either(symbol::COLON, ':', symbol::NAMESPACE, strm, curr);
    case ';': return { symbol::SEMI, start };
    case '<': return either_or(symbol::LESS_THAN, '=', symbol::LESS_THAN_EQUALS, '<', symbol::SHIFT_LEFT, strm, curr);
    case '=': return either(symbol::ASSIGN, '=', symbol::EQUALS, strm, curr);
    case '>': return either_or(symbol::GREATER_THAN, '=', symbol::GREATER_THAN_EQUALS, '>', symbol::SHIFT_RIGHT, strm, curr);
    case '?': return { symbol::QUESTION, start };
    case '[': return { symbol::BRACKET_OPEN, start };
    case ']': return { symbol::BRACKET_CLOSE, start };
    case '^': return { symbol::XOR, start };
    case '{': return { symbol::BRACE_OPEN, start };
    case '|': return either(symbol::BIT_OR, '|', symbol::OR, strm, curr);
    case '}': return { symbol::BRACE_CLOSE, start };
    case '~': return { symbol::BIT_NEG, start };

    default: {
      logger << error{ curr, std::string("Invalid token `") + first + "'." };
      return token{invalid_ignored{}, start};
    }
  }
}

token next_token(std::ifstream &strm, location &curr) {
  char next;

  // eliminate whitespace, keep track of position
  do {
    next = static_cast<char>(strm.get());
    curr.col++;
    if(next == '\n') {
      curr.line++;
      curr.col = 0;
    }
  } while((isblank(next) || next == '\n' || next == '\v') && !strm.eof());

  if(strm.eof()) return token{eof{}, curr};

  // -> actual stuff
  if(isdigit(next)) {
    // -> numerical literal
    return numerical(next, strm, curr);
  }
  if(isalpha(next) || next == '_') {
    // -> identifier or keyword
    return keyword_identifier(next, strm, curr);
  }
  if(next == '\'') {
    // -> character literal
    return char_literal(strm, curr);
  }
  if(next == '"') {
    return string_literal(strm, curr);
  }
  // -> symbol or comment
  return symbol_token(next, strm, curr);
}