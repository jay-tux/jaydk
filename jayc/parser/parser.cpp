//
// Created by jay on 9/5/24.
//

#include "util.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include "parse_error.hpp"

using namespace jaydk;
using namespace jayc;
using namespace jayc::lexer;
using namespace jayc::parser;

// no need to check for EOF -> we use an EOF token (so any token-check will fail, return std::nullopt and bubble up)

std::optional<qualified_name> parse_qname(token_it &iterator) {
  // <name>(::<name>)*
  const auto [_, pos] = *iterator;
  qualified_name res{};

  auto token = *iterator;
  iterator.consume();
  if(!is<identifier>(token.actual)) {
    logger << expect("qualified name", token);
    return std::nullopt;
  }

  res.sections.push_back(as<identifier>(token.actual).ident);
  token = *iterator;
  while(is<symbol>(token.actual) && as<symbol>(token.actual) == symbol::NAMESPACE) {
    iterator.consume();
    token = *iterator;
    iterator.consume();
    if(!is<identifier>(token.actual)) {
      logger << expect("qualified name", token);
      return std::nullopt;
    }

    res.sections.push_back(as<identifier>(token.actual).ident);
  }

  return res;
}

std::optional<type_name> parse_tname(token_it &iterator) {
  // <qname>(< <tname>(, <tname>)* >)? ([])?
  const auto [_, pos] = *iterator;
  auto base = parse_qname(iterator);
  if(!base.has_value()) {
    logger << expect("qualified type name", *iterator);
    return std::nullopt;
  }

  auto token = *iterator;
  std::vector<type_name> template_args;
  if(is<symbol>(iterator->actual) && as<symbol>(iterator->actual) == symbol::LESS_THAN) {
    iterator.consume(); // consume <
    while(!iterator.eof()) {
      const auto tname = parse_tname(iterator);
      if(!tname.has_value()) {
        logger << expect("template argument", *iterator);
        return std::nullopt;
      }
      template_args.push_back(*tname);

      token = *iterator;
      iterator.consume(); // consume , or >
      if(is<symbol>(token.actual)) {
        if(as<symbol>(token.actual) != symbol::COMMA) {
          logger << expect("comma (`,`) or closing bracket (`>`)", token);
          return std::nullopt;
        }

        if(as<symbol>(token.actual) == symbol::GREATER_THAN) {
          break;
        }
      }
      else {
        logger << expect("comma (`,`) or closing bracket (`>`)", token);
        return std::nullopt;
      }
    }
  }

  bool is_array = false;
  if(is<symbol>(iterator->actual) && as<symbol>(iterator->actual) == symbol::BRACKET_OPEN) {
    iterator.consume(); // consume [
    token = *iterator;
    iterator.consume(); // consume ]
    if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::BRACKET_CLOSE) {
      logger << expect("closing bracket (`]`)", token);
      return std::nullopt;
    }

    is_array = true;
  }

  return type_name{ .base_name = std::move(*base), .template_args = std::move(template_args), .is_array = is_array };
}

namespace expr_parsers {
struct expr_pratt {
  token_it &iterator;

  template <typename T>
  std::optional<expression> lit_expr_helper(const token &t) {
    if(is<literal<T>>(t.actual)) return expression(literal_expr<T>(as<literal<T>>(t.actual).value), t.pos);
    return std::nullopt;
  }

  std::optional<expression> literal_to_expr(const token &t) {
    return lit_expr_helper<int64_t>(t) || lit_expr_helper<uint64_t>(t) || lit_expr_helper<float>(t) ||
           lit_expr_helper<double>(t) || lit_expr_helper<char>(t) || lit_expr_helper<std::string>(t) ||
           lit_expr_helper<bool>(t);
  }

