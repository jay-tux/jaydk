//
// Created by jay on 9/6/24.
//

#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <doctest/doctest.h>

#include "parser/parser.hpp"
#include "parser/ast.hpp"
#include "parser/parse_error.hpp"

using namespace jaydk;
using namespace jayc;
using namespace jayc::lexer;
using namespace jayc::parser;

// NOTE: I know expanding std:: is not recommended, but otherwise this operator<< does not get picked up by doctest...
namespace std {
template<class T>
std::ostream &operator<<(std::ostream &target, const std::vector<T> &vec) { // NOLINT(*-dcl58-cpp)
  target << "[ ";
  auto it = vec.cbegin();
  if(it != vec.cend()) {
    target << *it;
    ++it;
    while(it != vec.cend()) {
      target << ", " << *it;
      ++it;
    }
  }
  target << " ]";

  return target;
}
}

class vec_source {
public:
  explicit vec_source(std::vector<token> tokens) : tokens{std::move(tokens)} {}

  token operator()() {
    if(idx < tokens.size()) {
      auto res = tokens[idx];
      ++idx;
      return res;
    }
    return token{eof{}, {}};
  }

private:
  std::vector<token> tokens;
  size_t idx = 0;
};

namespace internal_ {
template <typename R>
using range_iter_t = std::remove_cvref_t<decltype(std::declval<R>().begin())>;
template <typename R>
using range_contained_t = std::remove_cvref_t<decltype(*std::declval<range_iter_t<R>>())>;
}

template <std::ranges::range R>
std::vector<::internal_::range_contained_t<R>> to_vec(R &&range) {
  return {std::ranges::begin(range), std::ranges::end(range)};
}

