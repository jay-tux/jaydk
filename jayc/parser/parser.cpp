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

// no need to check for EOF -> we use an EOF token (so any token-check will fail, return std::nullopt and bubble up)

std::optional<type_name> parse_tname(token_it &iterator) {
  return std::nullopt;
}

std::optional<expression> parse_expr(token_it &iterator) {
  return std::nullopt;
}

std::optional<statement> parse_stmt(token_it &iterator) {
  return std::nullopt;
}

namespace decl_parsers {
std::optional<declaration> parse_decl(token_it &iterator);

std::optional<declaration> parse_fun_decl(token_it &iterator) {
  // fun (<type>.)?<name>((<type> <name>(, <type> <name>)*)?) { <body> }
  const auto [_, fun_pos] = *iterator;
  iterator.consume(); // consume fun

  auto token = iterator.peek();
  std::optional<type_name> receiver; // is null if normal, otherwise typename if receiver
  if(is<symbol>(token.actual) && as<symbol>(token.actual) == symbol::PAREN_OPEN) {
    receiver = std::nullopt;
  }
  else {
    auto tname = parse_tname(iterator);
    if(tname == std::nullopt) return std::nullopt;
    receiver = *tname;

    const auto next = *iterator;
    if(!is<symbol>(next.actual) || as<symbol>(next.actual) != symbol::DOT) {
      logger << expect("opening dot (`.`) after function receiver", next);
    }
    iterator.consume(); // consume .
  }

  token = *iterator;
  iterator.consume(); // consume <name>
  if(!is<identifier>(token.actual)) {
    logger << expect_identifier({token.actual, token.pos});
    return std::nullopt;
  }
  const std::string name = as<identifier>(token.actual).ident;

  iterator.consume(); // consume ( (already checked)

  std::vector<function_decl::arg> args;
  while(!iterator.eof()) {
    const auto p = (*iterator).pos;
    auto tname = parse_tname(iterator);
    if(tname == std::nullopt) return std::nullopt;
    auto a_name = *iterator;
    iterator.consume(); // consume arg name
    if(!is<identifier>(a_name.actual)) {
      logger << expect_identifier(a_name);
      return std::nullopt;
    }

    args.push_back({ .type = *tname, .name = as<identifier>(a_name.actual).ident, p });

    token = *iterator;
    iterator.consume(); // consume , or )
    if(is<symbol>(token.actual)) {
      if(as<symbol>(token.actual) == symbol::PAREN_CLOSE) break;
      if(as<symbol>(token.actual) != symbol::COMMA) {
        logger << expect("comma (`,`) or closing parenthesis (`)`)", token);
        return std::nullopt;
      }
    }
    else {
      logger << expect("comma (`,`) or closing parenthesis (`)`)", token);
      return std::nullopt;
    }
  }

  token = *iterator;
  iterator.consume();
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::BRACE_OPEN) {
    logger << expect("opening brace (`{`)", token);
    return std::nullopt;
  }

  std::vector<statement> body;
  token = *iterator;
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::BRACE_CLOSE) {
    auto stmt = parse_stmt(iterator);
    if(stmt == std::nullopt) return std::nullopt;
    body.push_back(*stmt);
  }

  iterator.consume(); // consume }

  if(receiver.has_value()) {
    return declaration(
      ext_function_decl { .receiver = *receiver, .name = name, .args = std::move(args), .body = std::move(body) },
      fun_pos
    );
  }

  return declaration(
    function_decl { .name = name, .args = std::move(args), .body = std::move(body) },
    fun_pos
  );
}