  constexpr static uint8_t precedence_for(const token_t &t) {
    if(!is<symbol>(t)) return 0;
    switch(as<symbol>(t)) {
      using enum symbol;
      case INCREMENT:
      case DECREMENT:
      case PAREN_OPEN:
      case BRACKET_OPEN:
      case DOT:
        return 12;

      case MULTIPLY:
      case DIVIDE:
      case MODULO:
        return 11;

      case PLUS:
      case MINUS:
        return 10;

      case SHIFT_LEFT:
      case SHIFT_RIGHT:
        return 9;

      case LESS_THAN:
      case GREATER_THAN:
      case LESS_THAN_EQUALS:
      case GREATER_THAN_EQUALS:
        return 8;

      case EQUALS:
      case NOT_EQUALS:
        return 7;

      case BIT_AND:
        return 6;

      case XOR:
        return 5;

      case BIT_OR:
        return 4;

      case AND:
        return 3;

      case OR:
        return 2;

      case QUESTION:
      case COLON:
        return 1;

      default:
        return 0;
    }
  }

  constexpr static bool is_prefix(const symbol s) {
    switch(s) {
      using enum symbol;
      case PLUS:
      case MINUS:
      case INCREMENT:
      case DECREMENT:
      case NOT:
      case BIT_NEG:
      case PAREN_OPEN: // also required to distinguish (<expr>) from <expr>(<exprs>)
        return true;

      default:
        return false;
    }
    return false;
  }

  constexpr static bool is_postfix(const symbol s) {
    switch(s) {
      using enum symbol;
      case INCREMENT:
      case DECREMENT:
        return true;

      default:
        return false;
    }
  }

  constexpr static std::optional<unary_op> un_op_for(const symbol s) {
    switch(s) {
      using enum symbol;
      case PLUS: return unary_op::UN_PLUS;
      case MINUS: return unary_op::UN_MINUS;
      case INCREMENT: return unary_op::PRE_INCR;
      case DECREMENT: return unary_op::PRE_DECR;
      case NOT: return unary_op::BOOL_NEG;
      case BIT_NEG: return unary_op::BIT_NEG;
      default: return std::nullopt;
    }
  }

  constexpr static std::optional<binary_op> bin_op_for(const symbol s) {
    switch(s) {
      case symbol::MULTIPLY: return binary_op::MULTIPLY;
      case symbol::DIVIDE: return binary_op::DIVIDE;
      case symbol::MODULO: return binary_op::MODULO;
      case symbol::PLUS: return binary_op::ADD;
      case symbol::MINUS: return binary_op::SUBTRACT;
      case symbol::SHIFT_LEFT: return binary_op::SHIFT_LEFT;
      case symbol::SHIFT_RIGHT: return binary_op::SHIFT_RIGHT;
      case symbol::LESS_THAN: return binary_op::LESS;
      case symbol::GREATER_THAN: return binary_op::GREATER;
      case symbol::LESS_THAN_EQUALS: return binary_op::LESS_EQUAL;
      case symbol::GREATER_THAN_EQUALS: return binary_op::GREATER_EQUAL;
      case symbol::EQUALS: return binary_op::EQUAL;
      case symbol::NOT_EQUALS: return binary_op::NOT_EQUAL;
      case symbol::BIT_AND: return binary_op::BIT_AND;
      case symbol::BIT_OR: return binary_op::BIT_OR;
      case symbol::XOR: return binary_op::XOR;
      case symbol::AND: return binary_op::BOOL_AND;
      case symbol::OR: return binary_op::BOOL_OR;

      default: return std::nullopt;
    }
  }

  std::optional<expression> parse_prefix(symbol s, const location &loc) {
    if(s == symbol::PAREN_OPEN) {
      // handle special case separately
      auto res = parse(0); // precedence 0 -> stop on ) or invalid token
      auto token = *iterator;
      iterator.consume();
      if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_CLOSE) {
        logger << expect("closing parenthesis (`)`)", token);
        return std::nullopt;
      }
      return res;
    }

    return merge(parse(precedence_for(s)), un_op_for(s)) | [&loc](const std::pair<expression, unary_op> &pair) {
      return expression(unary_expr{ .op = pair.second, .expr = alloc(pair.first) }, loc);
    };
  }