TEST_SUITE("jayc - parser (parsing okay)") {
  TEST_CASE("valid qualified names") {
    // <identifier>(::<identifier>)*
    logger.enable_throw_on_error();

    SUBCASE("single identifier") {
      const vec_source source({
        {identifier{"a"}, {}}
      });
      auto it = token_it(source);
      const auto res = parse_qname(it);
      REQUIRE(res.has_value());
      REQUIRE(res->sections.size() == 1);
      CHECK(res->sections[0] == "a");
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("multiple identifiers") {
      const vec_source source({
        {identifier{"a"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"b"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"c"}, {}}
      });
      auto it = token_it(source);
      const auto res = parse_qname(it);
      REQUIRE(res.has_value());
      CAPTURE(res->sections);
      REQUIRE(res->sections.size() == 3);
      CHECK(res->sections[0] == "a");
      CHECK(res->sections[1] == "b");
      CHECK(res->sections[2] == "c");
      CHECK(is<eof>(it->actual));
    }
  }

  TEST_CASE("valid type names") {
    // <qname>(< <tname>(, <tname>)* >)? ([])?
    logger.enable_throw_on_error();

    SUBCASE("simple type name") {
      // type
      const vec_source source({
        {identifier{"type"}, {}}
      });
      auto it = token_it(source);
      auto res = parse_tname(it);
      REQUIRE(res.has_value());
      REQUIRE(res->base_name.sections.size() == 1);
      CHECK(res->base_name.sections[0] == "type");
      CHECK(res->template_args.size() == 0);
      CHECK_FALSE(res->is_array);
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("simple qualified type name") {
      // type::a::b
      const vec_source source({
        {identifier{"type"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"a"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"b"}, {}}
      });
      auto it = token_it(source);
      auto res = parse_tname(it);
      REQUIRE(res.has_value());
      REQUIRE(res->base_name.sections.size() == 3);
      CHECK(res->base_name.sections[0] == "type");
      CHECK(res->base_name.sections[1] == "a");
      CHECK(res->base_name.sections[2] == "b");
      CHECK(res->template_args.size() == 0);
      CHECK_FALSE(res->is_array);
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("simple qualified array type") {
      // type::a::b[]
      const vec_source source({
        {identifier{"type"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"a"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"b"}, {}},
        {symbol{symbol::BRACKET_OPEN}, {}},
        {symbol{symbol::BRACKET_CLOSE}, {}}
      });
      auto it = token_it(source);
      auto res = parse_tname(it);
      REQUIRE(res.has_value());
      REQUIRE(res->base_name.sections.size() == 3);
      CHECK(res->base_name.sections[0] == "type");
      CHECK(res->base_name.sections[1] == "a");
      CHECK(res->base_name.sections[2] == "b");
      CHECK(res->is_array);
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("single templated type name") {
      // type<a>
      const vec_source source({
        {identifier{"type"}, {}},
        {symbol{symbol::LESS_THAN}, {}},
        {identifier{"a"}, {}},
        {symbol{symbol::GREATER_THAN}, {}}
      });
      auto it = token_it(source);
      auto res = parse_tname(it);
      REQUIRE(res.has_value());
      REQUIRE(res->base_name.sections.size() == 1);
      CHECK(res->base_name.sections[0] == "type");
      REQUIRE(res->template_args.size() == 1);
      REQUIRE(res->template_args[0].base_name.sections.size() == 1);
      CHECK(res->template_args[0].base_name.sections[0] == "a");
      CHECK(res->template_args[0].template_args.size() == 0);
      CHECK_FALSE(res->template_args[0].is_array);
      CHECK_FALSE(res->is_array);
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("multiple templated type name") {
      // type<a, b>
      const vec_source source({
        {identifier{"type"}, {}},
        {symbol{symbol::LESS_THAN}, {}},
        {identifier{"a"}, {}},
        {symbol{symbol::COMMA}, {}},
        {identifier{"b"}, {}},
        {symbol{symbol::GREATER_THAN}, {}}
      });
      auto it = token_it(source);
      auto res = parse_tname(it);
      REQUIRE(res.has_value());
      // type
      REQUIRE(res->base_name.sections.size() == 1);
      CHECK(res->base_name.sections[0] == "type");
      CHECK(res->template_args.size() == 2);
      // <a,
      REQUIRE(res->template_args[0].base_name.sections.size() == 1);
      CHECK(res->template_args[0].base_name.sections[0] == "a");
      CHECK(res->template_args[0].template_args.size() == 0);
      CHECK_FALSE(res->template_args[0].is_array);
      // b>
      REQUIRE(res->template_args[1].base_name.sections.size() == 1);
      CHECK(res->template_args[1].base_name.sections[0] == "b");
      CHECK(res->template_args[1].template_args.size() == 0);
      CHECK_FALSE(res->template_args[1].is_array);
      CHECK_FALSE(res->is_array);
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("multiple templated qualified type name") {
      // type::a<b, c, d>
      const vec_source source({
        {identifier{"type"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"a"}, {}},
        {symbol{symbol::LESS_THAN}, {}},
        {identifier{"b"}, {}},
        {symbol{symbol::COMMA}, {}},
        {identifier{"c"}, {}},
        {symbol{symbol::COMMA}, {}},
        {identifier{"d"}, {}},
        {symbol{symbol::GREATER_THAN}, {}}
      });
      auto it = token_it(source);
      auto res = parse_tname(it);
      REQUIRE(res.has_value());
      // type::a
      REQUIRE(res->base_name.sections.size() == 2);
      //    type::
      CHECK(res->base_name.sections[0] == "type");
      //    a
      CHECK(res->base_name.sections[1] == "a");

      CHECK(res->template_args.size() == 3);

      // <b,
      REQUIRE(res->template_args[0].base_name.sections.size() == 1);
      CHECK(res->template_args[0].base_name.sections[0] == "b");
      CHECK(res->template_args[0].template_args.size() == 0);
      CHECK_FALSE(res->template_args[0].is_array);

      // c,
      REQUIRE(res->template_args[1].base_name.sections.size() == 1);
      CHECK(res->template_args[1].base_name.sections[0] == "c");
      CHECK(res->template_args[1].template_args.size() == 0);
      CHECK_FALSE(res->template_args[1].is_array);

      // d>
      REQUIRE(res->template_args[2].base_name.sections.size() == 1);
      CHECK(res->template_args[2].base_name.sections[0] == "d");
      CHECK(res->template_args[2].template_args.size() == 0);
      CHECK_FALSE(res->template_args[2].is_array);

      CHECK_FALSE(res->is_array);
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("mixed non-array and array template arguments") {
      // type<a, b::c>
      const vec_source source({
        {identifier{"type"}, {}},
        {symbol{symbol::LESS_THAN}, {}},
        {identifier{"a"}, {}},
        {symbol{symbol::COMMA}, {}},
        {identifier{"b"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"c"}, {}},
          {symbol{symbol::GREATER_THAN}, {}}
      });

      auto it = token_it(source);
      auto res = parse_tname(it);
      REQUIRE(res.has_value());
      // type
      REQUIRE(res->base_name.sections.size() == 1);
      CHECK(res->base_name.sections[0] == "type");
      CHECK(res->template_args.size() == 2);
      // <a,
      REQUIRE(res->template_args[0].base_name.sections.size() == 1);
      CHECK(res->template_args[0].base_name.sections[0] == "a");
      CHECK(res->template_args[0].template_args.size() == 0);
      CHECK_FALSE(res->template_args[0].is_array);
      // b::c>
      REQUIRE(res->template_args[1].base_name.sections.size() == 2);
      //    b::
      CHECK(res->template_args[1].base_name.sections[0] == "b");
      //    c
      CHECK(res->template_args[1].base_name.sections[1] == "c");
      CHECK(res->template_args[1].template_args.size() == 0);
      CHECK_FALSE(res->template_args[1].is_array);
      CHECK_FALSE(res->is_array);
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("crazy nested type name") {
      // ns::type<ns::nested::type, ns::type2<std::string>[], ns::home[], ns::deeply<nested<type[]>>>

      const vec_source source({
        {identifier{"ns"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"type"}, {}},
        {symbol{symbol::LESS_THAN}, {}},
        {identifier{"ns"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"nested"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"type"}, {}},
        {symbol{symbol::COMMA}, {}},
        {identifier{"ns"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"type2"}, {}},
        {symbol{symbol::LESS_THAN}, {}},
        {identifier{"std"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"string"}, {}},
        {symbol{symbol::GREATER_THAN}, {}},
        {symbol::BRACKET_OPEN, {}},
        {symbol::BRACKET_CLOSE, {}},
        {symbol{symbol::COMMA}, {}},
        {identifier{"ns"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"home"}, {}},
        {symbol::BRACKET_OPEN, {}},
        {symbol::BRACKET_CLOSE, {}},
        {symbol{symbol::COMMA}, {}},
        {identifier{"ns"}, {}},
        {symbol{symbol::NAMESPACE}, {}},
        {identifier{"deeply"}, {}},
        {symbol{symbol::LESS_THAN}, {}},
        {identifier{"nested"}, {}},
        {symbol{symbol::LESS_THAN}, {}},
        {identifier{"type"}, {}},
        {symbol::BRACKET_OPEN, {}},
        {symbol::BRACKET_CLOSE, {}},
        {symbol{symbol::GREATER_THAN}, {}},
        {symbol{symbol::GREATER_THAN}, {}},
        {symbol{symbol::GREATER_THAN}, {}}
      });

      auto it = token_it(source);
      auto res = parse_tname(it);
      REQUIRE(res.has_value());
      // ns::type
      REQUIRE(res->base_name.sections.size() == 2);
      CHECK(res->base_name.sections[0] == "ns");
      CHECK(res->base_name.sections[1] == "type");
      CHECK(res->template_args.size() == 4);
      CHECK_FALSE(res->is_array);
      //    ns::nested::type,
      REQUIRE(res->template_args[0].base_name.sections.size() == 3);
      CHECK(res->template_args[0].base_name.sections[0] == "ns");
      CHECK(res->template_args[0].base_name.sections[1] == "nested");
      CHECK(res->template_args[0].base_name.sections[2] == "type");
      CHECK(res->template_args[0].template_args.size() == 0);
      CHECK_FALSE(res->template_args[0].is_array);
      //    ns::type2<>[],
      REQUIRE(res->template_args[1].base_name.sections.size() == 2);
      CHECK(res->template_args[1].base_name.sections[0] == "ns");
      CHECK(res->template_args[1].base_name.sections[1] == "type2");
      CHECK(res->template_args[1].is_array);
      //        std::string
      REQUIRE(res->template_args[1].template_args.size() == 1);
      CHECK(res->template_args[1].template_args[0].base_name.sections.size() == 2);
      CHECK(res->template_args[1].template_args[0].base_name.sections[0] == "std");
      CHECK(res->template_args[1].template_args[0].base_name.sections[1] == "string");
      CHECK(res->template_args[1].template_args[0].template_args.size() == 0);
      CHECK_FALSE(res->template_args[1].template_args[0].is_array);
      //    ns::home[],
      REQUIRE(res->template_args[2].base_name.sections.size() == 2);
      CHECK(res->template_args[2].base_name.sections[0] == "ns");
      CHECK(res->template_args[2].base_name.sections[1] == "home");
      CHECK(res->template_args[2].template_args.size() == 0);
      CHECK(res->template_args[2].is_array);
      //    ns::deeply<>
      REQUIRE(res->template_args[3].base_name.sections.size() == 2);
      CHECK(res->template_args[3].base_name.sections[0] == "ns");
      CHECK(res->template_args[3].base_name.sections[1] == "deeply");
      CHECK_FALSE(res->template_args[3].is_array);
      //        nested<>
      REQUIRE(res->template_args[3].template_args.size() == 1);
      CHECK(res->template_args[3].template_args[0].base_name.sections.size() == 1);
      CHECK(res->template_args[3].template_args[0].base_name.sections[0] == "nested");
      CHECK_FALSE(res->template_args[3].template_args[0].is_array);
      //            type[]
      CHECK(res->template_args[3].template_args[0].template_args.size() == 1);
      CHECK(res->template_args[3].template_args[0].template_args[0].base_name.sections.size() == 1);
      CHECK(res->template_args[3].template_args[0].template_args[0].base_name.sections[0] == "type");
      CHECK(res->template_args[3].template_args[0].template_args[0].template_args.size() == 0);
      CHECK(res->template_args[3].template_args[0].template_args[0].is_array);

      CHECK(is<eof>(it->actual));
    }
  }

  TEST_CASE("valid expressions") {
    logger.enable_throw_on_error();
    SUBCASE("literal expressions") {
      const std::vector tokens {
        token{literal<int64_t>{-12}, {}},
        token{literal<uint64_t>{1234}, {}},
        token{literal{123.3f}, {}},
        token{literal{158.9}, {}},
        token{literal{true}, {}},
        token{literal{'@'}, {}},
        token{literal<std::string>{"hello!"}, {}},
      };
      const vec_source source(tokens);
      auto it = token_it(source);

      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<literal_expr<int64_t>>(res->content));
      CHECK(as<literal_expr<int64_t>>(res->content).value == -12);

      res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<literal_expr<uint64_t>>(res->content));
      CHECK(as<literal_expr<uint64_t>>(res->content).value == 1234);

      res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<literal_expr<float>>(res->content));
      CHECK(as<literal_expr<float>>(res->content).value == 123.3f);

      res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<literal_expr<double>>(res->content));
      CHECK(as<literal_expr<double>>(res->content).value == 158.9);

      res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<literal_expr<bool>>(res->content));
      CHECK(as<literal_expr<bool>>(res->content).value == true);

      res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<literal_expr<char>>(res->content));
      CHECK(as<literal_expr<char>>(res->content).value == '@');

      res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<literal_expr<std::string>>(res->content));
      CHECK(as<literal_expr<std::string>>(res->content).value == "hello!");

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("(qualified) identifier expression") {
      const vec_source source({
        {identifier{"a"}, {"", 1, 1}},
        {symbol{symbol::NAMESPACE}, {"", 1, 2}},
        {identifier{"b"}, {"", 1, 3}},
        {symbol{symbol::NAMESPACE}, {"", 1, 3}},
        {identifier{"c"}, {"", 1, 4}},
      });

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<name_expr>(res->content));
      CHECK(as<name_expr>(res->content).name.sections.size() == 3);
      CHECK(as<name_expr>(res->content).name.sections[0] == "a");
      CHECK(as<name_expr>(res->content).name.sections[1] == "b");
      CHECK(as<name_expr>(res->content).name.sections[2] == "c");

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("trivial unary prefix expressions") {
      std::unordered_map<unary_op, symbol> ops{
        {unary_op::UN_PLUS, symbol::PLUS},
        {unary_op::UN_MINUS, symbol::MINUS},
        {unary_op::PRE_INCR, symbol::INCREMENT},
        {unary_op::PRE_DECR, symbol::DECREMENT},
        {unary_op::BOOL_NEG, symbol::NOT},
        {unary_op::BIT_NEG, symbol::BIT_NEG},
      };

      for(const auto &[op, tok] : ops) {
        const vec_source source({
          {tok, {"", 1, 1}},
          {literal<int64_t>{12}}
        });

        auto it = token_it(source);
        auto res = parse_expr(it);
        REQUIRE(res.has_value());
        CHECK(is<unary_expr>(res->content));
        CHECK(as<unary_expr>(res->content).op == op);
        const auto &arg = *as<unary_expr>(res->content).expr;
        CHECK(is<literal_expr<int64_t>>(arg.content));
        CHECK(as<literal_expr<int64_t>>(arg.content).value == 12);

        CHECK(is<eof>(it->actual));
      }
    }

    SUBCASE("trivial unary postfix expressions") {
      std::unordered_map<unary_op, symbol> ops{
          {unary_op::POST_INCR, symbol::INCREMENT},
          {unary_op::POST_DECR, symbol::DECREMENT},
        };

      for(const auto &[op, tok] : ops) {
        const vec_source source({
          {literal<int64_t>{12}},
          {tok, {"", 1, 1}},
        });

        auto it = token_it(source);
        auto res = parse_expr(it);
        REQUIRE(res.has_value());
        CHECK(is<unary_expr>(res->content));
        CHECK(as<unary_expr>(res->content).op == op);
        const auto &arg = *as<unary_expr>(res->content).expr;
        CHECK(is<literal_expr<int64_t>>(arg.content));
        CHECK(as<literal_expr<int64_t>>(arg.content).value == 12);

        CHECK(is<eof>(it->actual));
      }
    }

    SUBCASE("trivial binary infix expressions") {
      std::unordered_map<binary_op, symbol> ops{
        {binary_op::ADD, symbol::PLUS},
        {binary_op::SUBTRACT, symbol::MINUS},
        {binary_op::MULTIPLY, symbol::MULTIPLY},
        {binary_op::DIVIDE, symbol::DIVIDE},
        {binary_op::MODULO, symbol::MODULO},
        {binary_op::EQUAL, symbol::EQUALS},
        {binary_op::NOT_EQUAL, symbol::NOT_EQUALS},
        {binary_op::LESS, symbol::LESS_THAN},
        {binary_op::GREATER, symbol::GREATER_THAN},
        {binary_op::LESS_EQUAL, symbol::LESS_THAN_EQUALS},
        {binary_op::GREATER_EQUAL, symbol::GREATER_THAN_EQUALS},
        {binary_op::BOOL_AND, symbol::AND},
        {binary_op::BOOL_OR, symbol::OR},
        {binary_op::BIT_AND, symbol::BIT_AND},
        {binary_op::BIT_OR, symbol::BIT_OR},
        {binary_op::XOR, symbol::XOR},
        {binary_op::SHIFT_LEFT, symbol::SHIFT_LEFT},
        {binary_op::SHIFT_RIGHT, symbol::SHIFT_RIGHT}
      };

      for(const auto &[op, tok] : ops) {
        const vec_source source({
          {literal<int64_t>{12}, {"", 1, 0}},
          {tok, {"", 1, 1}},
          {identifier{"x"}, {"", 1, 2}}
        });

        auto it = token_it(source);
        auto res = parse_expr(it);
        REQUIRE(res.has_value());
        CHECK(is<binary_expr>(res->content));
        CHECK(as<binary_expr>(res->content).op == op);
        const auto &left = *as<binary_expr>(res->content).left;
        CHECK(is<literal_expr<int64_t>>(left.content));
        CHECK(as<literal_expr<int64_t>>(left.content).value == 12);

        const auto &right = *as<binary_expr>(res->content).right;
        CHECK(is<name_expr>(right.content));
        CHECK(as<name_expr>(right.content).name.sections.size() == 1);
        CHECK(as<name_expr>(right.content).name.sections[0] == "x");

        CHECK(is<eof>(it->actual));
      }
    }

    SUBCASE("trivial ternary expression") {
      const vec_source source({
        {literal<int64_t>{12}, {"", 1, 0}},
        {symbol{symbol::QUESTION}, {"", 1, 1}},
        {identifier{"x"}, {"", 1, 2}},
        {symbol{symbol::COLON}, {"", 1, 3}},
        {identifier{"y"}, {"", 1, 4}}
      });
      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<ternary_expr>(res->content));

      const auto &condition = *as<ternary_expr>(res->content).cond;
      CHECK(is<literal_expr<int64_t>>(condition.content));
      CHECK(as<literal_expr<int64_t>>(condition.content).value == 12);

      const auto &true_expr = *as<ternary_expr>(res->content).true_expr;
      CHECK(is<name_expr>(true_expr.content));
      CHECK(as<name_expr>(true_expr.content).name.sections.size() == 1);
      CHECK(as<name_expr>(true_expr.content).name.sections[0] == "x");

      const auto &false_expr = *as<ternary_expr>(res->content).false_expr;
      CHECK(is<name_expr>(false_expr.content));
      CHECK(as<name_expr>(false_expr.content).name.sections.size() == 1);
      CHECK(as<name_expr>(false_expr.content).name.sections[0] == "y");

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("trivial parenthesized expressions") {
      const vec_source source1({
        {symbol{symbol::PAREN_OPEN}, {"", 1, 0}},
        {literal<int64_t>{12}, {"", 1, 1}},
        {symbol{symbol::PAREN_CLOSE}, {"", 1, 2}}
      });
      auto it = token_it(source1);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<literal_expr<int64_t>>(res->content));
      CHECK(as<literal_expr<int64_t>>(res->content).value == 12);

      CHECK(is<eof>(it->actual));

      const vec_source source2({
        {symbol{symbol::PAREN_OPEN}, {"", 1, 0}},
        {identifier{"x"}, {"", 1, 1}},
        {symbol{symbol::PLUS}, {"", 1, 2}},
        {identifier{"y"}, {"", 1, 3}},
        {symbol{symbol::PAREN_CLOSE}, {"", 1, 4}}
      });
      it = token_it(source2);
      res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<binary_expr>(res->content));
      CHECK(as<binary_expr>(res->content).op == binary_op::ADD);

      const auto &left = *as<binary_expr>(res->content).left;
      CHECK(is<name_expr>(left.content));
      CHECK(as<name_expr>(left.content).name.sections.size() == 1);
      CHECK(as<name_expr>(left.content).name.sections[0] == "x");

      const auto &right = *as<binary_expr>(res->content).right;
      CHECK(is<name_expr>(right.content));
      CHECK(as<name_expr>(right.content).name.sections.size() == 1);
      CHECK(as<name_expr>(right.content).name.sections[0] == "y");

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("priority 1: RTL tree (increasing priorities)") {
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

      const static auto i = [](const int x) {
        return token{literal<int64_t>{x}, {}};
      };
      const static auto o = [](const symbol t) {
        return token{t, {}};
      };

      using enum symbol;
      const vec_source source({
        i(1), o(OR), i(2), o(AND), i(3), o(BIT_OR), i(4), o(XOR), i(5),
        o(BIT_AND), i(6), o(NOT_EQUALS), i(7), o(GREATER_THAN), i(8),
        o(SHIFT_RIGHT), i(9), o(PLUS), i(10), o(MULTIPLY), i(11)
      });

      const std::vector<std::pair<binary_op, int>> expected_sequence = {
        {binary_op::BOOL_OR, 1}, {binary_op::BOOL_AND, 2}, {binary_op::BIT_OR, 3},
        {binary_op::XOR, 4}, {binary_op::BIT_AND, 5}, {binary_op::NOT_EQUAL, 6},
        {binary_op::GREATER, 7}, {binary_op::SHIFT_RIGHT, 8},
        {binary_op::ADD, 9}, {binary_op::MULTIPLY, 10}
      };

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      const expression *curr = &*res;
      for(const auto &[op, l_arg] : expected_sequence) {
        REQUIRE(is<binary_expr>(curr->content));
        CHECK(as<binary_expr>(curr->content).op == op);
        REQUIRE(is<literal_expr<int64_t>>(as<binary_expr>(curr->content).left->content));
        CHECK(as<literal_expr<int64_t>>(as<binary_expr>(curr->content).left->content).value == l_arg);
        curr = &*as<binary_expr>(curr->content).right;
      }
      REQUIRE(is<literal_expr<int64_t>>(curr->content));
      CHECK(as<literal_expr<int64_t>>(curr->content).value == 11);

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("priority 2: LTR tree (decreasing priorities)") {
      const static auto i = [](const int x) {
        return token{literal<int64_t>{x}, {}};
      };
      const static auto o = [](const symbol t) {
        return token{t, {}};
      };

      using enum symbol;
      const vec_source source({
        i(1), o(DIVIDE), i(2), o(MINUS), i(3), o(SHIFT_LEFT), i(4), o(LESS_THAN_EQUALS), i(5),
        o(EQUALS), i(6), o(BIT_AND), i(7), o(XOR), i(8), o(BIT_OR), i(9), o(AND), i(10), o(OR), i(11)
      });

      const std::vector<std::pair<binary_op, int>> expected_sequence = {
        {binary_op::BOOL_OR, 11}, {binary_op::BOOL_AND, 10}, {binary_op::BIT_OR, 9},
        {binary_op::XOR, 8}, {binary_op::BIT_AND, 7}, {binary_op::EQUAL, 6},
        {binary_op::LESS_EQUAL, 5}, {binary_op::SHIFT_LEFT, 4}, {binary_op::SUBTRACT, 3},
        {binary_op::DIVIDE, 2}
      };

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      const expression *curr = &*res;
      for(const auto &[op, l_arg] : expected_sequence) {
        REQUIRE(is<binary_expr>(curr->content));
        CHECK(as<binary_expr>(curr->content).op == op);
        REQUIRE(is<literal_expr<int64_t>>(as<binary_expr>(curr->content).right->content));
        CHECK(as<literal_expr<int64_t>>(as<binary_expr>(curr->content).right->content).value == l_arg);
        curr = &*as<binary_expr>(curr->content).left;
      }
      REQUIRE(is<literal_expr<int64_t>>(curr->content));
      CHECK(as<literal_expr<int64_t>>(curr->content).value == 1);

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("no-args call expression") {
      const vec_source source({
        {identifier{"a"}, {"", 1, 1}},
        {symbol{symbol::PAREN_OPEN}, {"", 1, 2}},
        {symbol{symbol::PAREN_CLOSE}, {"", 1, 3}}
      });

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<call_expr>(res->content));
      const auto &[call, args] = as<call_expr>(res->content);
      CHECK(is<name_expr>(call->content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &name = as<name_expr>(call->content);
      CHECK(name.name.sections.size() == 1);
      CHECK(name.name.sections[0] == "a");
      CHECK(args.empty());

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("single-arg call expression") {
      const vec_source source({
        {identifier{"a"}, {"", 1, 1}},
        {symbol{symbol::PAREN_OPEN}, {"", 1, 2}},
        {identifier{"b"}, {"", 1, 3}},
        {symbol{symbol::PAREN_CLOSE}, {"", 1, 4}}
      });

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<call_expr>(res->content));
      const auto &[call, args] = as<call_expr>(res->content);
      CHECK(is<name_expr>(call->content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &name = as<name_expr>(call->content);
      CHECK(name.name.sections.size() == 1);
      CHECK(name.name.sections[0] == "a");
      CHECK(args.size() == 1);
      CHECK(is<name_expr>(args[0].content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg = as<name_expr>(args[0].content);
      CHECK(arg.name.sections.size() == 1);
      CHECK(arg.name.sections[0] == "b");

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("multi-arg call expression") {
      // method(arg::no1, 1 + 2 * 4, "test")
      const vec_source source({
        {identifier{"method"}, {"", 1, 1}},
        {symbol{symbol::PAREN_OPEN}, {"", 1, 7}},
        {identifier{"arg"}, {"", 1, 8}},
        {symbol{symbol::NAMESPACE}, {"", 1, 9}},
        {identifier{"no1"}, {"", 1, 10}},
        {symbol{symbol::COMMA}, {"", 1, 11}},
        {literal<int64_t>{1}, {"", 1, 13}},
        {symbol{symbol::PLUS}, {"", 1, 14}},
        {literal<int64_t>{2}, {"", 1, 15}},
        {symbol{symbol::MULTIPLY}, {"", 1, 16}},
        {literal<int64_t>{4}, {"", 1, 18}},
        {symbol{symbol::COMMA}, {"", 1, 19}},
        {literal<std::string>{"test"}, {"", 1, 21}},
        {symbol{symbol::PAREN_CLOSE}, {"", 1, 28}}
      });

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      CHECK(is<call_expr>(res->content));
      const auto &[call, args] = as<call_expr>(res->content);
      CHECK(is<name_expr>(call->content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &name = as<name_expr>(call->content);
      CHECK(name.name.sections.size() == 1);
      CHECK(name.name.sections[0] == "method");
      CHECK(args.size() == 3);

      REQUIRE(is<name_expr>(args[0].content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg1 = as<name_expr>(args[0].content);
      CHECK(arg1.name.sections.size() == 2);
      CHECK(arg1.name.sections[0] == "arg");
      CHECK(arg1.name.sections[1] == "no1");

      REQUIRE(is<binary_expr>(args[1].content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg2 = as<binary_expr>(args[1].content);
      CHECK(arg2.op == binary_op::ADD);

      REQUIRE(is<literal_expr<int64_t>>(arg2.left->content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg2_left = as<literal_expr<int64_t>>(arg2.left->content);
      CHECK(arg2_left.value == 1);

      REQUIRE(is<binary_expr>(arg2.right->content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg2_right = as<binary_expr>(arg2.right->content);
      CHECK(arg2_right.op == binary_op::MULTIPLY);

      REQUIRE(is<literal_expr<int64_t>>(arg2_right.left->content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg2_right_left = as<literal_expr<int64_t>>(arg2_right.left->content);
      CHECK(arg2_right_left.value == 2);

      REQUIRE(is<literal_expr<int64_t>>(arg2_right.right->content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg2_right_right = as<literal_expr<int64_t>>(arg2_right.right->content);
      CHECK(arg2_right_right.value == 4);

      REQUIRE(is<literal_expr<std::string>>(args[2].content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg3 = as<literal_expr<std::string>>(args[2].content);
      CHECK(arg3.value == "test");

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("index expression (nested)") {
      // (vec << 2)[get_int_arr("test")[3] * 0]
      const vec_source source({
        {symbol::PAREN_OPEN, {}},
        {identifier{"vec"}, {}},
        {symbol::SHIFT_LEFT, {}},
        {literal<int64_t>{2}, {}},
        {symbol::PAREN_CLOSE, {}},
        {symbol::BRACKET_OPEN, {}},
        {identifier{"get_int_arr"}, {}},
        {symbol::PAREN_OPEN, {}},
        {literal<std::string>{"test"}, {}},
        {symbol::PAREN_CLOSE, {}},
        {symbol::BRACKET_OPEN, {}},
        {literal<int64_t>{3}, {}},
        {symbol::BRACKET_CLOSE, {}},
        {symbol::MULTIPLY, {}},
        {literal<int64_t>{0}, {}},
        {symbol::BRACKET_CLOSE, {}}
      });

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      REQUIRE(is<index_expr>(res->content));

      const auto &shift_expr = as<index_expr>(res->content).base;
      REQUIRE(is<binary_expr>(shift_expr->content));

      const auto &vec = as<binary_expr>(shift_expr->content).left;
      REQUIRE(is<name_expr>(vec->content));
      CHECK(as<name_expr>(vec->content).name.sections.size() == 1);
      CHECK(as<name_expr>(vec->content).name.sections[0] == "vec");

      const auto &two = as<binary_expr>(shift_expr->content).right;
      REQUIRE(is<literal_expr<int64_t>>(two->content));
      CHECK(as<literal_expr<int64_t>>(two->content).value == 2);

      const auto &multiply_expr = as<index_expr>(res->content).index;
      REQUIRE(is<binary_expr>(multiply_expr->content));

      const auto &idx_expr = as<binary_expr>(multiply_expr->content).left;
      REQUIRE(is<index_expr>(idx_expr->content));

      const auto &get_int_arr_call = as<index_expr>(idx_expr->content).base;
      REQUIRE(is<call_expr>(get_int_arr_call->content));

      const auto &get_int_arr = as<call_expr>(get_int_arr_call->content).call;
      REQUIRE(is<name_expr>(get_int_arr->content));
      CHECK(as<name_expr>(get_int_arr->content).name.sections.size() == 1);
      CHECK(as<name_expr>(get_int_arr->content).name.sections[0] == "get_int_arr");

      REQUIRE(as<call_expr>(get_int_arr_call->content).args.size() == 1);
      const auto &get_int_arr_args = as<call_expr>(get_int_arr_call->content).args;

      REQUIRE(is<literal_expr<std::string>>(get_int_arr_args[0].content));
      CHECK(as<literal_expr<std::string>>(get_int_arr_args[0].content).value == "test");

      const auto &three = as<index_expr>(idx_expr->content).index;
      REQUIRE(is<literal_expr<int64_t>>(three->content));
      CHECK(as<literal_expr<int64_t>>(three->content).value == 3);

      const auto &zero = as<binary_expr>(multiply_expr->content).right;
      REQUIRE(is<literal_expr<int64_t>>(zero->content));
      CHECK(as<literal_expr<int64_t>>(zero->content).value == 0);

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("complex expression") {
      // (look, I know it's a senseless expression, but oh well... it's a nice nested test-case)
      // make_call() ? arr[12 - other_var] * 5 : std::test(_internal::arr()[6]) ? true * 12 : false << 14;
      // TODO
      const vec_source source({
        {identifier{"make_call"}, {}}, {symbol::PAREN_OPEN, {}},
        {symbol::PAREN_CLOSE, {}}, {symbol::QUESTION, {}}, {identifier{"arr"}, {}},
        {symbol::BRACKET_OPEN, {}}, {literal<int64_t>{12}, {}},
        {symbol::MINUS, {}}, {identifier{"other_var"}, {}}, {symbol::BRACKET_CLOSE, {}},
        {symbol::MULTIPLY, {}}, {literal<int64_t>{5}, {}}, {symbol::COLON, {}},
        {identifier{"std"}, {}}, {symbol::NAMESPACE, {}}, {identifier{"test"}, {}},
        {symbol::PAREN_OPEN, {}}, {identifier{"_internal"}, {}}, {symbol::NAMESPACE, {}},
        {identifier{"arr"}, {}}, {symbol::PAREN_OPEN, {}}, {symbol::PAREN_CLOSE, {}},
        {symbol::BRACKET_OPEN, {}}, {literal<int64_t>{6}, {}}, {symbol::BRACKET_CLOSE, {}},
        {symbol::PAREN_CLOSE, {}}, {symbol::QUESTION, {}}, {literal{true}, {}},
        {symbol::MULTIPLY, {}}, {literal<int64_t>{12}, {}}, {symbol::COLON, {}},
        {literal{false}, {}}, {symbol::SHIFT_LEFT, {}}, {literal<int64_t>{14}, {}}
      });

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      REQUIRE(is<ternary_expr>(res->content));
      const auto &root = as<ternary_expr>(res->content);

      REQUIRE(is<call_expr>(root.cond->content));
      const auto &make_call = as<call_expr>(root.cond->content);
      REQUIRE(is<name_expr>(make_call.call->content));
      CHECK(as<name_expr>(make_call.call->content).name.sections.size() == 1);
      CHECK(as<name_expr>(make_call.call->content).name.sections[0] == "make_call");
      CHECK(make_call.args.empty());

      REQUIRE(is<binary_expr>(root.true_expr->content));
      const auto &true_expr = as<binary_expr>(root.true_expr->content);

      REQUIRE(is<index_expr>(true_expr.left->content));
      const auto &arr = as<index_expr>(true_expr.left->content);
      REQUIRE(is<name_expr>(arr.base->content));
      const auto &arr_name = as<name_expr>(arr.base->content);
      CHECK(arr_name.name.sections.size() == 1);
      CHECK(arr_name.name.sections[0] == "arr");

      REQUIRE(is<binary_expr>(arr.index->content));
      const auto &arr_minus = as<binary_expr>(arr.index->content);
      REQUIRE(is<literal_expr<int64_t>>(arr_minus.left->content));
      CHECK(as<literal_expr<int64_t>>(arr_minus.left->content).value == 12);

      REQUIRE(is<name_expr>(arr_minus.right->content));
      const auto &other_var = as<name_expr>(arr_minus.right->content);
      CHECK(other_var.name.sections.size() == 1);
      CHECK(other_var.name.sections[0] == "other_var");

      CHECK(arr_minus.op == binary_op::SUBTRACT);

      REQUIRE(is<literal_expr<int64_t>>(true_expr.right->content));
      CHECK(as<literal_expr<int64_t>>(true_expr.right->content).value == 5);

      CHECK(true_expr.op == binary_op::MULTIPLY);

      REQUIRE(is<ternary_expr>(root.false_expr->content));
      const auto &false_expr = as<ternary_expr>(root.false_expr->content);

      REQUIRE(is<call_expr>(false_expr.cond->content));
      const auto &std_test_call = as<call_expr>(false_expr.cond->content);
      REQUIRE(is<name_expr>(std_test_call.call->content));
      const auto &std_test = as<name_expr>(std_test_call.call->content);
      CHECK(std_test.name.sections.size() == 2);
      CHECK(std_test.name.sections[0] == "std");
      CHECK(std_test.name.sections[1] == "test");

      REQUIRE(std_test_call.args.size() == 1);
      REQUIRE(is<index_expr>(std_test_call.args[0].content));
      const auto &internal_arr_indexing = as<index_expr>(std_test_call.args[0].content);

      REQUIRE(is<call_expr>(internal_arr_indexing.base->content));
      const auto &internal_arr_call = as<call_expr>(internal_arr_indexing.base->content);
      REQUIRE(is<name_expr>(internal_arr_call.call->content));
      const auto &internal_arr = as<name_expr>(internal_arr_call.call->content);
      CHECK(internal_arr.name.sections.size() == 2);
      CHECK(internal_arr.name.sections[0] == "_internal");
      CHECK(internal_arr.name.sections[1] == "arr");
      CHECK(internal_arr_call.args.empty());

      REQUIRE(is<literal_expr<int64_t>>(internal_arr_indexing.index->content));
      CHECK(as<literal_expr<int64_t>>(internal_arr_indexing.index->content).value == 6);

      REQUIRE(is<binary_expr>(false_expr.true_expr->content));
      const auto &bool_mul_expr = as<binary_expr>(false_expr.true_expr->content);
      REQUIRE(is<literal_expr<bool>>(bool_mul_expr.left->content));
      CHECK(as<literal_expr<bool>>(bool_mul_expr.left->content).value == true); // compare to true for clarity and uniformity with other literals
      REQUIRE(is<literal_expr<int64_t>>(bool_mul_expr.right->content));
      CHECK(as<literal_expr<int64_t>>(bool_mul_expr.right->content).value == 12);

      CHECK(bool_mul_expr.op == binary_op::MULTIPLY);

      REQUIRE(is<binary_expr>(false_expr.false_expr->content));
      const auto &bool_shift_expr = as<binary_expr>(false_expr.false_expr->content);
      REQUIRE(is<literal_expr<bool>>(bool_shift_expr.left->content));
      CHECK(as<literal_expr<bool>>(bool_shift_expr.left->content).value == false); // compare to false for clarity and uniformity with other literals
      REQUIRE(is<literal_expr<int64_t>>(bool_shift_expr.right->content));
      CHECK(as<literal_expr<int64_t>>(bool_shift_expr.right->content).value == 14);

      CHECK(bool_shift_expr.op == binary_op::SHIFT_LEFT);

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("Bernouilli") {
      // 1.0f/(1 + x*x) == 1.0f/2 * (1 / (1 - i * x) + 1 / (1 + i * x))
      const vec_source source({
        {literal{1.0f}, {}}, {symbol::DIVIDE, {}}, {symbol::PAREN_OPEN, {}}, {literal<int64_t>{1}, {}},
        {symbol::PLUS, {}}, {identifier{"x"}, {}}, {symbol::MULTIPLY, {}}, {identifier{"x"}, {}},
        {symbol::PAREN_CLOSE, {}}, {symbol::EQUALS, {}}, {literal{1.0f}, {}}, {symbol::DIVIDE, {}},
        {literal<int64_t>{2}, {}}, {symbol::MULTIPLY, {}}, {symbol::PAREN_OPEN, {}}, {literal<int64_t>{1}, {}},
        {symbol::DIVIDE, {}}, {symbol::PAREN_OPEN, {}}, {literal<int64_t>{1}, {}}, {symbol::MINUS, {}},
        {identifier{"i"}, {}}, {symbol::MULTIPLY, {}}, {identifier{"x"}, {}}, {symbol::PAREN_CLOSE, {}},
        {symbol::PLUS, {}}, {literal<int64_t>{1}, {}}, {symbol::DIVIDE, {}}, {symbol::PAREN_OPEN, {}},
        {literal<int64_t>{1}, {}}, {symbol::PLUS, {}}, {identifier{"i"}, {}}, {symbol::MULTIPLY, {}},
        {identifier{"x"}, {}}, {symbol::PAREN_CLOSE, {}}, {symbol::PAREN_CLOSE, {}}
      });

      auto it = token_it(source);
      auto res = parse_expr(it);

      constexpr static auto is_bin = [](const expression &x) { return is<binary_expr>(x.content); };
      constexpr static auto as_bin = [](const expression &x) { return as<binary_expr>(x.content); };
      constexpr static auto is_i_lit = [](const expression &x) { return is<literal_expr<int64_t>>(x.content); };
      constexpr static auto as_i_lit = [](const expression &x) { return as<literal_expr<int64_t>>(x.content); };
      constexpr static auto is_f_lit = [](const expression &x) { return is<literal_expr<float>>(x.content); };
      constexpr static auto as_f_lit = [](const expression &x) { return as<literal_expr<float>>(x.content); };
      constexpr static auto is_name = [](const expression &x) { return is<name_expr>(x.content); };
      constexpr static auto as_name = [](const expression &x) { return as<name_expr>(x.content); };

      REQUIRE(res.has_value());
      REQUIRE(is_bin(*res));
      const auto &cmp_eq = as_bin(*res);
      CHECK(cmp_eq.op == binary_op::EQUAL);

      // 1.0f/(1 + x*x)
      REQUIRE(is_bin(*cmp_eq.left));
      const auto &div1 = as_bin(*cmp_eq.left);
      CHECK(div1.op == binary_op::DIVIDE);
      REQUIRE(is_f_lit(*div1.left));
      CHECK(as_f_lit(*div1.left).value == 1.0f);
      //   1 + x*x
      REQUIRE(is_bin(*div1.right));
      const auto &add1 = as_bin(*div1.right);
      CHECK(add1.op == binary_op::ADD);
      REQUIRE(is_i_lit(*add1.left));
      CHECK(as_i_lit(*add1.left).value == 1);
      //    x*x
      REQUIRE(is_bin(*add1.right));
      const auto &mul1 = as_bin(*add1.right);
      CHECK(mul1.op == binary_op::MULTIPLY);
      REQUIRE(is_name(*mul1.left));
      CHECK(as_name(*mul1.left).name.sections.size() == 1);
      CHECK(as_name(*mul1.left).name.sections[0] == "x");
      REQUIRE(is_name(*mul1.right));
      CHECK(as_name(*mul1.right).name.sections.size() == 1);
      CHECK(as_name(*mul1.right).name.sections[0] == "x");

      // 1.0f/2 * (1 / (1 - i * x) + 1 / (1 + i * x))
      REQUIRE(is_bin(*cmp_eq.right));
      const auto &mul2 = as_bin(*cmp_eq.right);
      CHECK(mul2.op == binary_op::MULTIPLY);
      //    1.0f/2
      REQUIRE(is_bin(*mul2.left));
      const auto &div2 = as_bin(*mul2.left);
      CHECK(div2.op == binary_op::DIVIDE);
      REQUIRE(is_f_lit(*div2.left));
      CHECK(as_f_lit(*div2.left).value == 1.0f);
      REQUIRE(is_i_lit(*div2.right));
      CHECK(as_i_lit(*div2.right).value == 2);
      //    (1 / (1 - i * x) + 1 / (1 + i * x))
      REQUIRE(is_bin(*mul2.right));
      const auto &add2 = as_bin(*mul2.right);
      CHECK(add2.op == binary_op::ADD);
      //        1 / (1 - i * x)
      REQUIRE(is_bin(*add2.left));
      const auto &div3 = as_bin(*add2.left);
      CHECK(div3.op == binary_op::DIVIDE);
      REQUIRE(is_i_lit(*div3.left));
      CHECK(as_i_lit(*div3.left).value == 1);
      //            1 - i * x
      REQUIRE(is_bin(*div3.right));
      const auto &sub1 = as_bin(*div3.right);
      CHECK(sub1.op == binary_op::SUBTRACT);
      REQUIRE(is_i_lit(*sub1.left));
      CHECK(as_i_lit(*sub1.left).value == 1);
      //                i * x
      REQUIRE(is_bin(*sub1.right));
      const auto &mul3 = as_bin(*sub1.right);
      CHECK(mul3.op == binary_op::MULTIPLY);
      REQUIRE(is_name(*mul3.left));
      CHECK(as_name(*mul3.left).name.sections.size() == 1);
      CHECK(as_name(*mul3.left).name.sections[0] == "i");
      REQUIRE(is_name(*mul3.right));
      CHECK(as_name(*mul3.right).name.sections.size() == 1);
      CHECK(as_name(*mul3.right).name.sections[0] == "x");
      //        1 / (1 + i * x)
      REQUIRE(is_bin(*add2.right));
      const auto &div4 = as_bin(*add2.right);
      CHECK(div4.op == binary_op::DIVIDE);
      REQUIRE(is_i_lit(*div4.left));
      CHECK(as_i_lit(*div4.left).value == 1);
      //            1 + i * x
      REQUIRE(is_bin(*div4.right));
      const auto &add3 = as_bin(*div4.right);
      CHECK(add3.op == binary_op::ADD);
      REQUIRE(is_i_lit(*add3.left));
      CHECK(as_i_lit(*add3.left).value == 1);
      //                i * x
      REQUIRE(is_bin(*add3.right));
      const auto &mul4 = as_bin(*add3.right);
      CHECK(mul4.op == binary_op::MULTIPLY);
      REQUIRE(is_name(*mul4.left));
      CHECK(as_name(*mul4.left).name.sections.size() == 1);
      CHECK(as_name(*mul4.left).name.sections[0] == "i");
      REQUIRE(is_name(*mul4.right));
      CHECK(as_name(*mul4.right).name.sections.size() == 1);
      CHECK(as_name(*mul4.right).name.sections[0] == "x");
    }
  }

  // TODO: statements

  // TODO: declarations

  // TODO: mini scripts
}

TEST_SUITE("jayc - parser (parsing fails)") {
  TEST_CASE("invalid qualified names") {
    logger.disable_throw_on_error();

    SUBCASE("no identifier") {
      const vec_source source({
        {{symbol::NAMESPACE}, {}}
      });
      auto it = token_it(source);
      const auto res = parse_qname(it);
      REQUIRE(!res.has_value());
    }

    SUBCASE("qname ends with ::") {
      const vec_source source({
        {identifier{"a"}, {"", 1, 1}},
        {symbol{symbol::NAMESPACE}, {"", 1, 2}},
        {identifier{"b"}, {"", 1, 3}},
        {symbol{symbol::NAMESPACE}, {"", 1, 3}},
        {identifier{"c"}, {"", 1, 4}},
        {symbol{symbol::NAMESPACE}, {"", 1, 4}}
      });
      auto it = token_it(source);
      const auto res = parse_qname(it);
      REQUIRE(!res.has_value());
    }
  }

  // TODO: invalid type name

  // TODO: invalid expressions

  // TODO: invalid statements

  // TODO: invalid declarations
}