std::optional<declaration> parse_type_decl(token_it &iterator) {
  // struct <name>(<<template arg>(, <template arg>)*>)? (: <type> (, <type>)*)? { <body> }
  const auto [_, type_pos] = *iterator;
  iterator.consume(); // consume struct
  auto token = *iterator;
  iterator.consume(); // consume <name>
  if(!is<identifier>(token.actual)) {
    logger << expect_identifier(token);
    return std::nullopt;
  }

  const std::string name = as<identifier>(token.actual).ident;

  std::vector<std::string> template_args;
  if(is<symbol>(iterator->actual) && as<symbol>(iterator->actual) == symbol::LESS_THAN) {
    iterator.consume(); // consume <

    while(!iterator.eof()) {
      token = *iterator;
      iterator.consume(); // consume <name>
      if(!is<identifier>(token.actual)) {
        logger << expect_identifier(token);
        return std::nullopt;
      }

      template_args.push_back(as<identifier>(token.actual).ident);

      token = *iterator;
      iterator.consume(); // consume , or >
      if(is<symbol>(token.actual)) {
        if(as<symbol>(token.actual) == symbol::GREATER_THAN) break;
        if(as<symbol>(token.actual) != symbol::COMMA) {
          logger << expect("comma (`,`) or closing bracket (`>`)", token);
          return std::nullopt;
        }
      }
      else {
        logger << expect("comma (`,`) or closing bracket (`>`)", token);
        return std::nullopt;
      }
    }
  }

  std::vector<type_name> bases;
  token = *iterator;
  if(is<symbol>(token.actual) && as<symbol>(token.actual) == symbol::COLON) {
    iterator.consume(); // consume :
    while(!iterator.eof()) {
      const auto p = iterator->pos;
      auto tname = parse_tname(iterator);
      if(tname == std::nullopt) return std::nullopt;

      bases.push_back(*tname);

      token = *iterator;
      if(is<symbol>(token.actual)) {
        if(as<symbol>(token.actual) == symbol::BRACE_OPEN) break;
        if(as<symbol>(token.actual) != symbol::COMMA) {
          logger << expect("comma (`,`) or opening brace (`{`)", token);
          return std::nullopt;
        }
        iterator.consume(); // consume ,
      }
      else {
        logger << expect("comma (`,`) or opening brace (`{`)", token);
        return std::nullopt;
      }
    }
  }

  token = *iterator;
  iterator.consume(); // consume {
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::BRACE_OPEN) {
    logger << expect("opening brace (`{`)", token);
    return std::nullopt;
  }

  std::vector<std::pair<typed_global_decl, location>> fields;
  std::vector<std::pair<function_decl, location>> members;
  std::vector<std::pair<type_decl, location>> nested;
  while(!is<symbol>(iterator->actual) || as<symbol>(iterator->actual) != symbol::BRACE_CLOSE) {
    const auto decl = parse_decl(iterator);
    if(!decl.has_value()) return std::nullopt;

    if(is<global_decl>(decl->content)) {
      logger << untyped_field(decl->pos);
    }
    else if(is<namespace_decl>(decl->content)) {
      logger << ns_in_struct(decl->pos);
    }
    else if(is<typed_global_decl>(decl->content)) {
      fields.emplace_back(as<typed_global_decl>(decl->content), decl->pos);
    }
    else if(is<function_decl>(decl->content)) {
      members.emplace_back(as<function_decl>(decl->content), decl->pos);
    }
    else if(is<type_decl>(decl->content)) {
      nested.emplace_back(as<type_decl>(decl->content), decl->pos);
    }
  }

  iterator.consume(); // consume }

  return declaration(
    type_decl{
      .name = std::move(name), .template_args = std::move(template_args), .bases = std::move(bases),
      .fields = std::move(fields), .members = std::move(members), .nested_types = std::move(nested)
    },
    type_pos
  );
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

std::optional<declaration> parse_typed_glob_decl(token_it &iterator) {
  // <type> <name> (= <expr>)?;
  const auto pos = iterator->pos;
  auto type = parse_tname(iterator);
  if(!type.has_value()) return std::nullopt;

  auto token = *iterator;
  iterator.consume(); // consume <name>
  if(!is<identifier>(token.actual)) {
    logger << expect_identifier({token.actual, token.pos});
    return std::nullopt;
  }

  const auto name = as<identifier>(token.actual).ident;

  std::optional<expression> initial = std::nullopt;
  if(is<symbol>(iterator->actual) && as<symbol>(iterator->actual) == symbol::ASSIGN) {
    iterator.consume(); // consume =
    initial = parse_expr(iterator);
    if(initial == std::nullopt) return std::nullopt;
  }

  token = *iterator;
  iterator.consume(); // consume ;
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::SEMI) {
    logger << expect("semicolon (`;`)", token);
    return std::nullopt;
  }

  return declaration(
    typed_global_decl{
      .name = name,
      .type = *type,
      .initial = initial
    },
    pos
  ) | maybe{};
}

std::optional<declaration> parse_decl(token_it &iterator) {
  const auto &[actual, pos] = *iterator;

  if(is<identifier>(actual)) {
    return parse_typed_glob_decl(iterator);
  }

  if(!is<keyword>(actual)) {
    logger << expect_decl({actual, pos});
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
}

using namespace decl_parsers;

ast jayc::parser::build_ast(token_it &iterator) {
  ast result;
  while(!iterator.eof()) {
    if(auto next = parse_decl(iterator); next.has_value())
      result.push_back(*next);
  }
  return result;
}