  std::optional<expression> parse_infix(const expression &left, const symbol s, const location &loc) {
    token token;
    switch(s) {
      case symbol::PAREN_OPEN: {
        // special case #1 -> functor call
        std::vector<expression> args;
        while(!iterator.eof()) {
          auto arg = parse(0); // precedence 0 -> stop on non-operator
          if(!arg.has_value()) return std::nullopt;
          args.emplace_back(std::move(*arg));

          token = *iterator;
          iterator.consume();
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

        return expression(call_expr{ .call = alloc(left), .args = std::move(args) }, loc);
      }

      case symbol::BRACKET_OPEN: {
        // special case #2 -> array access
        auto idx = parse(0); // precedence 0 -> stop on non-operator
        if(!idx.has_value()) return std::nullopt;

        token = *iterator;
        iterator.consume();
        if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::BRACKET_CLOSE) {
          logger << expect("closing bracket (`]`) after array index", token);
          return std::nullopt;
        }

        return expression(index_expr{ .base = alloc(left), .index = alloc(*idx) }, loc);
      }

      case symbol::DOT: {
        // special case #3 -> member access
        token = *iterator;
        iterator.consume();
        if(!is<identifier>(token.actual)) {
          logger << expect_identifier({token.actual, token.pos});
          return std::nullopt;
        }
        return expression(member_expr{ .base = alloc(left), .member = as<identifier>(token.actual).ident }, loc);
      }

      case symbol::QUESTION: {
        iterator.consume();
        const auto b_true = parse(0);
        if(!b_true.has_value()) return std::nullopt;
        token = *iterator;
        iterator.consume();
        if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::COLON) {
          logger << expect("colon (`:`)", token);
          return std::nullopt;
        }

        const auto b_false = parse(0);
        if(!b_false.has_value()) return std::nullopt;
        return expression(ternary_expr{ .cond = alloc(left), .true_expr = alloc(*b_true), .false_expr = alloc(*b_false) }, loc);
      }

      case symbol::INCREMENT:
      case symbol::DECREMENT: {
        // special case #4 -> postfix operators
        return expression(unary_expr{ .op = s == symbol::INCREMENT ? unary_op::PRE_INCR : unary_op::PRE_DECR, .expr = alloc(left) }, loc);
      }

      case symbol::MULTIPLY:
      case symbol::DIVIDE:
      case symbol::MODULO:
      case symbol::PLUS:
      case symbol::MINUS:
      case symbol::SHIFT_LEFT:
      case symbol::SHIFT_RIGHT:
      case symbol::LESS_THAN:
      case symbol::GREATER_THAN:
      case symbol::LESS_THAN_EQUALS:
      case symbol::GREATER_THAN_EQUALS:
      case symbol::EQUALS:
      case symbol::NOT_EQUALS:
      case symbol::BIT_AND:
      case symbol::BIT_OR:
      case symbol::XOR:
      case symbol::AND:
      case symbol::OR: {
        return merge(parse(precedence_for(s)), bin_op_for(s)) | [&loc, &left](const std::pair<expression, binary_op> &pair) {
          return expression(binary_expr{ .op = pair.second, .left = alloc(left), .right = alloc(pair.first) }, loc);
        };
      }

      default: {
        logger << expect("binary or postfix operator", ::token{s, loc});
        return std::nullopt;
      }
    }
  }

  std::optional<expression> parse(const uint8_t precedence) {
    auto token = *iterator;

    std::optional<expression> left = literal_to_expr(token);
    if(left.has_value()) {
      iterator.consume();
    }
    if(!left.has_value()) {
      if(is<identifier>(token.actual)) {
        auto qname = parse_qname(iterator);
        if(!qname.has_value()) {
          logger << expect("qualified name", token);
          return left;
        }
        left = expression(name_expr{ std::move(*qname) }, token.pos);
      }
      else if(is<symbol>(token.actual) && is_prefix(as<symbol>(token.actual))) {
        iterator.consume();
        left = parse_prefix(as<symbol>(token.actual), token.pos);
      }
    }

    if(!left.has_value()) {
      logger << expect("expression", token);
      return std::nullopt;
    }

    token = *iterator;
    while(precedence_for(token.actual) > precedence) {
      if(!is<symbol>(token.actual)) break; // TODO: check if is error?
      iterator.consume();

      left = parse_infix(*left, as<symbol>(token.actual), token.pos);
      if(!left.has_value()) return std::nullopt;

      token = *iterator;
    }

    return left;
  }
};

