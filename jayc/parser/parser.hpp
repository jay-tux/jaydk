//
// Created by jay on 9/5/24.
//

#ifndef PARSER_HPP
#define PARSER_HPP

#include <functional>

#include "lexer/token_stream.hpp"
#include "lexer/lexer.hpp"
#include "ast.hpp"

#include "lexer/token_output.hpp"

namespace jayc::parser {
template <typename F>
concept token_getter = requires(F f) {
  { f() } -> std::convertible_to<lexer::token>;
};

class token_it {
public:
  template <token_getter F>
  explicit token_it(F f) : get{std::move(f)} { current.actual = lexer::invalid_ignored{}; consume(); }

  const lexer::token &operator*() const { return current; }
  const lexer::token *operator->() const { return &current; }
  const lexer::token &peek() {
    if(!lookahead.has_value()) {
      if(jaydk::is<lexer::eof>(current.actual)) lookahead = current;
      else lookahead = get();
    }
    return lookahead.value();
  }

  void consume() {
    logger << info{ current.pos, "Consume: " + lexer::token_type(current) };

    if(lookahead.has_value()) {
      current = lookahead.value();
      lookahead = std::nullopt;
    }
    else {
      current = get();
    }
  }

  [[nodiscard]] constexpr bool eof() const { return jaydk::is<lexer::eof>(current.actual); }

private:
  std::function<lexer::token()> get;

  lexer::token current{};
  std::optional<lexer::token> lookahead = std::nullopt;
};

ast build_ast(token_it &iterator);

template <typename Stream> requires(std::derived_from<Stream, std::istream>)
class parser {
public:
  explicit parser(lexer::token_stream<lexer::lexer<Stream>> &&stream) : stream{std::move(stream)} {}

  inline ast parse() {
    auto it = token_it([this] {
      lexer::token t;
      this->stream >> t;
      return t;
    });
    return build_ast(it);
  }

private:
  lexer::token_stream<lexer::lexer<Stream>> stream;
};
}

#endif //PARSER_HPP
