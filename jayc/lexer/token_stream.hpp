//
// Created by jay on 9/2/24.
//

#ifndef TOKEN_STREAM_HPP
#define TOKEN_STREAM_HPP

#include <string>
#include <variant>
#include <cstdint>

#include "error_queue.hpp"

namespace jayc::lexer {
struct eof {};
struct invalid_ignored {};
enum class symbol {
  PLUS, MINUS, MULTIPLY, DIVIDE, MODULO,
  INCREMENT, DECREMENT, ASSIGN,
  EQUALS, NOT_EQUALS, LESS_THAN, GREATER_THAN, LESS_THAN_EQUALS, GREATER_THAN_EQUALS,
  AND, OR, NOT, BIT_AND, BIT_OR, BIT_NEG, XOR, SHIFT_LEFT, SHIFT_RIGHT,
  DOT, NAMESPACE,
  PAREN_OPEN, PAREN_CLOSE, BRACKET_OPEN, BRACKET_CLOSE, BRACE_OPEN, BRACE_CLOSE,
  COMMA, SEMI, COLON, QUESTION
};
enum class keyword {
  FUN, VAR, IF, ELSE, FOR, WHILE, DO, RETURN, BREAK, CONTINUE, NAMESPACE, STRUCT
};
struct identifier { std::string ident; };
template <typename T> struct literal { T value; };

using token_t = std::variant<
  eof, invalid_ignored, symbol, keyword, identifier, literal<int64_t>,
  literal<uint64_t>, literal<float>, literal<double>, literal<char>,
  literal<std::string>, literal<bool>
>;

struct token {
  token_t actual;
  location pos;
};

template <typename YS>
concept token_source = requires(YS &s, const YS &ss) {
  { s() } -> std::convertible_to<token>;
  { s.eof() } -> std::convertible_to<bool>;
  { ss.pos() } -> std::convertible_to<location>;
};

template <typename YS> requires(token_source<YS>)
class token_stream {
public:
  explicit token_stream(YS &&source) : source{std::forward<YS &&>(source)} {}
  token_stream(const token_stream &other) = delete;
  token_stream(token_stream &&other) = default;
  token_stream &operator=(const token_stream &other) = delete;
  token_stream &operator=(token_stream &&other) = default;

  [[nodiscard]] bool is_eof() const { return source.eof(); }

  inline token_stream &operator>>(token &t) {
    if(source.eof()) {
      t.actual = eof{};
      t.pos = source.pos();
    }
    else {
      do {
        t = source();
      } while(jaydk::is<invalid_ignored>(t.actual));
      return *this;
    }
    return *this;
  }

  ~token_stream() = default;
private:
  YS source;
};
}

#endif //TOKEN_STREAM_HPP