/*
 * --- precedence ---
 * 1  a::b::c (via parse_qname)
 * 2 	-> 12 <e>++         <e>--         <e>()         <e>[]         <e>.a
 *          ++<e>         --<e>         +<e>          - <e>         ~<e>          !<e>
 * 3  -> 11 <e>*<e>       <e>/<e>       <e>%<e>
 * 4  -> 10 <e>+<e>       <e>-<e>
 * 5  -> 9  <e> << <e>    <e> >> <e>
 * 6  -> 8  <e> < <e>     <e> > <e>     <e> <= <e>    <e> >= <e>
 * 7  -> 7  <e> == <e>    <e> != <e>
 * 8  -> 6  <e> & <e>
 * 9  -> 5  <e> ^ <e>
 * 10 -> 4  <e> | <e>
 * 11 -> 3  <e> && <e>
 * 12 -> 2  <e> || <e>
 * 13 -> 1  <e> ? <e> : <e>
 */
std::optional<expression> parse_expr(token_it &iterator) {
  expr_pratt local{ iterator };
  return local.parse(0);
}
}

using namespace expr_parsers;

namespace stmt_parsers {
template <typename T>
std::optional<T> check_semi(token_it &iterator, const T &t) {
  const auto [a, p] = *iterator;
  iterator.consume();
  if(is<symbol>(a) && as<symbol>(a) == symbol::SEMI) {
    return t;
  }
  logger << expect("semicolon (`;`)", {a, p});
  return std::nullopt;
}

std::optional<statement> parse_stmt(token_it &iterator);

constexpr std::optional<binary_op> compound_assign_op_for(const symbol s) {
  switch(s) {
    using enum symbol;
    case PLUS_ASSIGN: return binary_op::ADD;
    case MINUS_ASSIGN: return binary_op::SUBTRACT;
    case MULTIPLY_ASSIGN: return binary_op::MULTIPLY;
    case DIVIDE_ASSIGN: return binary_op::DIVIDE;
    case MODULO_ASSIGN: return binary_op::MODULO;
    case BIT_AND_ASSIGN: return binary_op::BIT_AND;
    case BIT_OR_ASSIGN: return binary_op::BIT_OR;
    case XOR_ASSIGN: return binary_op::XOR;
    default: return std::nullopt;
  }
}

std::optional<statement> parse_initial_expr_stmt(token_it &iterator) {
  // 3 options:
  // <expr>;
  // <expr> = <expr>;
  // <expr> <compound assignment> <expr>;

  auto pos = iterator->pos;
  const auto left = parse_expr(iterator);
  if(left == std::nullopt) return std::nullopt;

  if(!is<symbol>(iterator->actual)) {
    logger << expect("semicolon (`;`), assignment (`=`), or compound assignment operator", *iterator);
    iterator.consume();
    return std::nullopt;
  }

  switch(const auto sym = as<symbol>(iterator->actual)) {
    case symbol::SEMI: {
      // case 1: <expr>;
      iterator.consume(); // consume ;
      return statement(expr_stmt{ .expr = *left }, pos);
    }

    case symbol::ASSIGN: {
      //case 2: <expr> = <expr>;
      iterator.consume(); // consume =
      const auto right = parse_expr(iterator);
      if(right == std::nullopt) return std::nullopt;

      return check_semi(iterator, statement(assign_stmt{ .lvalue = *left, .value = *right }, pos));
    }

    case symbol::PLUS_ASSIGN:
    case symbol::MINUS_ASSIGN:
    case symbol::MULTIPLY_ASSIGN:
    case symbol::DIVIDE_ASSIGN:
    case symbol::MODULO_ASSIGN:
    case symbol::BIT_AND_ASSIGN:
    case symbol::BIT_OR_ASSIGN:
    case symbol::XOR_ASSIGN: {
      // case 3: <expr> <compound assignment> <expr>;
      iterator.consume(); // consume compound assignment
      const auto right = parse_expr(iterator);
      if(right == std::nullopt) return std::nullopt;

      auto token = *iterator;
      iterator.consume();
      if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::SEMI) {
        logger << expect("semicolon (`;`)", token);
        iterator.consume();
        return std::nullopt;
      }

      return compound_assign_op_for(sym) | [&left, &right, &pos](const binary_op &op) {
        return statement(op_assign_stmt{ .lvalue = *left, .op = op, .value = *right }, pos);
      };
    }

    default: {
      logger << expect("semicolon (`;`), assignment (`=`), or compound assignment operator", *iterator);
      return std::nullopt;
    }
  }
}

