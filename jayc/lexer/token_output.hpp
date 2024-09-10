//
// Created by jay on 9/2/24.
//

#ifndef TOKEN_OUTPUT_HPP
#define TOKEN_OUTPUT_HPP

#include <iostream>
#include <sstream>

#include "token_stream.hpp"

namespace jayc::lexer {
inline std::ostream &operator<<(std::ostream &target, const eof &) {
  return target << "EOF";
}

inline std::ostream &operator<<(std::ostream &target, const invalid_ignored &) {
  return target << "invalid (error) or ignored (comment)";
}

inline std::ostream &operator<<(std::ostream &target, const symbol &sym) {
#define X(e, s) case jayc::lexer::symbol::e: return target << #e << " (`" s "`)";
#define SYMBOLS \
  X(PLUS, "+") X(MINUS, "-") X(MULTIPLY, "*") X(DIVIDE, "/") X(MODULO, "%") \
  X(INCREMENT, "++") X(DECREMENT, "--") \
  X(ASSIGN, "=") X(PLUS_ASSIGN, "+=") X(MINUS_ASSIGN, "-=") X(MULTIPLY_ASSIGN, "*=")\
  X(DIVIDE_ASSIGN, "/=") X(MODULO_ASSIGN, "%=")\
  X(BIT_AND_ASSIGN, "&=") X(BIT_OR_ASSIGN, "|=") X(XOR_ASSIGN, "^=")\
  X(EQUALS, "==") X(NOT_EQUALS, "!=") X(LESS_THAN, "<") X(LESS_THAN_EQUALS, "<=") \
  X(GREATER_THAN, ">") X(GREATER_THAN_EQUALS, ">=") \
  X(AND, "&&") X(OR, "||") X(NOT, "!") X(BIT_AND, "&") X(BIT_OR, "|") X(BIT_NEG, "~") \
  X(XOR, "^") X(SHIFT_LEFT, "<<") X(SHIFT_RIGHT, ">>") \
  X(DOT, ".") X(NAMESPACE, "::") \
  X(PAREN_OPEN, "(") X(PAREN_CLOSE, ")") X(BRACKET_OPEN, "[") X(BRACKET_CLOSE, "]") \
  X(BRACE_OPEN, "{") X(BRACE_CLOSE, "}") X(COMMA, ",") X(SEMI, ";") X(COLON, ":") X(QUESTION, "?");

  switch(sym) {
    SYMBOLS
  }

#undef SYMBOLS
#undef X
  return target;
}

inline std::ostream &operator<<(std::ostream &target, const keyword &kw) {
#define X(e) case jayc::lexer::keyword::e: return target << #e;
#define KEYWORDS \
  X(FUN) X(VAR) X(IF) X(ELSE) X(FOR) X(WHILE) X(DO) X(RETURN) X(BREAK) X(CONTINUE) \
  X(NAMESPACE) X(STRUCT) X(AUTO)

  switch(kw) {
    KEYWORDS
  }

#undef KEYWORDS
#undef X

  return target;
}

inline std::ostream &operator<<(std::ostream &target, const identifier &id) {
  return target << "IDENTIFIER (`" << id.ident << "`)";
}

namespace internal_ {
template <typename T>
struct _internal_type_name;
template <> struct _internal_type_name<int64_t> { constexpr static auto value = "int(64)"; };
template <> struct _internal_type_name<uint64_t> { constexpr static auto value = "uint(64)"; };
template <> struct _internal_type_name<float> { constexpr static auto value = "float(32)"; };
template <> struct _internal_type_name<double> { constexpr static auto value = "float(64)"; };
template <> struct _internal_type_name<char> { constexpr static auto value = "char(8)"; };
template <> struct _internal_type_name<std::string> { constexpr static auto value = "string"; };
template <> struct _internal_type_name<bool> { constexpr static auto value = "bool(1)"; };
}

template <typename T>
inline std::ostream &operator<<(std::ostream &target, const literal<T> &lit) {
  return target << "LITERAL(" << internal_::_internal_type_name<T>::value << ") `" << lit.value << "`";
}

inline std::ostream &operator<<(std::ostream &target, const token &tok) {
  target << tok.pos << ": ";
  std::visit([&target](const auto &x) { target << x; }, tok.actual);
  return target;
}

inline std::string token_type(const token &t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}
}

using jayc::lexer::operator<<;

#endif //TOKEN_OUTPUT_HPP
