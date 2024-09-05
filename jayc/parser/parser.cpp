//
// Created by jay on 9/5/24.
//

#include "parser.hpp"
#include "ast.hpp"
#include "parse_error.hpp"

using namespace jaydk;
using namespace jayc;
using namespace jayc::lexer;
using namespace jayc::parser;

std::optional<expression> parse_expr(token_it &iterator) {
  return std::nullopt;
}

std::optional<declaration> parse_decl(token_it &iterator);

std::optional<declaration> parse_fun_decl(token_it &iterator) {
  return std::nullopt;
}

std::optional<declaration> parse_type_decl(token_it &iterator) {
  return std::nullopt;
}

std::optional<declaration> parse_ns_decl(token_it &iterator) {
  // namespace <name> { <body> }
  const auto [_, ns_pos] = *iterator;
  iterator.consume(); // consume namespace
  const auto [actual, pos] = *iterator;
  iterator.consume(); // consume <name>
  if(!is<identifier>(actual)) {
    logger << expect_identifier({actual, pos});
    return std::nullopt;
  }

  const auto open_tok = *iterator;
  iterator.consume(); // consume {
  if(!is<symbol>(open_tok.actual) || as<symbol>(open_tok.actual) != symbol::BRACE_OPEN) {
    logger << expect("opening brace (`{`)", open_tok);
    return std::nullopt;
  }

  auto maybe_close = *iterator;
  std::vector<declaration> body;
  while(!is<symbol>(maybe_close.actual) || as<symbol>(maybe_close.actual) != symbol::BRACE_CLOSE) {
    if(const auto next = parse_decl(iterator); next.has_value()) body.push_back(next.value());
    maybe_close = *iterator;
  }

  iterator.consume(); // consume }

  return declaration(namespace_decl{
    .name = as<identifier>(actual).ident,
    .declarations = body
  }, ns_pos) | maybe{};
}

std::optional<declaration> parse_glob_decl(token_it &iterator) {
  // VAR <name> = <expr>;
  const auto var_tok = *iterator; // guaranteed by call
  iterator.consume(); // consume VAR
  const auto name_tok = *iterator;
  iterator.consume(); // consume <name>
  if(!is<identifier>(name_tok.actual)) {
    logger << expect_identifier({name_tok.actual, name_tok.pos});
    return std::nullopt;
  }

  const auto eq_tok = *iterator;
  iterator.consume(); // consume =
  if(!is<symbol>(eq_tok.actual) || as<symbol>(eq_tok.actual) != symbol::EQUALS) {
    logger << expect("assignment (`=`)", eq_tok);
    return std::nullopt;
  }

  const auto expr = parse_expr(iterator);
  if(expr == std::nullopt) return std::nullopt;

  const auto semi_tok = *iterator;
  iterator.consume(); // consume ;
  if(!is<symbol>(semi_tok.actual) || as<symbol>(semi_tok.actual) != symbol::SEMI) {
    logger << expect("semicolon (`;`)", semi_tok);
    return std::nullopt;
  }

  return declaration(
    global_decl{
      .name = as<identifier>(name_tok.actual).ident,
      .value = *expr
    },
    var_tok.pos
  ) | maybe{};
}

std::optional<declaration> parse_decl(token_it &iterator) {
  const auto &[actual, pos] = *iterator;
  if(!is<keyword>(actual)) {
    logger << expect_decl({actual, pos});
    std::exit(-100);
    return std::nullopt;
  }

  switch(as<keyword>(actual)) {
    case keyword::FUN:
      return parse_fun_decl(iterator);

    case keyword::STRUCT:
      return parse_type_decl(iterator);

    case keyword::NAMESPACE:
      return parse_ns_decl(iterator);

    case keyword::VAR:
      return parse_glob_decl(iterator);

    default:
      logger << expect_decl({actual, pos});
      return std::nullopt;
  }
}

ast jayc::parser::build_ast(token_it &iterator) {
  ast result;
  while(!iterator.eof()) {
    if(auto next = parse_decl(iterator); next.has_value())
      result.push_back(*next);
  }
  return result;
}