std::optional<statement> parse_var_decl_stmt(token_it &iterator) {
  // var <ident> = <expr>;
  const auto pos = iterator->pos;
  iterator.consume(); // consume var

  auto token = *iterator;
  iterator.consume(); // consume <ident>
  if(!is<identifier>(token.actual)) {
    logger << expect_identifier(token);
    return std::nullopt;
  }
  const std::string name = as<identifier>(token.actual).ident;

  token = *iterator;
  iterator.consume(); // consume =
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::ASSIGN) {
    logger << expect("assignment (`=`)", token);
    iterator.consume();
    return std::nullopt;
  }

  const auto expr = parse_expr(iterator);
  if(expr == std::nullopt) return std::nullopt;

  token = *iterator;
  iterator.consume(); // consume ;
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::SEMI) {
    logger << expect("semicolon (`;`)", token);
    return std::nullopt;
  }
  return statement(var_decl_stmt{ .name = name, .value = *expr }, pos);
}

std::optional<statement> parse_if_stmt(token_it &iterator) {
  // if (<expr>) <stmt> (else <stmt>)?
  const auto pos = iterator->pos;
  iterator.consume(); // consume if

  auto token = *iterator;
  iterator.consume(); // consume (
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_OPEN) {
    logger << expect("opening parenthesis (`(`)", token);
    return std::nullopt;
  }

  const auto cond = parse_expr(iterator);
  if(cond == std::nullopt) return std::nullopt;

  token = *iterator;
  iterator.consume(); // consume )
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_CLOSE) {
    logger << expect("closing parenthesis (`)`)", token);
    return std::nullopt;
  }

  const auto true_stmt = parse_stmt(iterator);
  if(true_stmt == std::nullopt) return std::nullopt;

  if_stmt res{ .condition = *cond, .true_block = alloc(*true_stmt), .false_block = std::nullopt };

  token = *iterator;
  if(is<keyword>(token.actual) && as<keyword>(token.actual) == keyword::ELSE) {
    iterator.consume(); // consume else

    const auto false_stmt = parse_stmt(iterator);
    if(false_stmt == std::nullopt) return std::nullopt;
    res.false_block = alloc(*false_stmt);
  }

  return statement(res, pos);
}

std::optional<statement> parse_for_stmt(token_it &iterator) {
  // for (<stmt (;) included> <expr>; <expr>) <body>      (for)
  // for (<ident> : <expr>) <body>                        (for-each)

  auto pos = iterator->pos;
  iterator.consume(); // consume for

  auto token = *iterator;
  iterator.consume(); // consume (
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_OPEN) {
    logger << expect("opening parenthesis (`(`)", token);
    return std::nullopt;
  }

  if(const auto [actual, _] = iterator.peek(); is<symbol>(actual) && as<symbol>(actual) == symbol::COLON) {
    // case 2: for (<ident> : <expr>) <body>
    token = *iterator;
    iterator.consume(); // consume <ident>
    if(!is<identifier>(token.actual)) {
      logger << expect_identifier(token);
      return std::nullopt;
    }

    const std::string name = as<identifier>(token.actual).ident;

    iterator.consume(); // consume : (checked)

    const auto expr = parse_expr(iterator);
    if(expr == std::nullopt) return std::nullopt;

    token = *iterator;
    if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_CLOSE) {
      logger << expect("closing parenthesis (`)`)", token);
      return std::nullopt;
    }

    const auto body = parse_stmt(iterator);
    if(body == std::nullopt) return std::nullopt;

    return statement(for_each_stmt{ .binding = name, .collection = *expr, .block = alloc(*body) }, pos);
  }

  // case 1: for (<stmt (;) included> <expr>; <expr>) <body>
  const auto stmt = parse_stmt(iterator);
  if(stmt == std::nullopt) return std::nullopt;

  const auto expr = parse_expr(iterator);
  if(expr == std::nullopt) return std::nullopt;

  token = *iterator;
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::SEMI) {
    logger << expect("semicolon (`;`)", token);
    return std::nullopt;
  }

  iterator.consume(); // consume ;

  const auto upd = parse_expr(iterator);
  if(upd == std::nullopt) return std::nullopt;

  token = *iterator;
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_CLOSE) {
    logger << expect("closing parenthesis (`)`)", token);
    return std::nullopt;
  }

  const auto body = parse_stmt(iterator);
  if(body == std::nullopt) return std::nullopt;

  return statement(for_stmt{ .init = alloc(*stmt), .condition = *expr, .update = *upd, .block = alloc(*body) }, pos);
}

std::optional<statement> parse_while_stmt(token_it &iterator) {
  // while (<expr>) <body>
  const auto pos = iterator->pos;
  iterator.consume(); // consume while

  auto token = *iterator;
  iterator.consume(); // consume (
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_OPEN) {
    logger << expect("opening parenthesis (`(`)", token);
    return std::nullopt;
  }

  const auto expr = parse_expr(iterator);
  if(expr == std::nullopt) return std::nullopt;

  token = *iterator;
  iterator.consume(); // consume )
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_CLOSE) {
    logger << expect("closing parenthesis (`)`)", token);
    return std::nullopt;
  }

  token = *iterator;
  iterator.consume(); // consume {
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::BRACE_OPEN) {
    logger << expect("opening brace (`{`)", token);
    return std::nullopt;
  }

  std::vector<statement> body;
  while(!is<symbol>(iterator->actual) || as<symbol>(iterator->actual) != symbol::BRACE_CLOSE) {
    logger << info{ iterator->pos, "Started parsing statement in while body; *iterator=" + token_type(*iterator)};
    const auto stmt = parse_stmt(iterator);
    if(stmt == std::nullopt) return std::nullopt;
    body.push_back(*stmt);
  }

  iterator.consume(); // consume }

  return statement(while_stmt{ .is_do_while = false, .condition = *expr, .block = body }, pos);
}

std::optional<statement> parse_do_while_stmt(token_it &iterator) {
  // do <body> while (<expr>)
  const auto pos = iterator->pos;
  iterator.consume(); // consume do

  auto token = *iterator;
  iterator.consume(); // consume }
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::BRACE_OPEN) {
    logger << expect("opening brace (`{`)", token);
    return std::nullopt;
  }

  std::vector<statement> body;
  while(!is<symbol>(iterator->actual) || as<symbol>(iterator->actual) != symbol::BRACE_CLOSE) {
    const auto stmt = parse_stmt(iterator);
    if(stmt == std::nullopt) return std::nullopt;
    body.push_back(*stmt);
  }

  iterator.consume(); // consume }

  token = *iterator;
  iterator.consume(); // consume while
  if(!is<keyword>(token.actual) || as<keyword>(token.actual) != keyword::WHILE) {
    logger << expect("while", token);
    return std::nullopt;
  }

  token = *iterator;
  iterator.consume(); // consume (
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_OPEN) {
    logger << expect("opening parenthesis (`(`)", token);
    return std::nullopt;
  }

  const auto expr = parse_expr(iterator);
  if(expr == std::nullopt) return std::nullopt;

  token = *iterator;
  iterator.consume(); // consume )
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::PAREN_CLOSE) {
    logger << expect("closing parenthesis (`)`)", token);
    return std::nullopt;
  }

  token = *iterator;
  iterator.consume(); // consume ;
  if(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::SEMI) {
    logger << expect("semicolon (`;`)", token);
    return std::nullopt;
  }

  return statement(while_stmt{ .is_do_while = true, .condition = *expr, .block = body }, pos);
}

std::optional<statement> parse_return_stmt(token_it &iterator) {
  // return <expr>?;
  const auto pos = iterator->pos;
  iterator.consume(); // consume return
  if(is<symbol>(iterator->actual) && as<symbol>(iterator->actual) == symbol::SEMI) {
    iterator.consume(); // consume ;
    return statement(return_stmt{ .value = std::nullopt }, pos);
  }

  const auto expr = parse_expr(iterator);
  if(!expr.has_value()) {
    return std::nullopt;
  }
  return check_semi(iterator, statement(return_stmt{ .value = expr }, pos));
}

std::optional<statement> parse_stmt(token_it &iterator) {
  // ignore empty statements
  while(is<symbol>(iterator->actual) && as<symbol>(iterator->actual) == symbol::SEMI) {
    iterator.consume();
  }

  const auto [actual, pos] = *iterator;
  if(is<symbol>(actual) && as<symbol>(actual) == symbol::BRACE_OPEN) {
    // { <body> }
    iterator.consume(); // consume {
    std::vector<statement> body;
    while(!is<symbol>(iterator->actual) || as<symbol>(iterator->actual) != symbol::BRACE_CLOSE) {
      const auto next = parse_stmt(iterator);
      if(!next.has_value()) {
        return std::nullopt;
      }

      body.push_back(*next);
    }

    iterator.consume(); // consume }
    return statement(block{ .statements = body }, pos);
  }

  if(is<keyword>(actual)) {
    switch(as<keyword>(actual)) {
      case keyword::VAR: return parse_var_decl_stmt(iterator);
      case keyword::IF: return parse_if_stmt(iterator);
      case keyword::FOR: return parse_for_stmt(iterator);
      case keyword::WHILE: return parse_while_stmt(iterator);
      case keyword::DO: return parse_do_while_stmt(iterator);
      case keyword::RETURN: return parse_return_stmt(iterator);
      case keyword::BREAK: return check_semi(iterator, statement(break_stmt{}, pos));
      case keyword::CONTINUE: return check_semi(iterator, statement(continue_stmt{}, pos));
      case keyword::ELSE: {
        logger << else_no_if(pos);
        return std::nullopt;
      }
      default: {
        logger << expect("statement", *iterator);
        iterator.consume();
        return std::nullopt;
      }
    }
  }

  return parse_initial_expr_stmt(iterator);
}
}

using namespace stmt_parsers;

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
    const auto p = iterator->pos;
    auto tname = parse_tname(iterator);
    if(tname == std::nullopt) return std::nullopt;
    auto a_name = *iterator;
    iterator.consume(); // consume arg name
    if(!is<identifier>(a_name.actual)) {
      logger << expect_identifier(a_name);
      return std::nullopt;
    }

    args.push_back({ .type = *tname, .name = as<identifier>(a_name.actual).ident, .pos = p });

    token = *iterator;
    iterator.consume(); // consume , or )
    if(is<symbol>(token.actual)) {
      if(as<symbol>(token.actual) == symbol::PAREN_CLOSE) {
        break;
      }
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
  while(!is<symbol>(token.actual) || as<symbol>(token.actual) != symbol::BRACE_CLOSE) {
    auto stmt = parse_stmt(iterator);
    if(stmt == std::nullopt) return std::nullopt;
    body.push_back(*stmt);
    token = *iterator;
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
      .type = *type,
      .name = name,
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
