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

#define START_SUBCASE logger << info{ location{}, " ------ STARTED SUBCASE ------" }

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

bool operator==(const name &n1, const name &n2) {
  if(n1.section != n2.section) return false;
  if(n1.template_args.size() != n2.template_args.size()) return false;
  for(size_t i = 0; i < n1.template_args.size(); ++i) {
    if(n1.template_args[i] != n2.template_args[i]) return false;
  }
  if(n1.next.has_value() != n2.next.has_value()) return false;
  if(n1.next.has_value() && *n1.next != *n2.next) return false;
  return n1.is_array == n2.is_array;
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

inline name single_name(const std::string &s) {
  return name{.section = s, .template_args = {}, .next = std::nullopt, .is_array = false};
}

inline name linear_name(const std::vector<std::string> &sections) {
  name res = single_name(sections[0]);
  name *ptr = &res;
  for(size_t i = 1; i < sections.size(); ++i) {
    ptr->next = name{.section = sections[i], .template_args = {}, .next = std::nullopt, .is_array = false};
    ptr = &*ptr->next;
  }
  return res;
}

TEST_SUITE("jayc - parser (parsing okay)") {
  // TODO: valid names

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
      CHECK(as<name_expr>(res->content).actual == name {
        .section = "a", .template_args = {},
        .next = name {
          .section = "b", .template_args = {},
          .next = name {
            .section = "c", .template_args = {},
            .next = std::nullopt, .is_array = false
          }, .is_array = false
        }, .is_array = false
      });

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
        {binary_op::SHIFT_RIGHT, symbol::SHIFT_RIGHT},
        {binary_op::ASSIGN, symbol::ASSIGN},
        {binary_op::ADD_ASSIGN, symbol::PLUS_ASSIGN},
        {binary_op::SUB_ASSIGN, symbol::MINUS_ASSIGN},
        {binary_op::MUL_ASSIGN, symbol::MULTIPLY_ASSIGN},
        {binary_op::DIV_ASSIGN, symbol::DIVIDE_ASSIGN},
        {binary_op::MOD_ASSIGN, symbol::MODULO_ASSIGN},
        {binary_op::BIT_AND_ASSIGN, symbol::BIT_AND_ASSIGN},
        {binary_op::BIT_OR_ASSIGN, symbol::BIT_OR_ASSIGN},
        {binary_op::XOR_ASSIGN, symbol::XOR_ASSIGN},
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
        CHECK(as<name_expr>(right.content).actual == single_name("x"));

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
      CHECK(as<name_expr>(true_expr.content).actual == single_name("x"));

      const auto &false_expr = *as<ternary_expr>(res->content).false_expr;
      CHECK(is<name_expr>(false_expr.content));
      CHECK(as<name_expr>(false_expr.content).actual == single_name("y"));

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
      CHECK(as<name_expr>(left.content).actual == single_name("x"));

      const auto &right = *as<binary_expr>(res->content).right;
      CHECK(is<name_expr>(right.content));
      CHECK(as<name_expr>(right.content).actual == single_name("y"));

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("priority 1: RTL tree (increasing priorities)") {
      /*
       * --- precedence ---
       * 1  a::b::c (via parse_qname)
       * 2 	-> 13 <e>++         <e>--         <e>()         <e>[]         <e>.a
       *          ++<e>         --<e>         +<e>          - <e>         ~<e>          !<e>
       * 3  -> 12 <e>*<e>       <e>/<e>       <e>%<e>
       * 4  -> 11 <e>+<e>       <e>-<e>
       * 5  -> 10 <e> << <e>    <e> >> <e>
       * 6  -> 9  <e> < <e>     <e> > <e>     <e> <= <e>    <e> >= <e>
       * 7  -> 8  <e> == <e>    <e> != <e>
       * 8  -> 7  <e> & <e>
       * 9  -> 6  <e> ^ <e>
       * 10 -> 5  <e> | <e>
       * 11 -> 4  <e> && <e>
       * 12 -> 3  <e> || <e>
       * 13 -> 2  <e> ? <e> : <e>
       * 14 -> 1  <e> = <e>     <e> += <e>    <e> -= <e>    <e> *= <e>    <e> /= <e>    <e> %= <e>
       *          <e> &= <e>    <e> |= <e>    <e> ^= <e>
       */

      const static auto i = [](const int x) {
        return token{literal<int64_t>{x}, {}};
      };
      const static auto o = [](const symbol t) {
        return token{t, {}};
      };

      using enum symbol;
      const vec_source source({
        i(0), o(ASSIGN), i(1), o(OR), i(2), o(AND), i(3), o(BIT_OR), i(4), o(XOR), i(5),
        o(BIT_AND), i(6), o(NOT_EQUALS), i(7), o(GREATER_THAN), i(8),
        o(SHIFT_RIGHT), i(9), o(PLUS), i(10), o(MULTIPLY), i(11)
      });

      const std::vector<std::pair<binary_op, int>> expected_sequence = {
        {binary_op::ASSIGN, 0},
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
        o(EQUALS), i(6), o(BIT_AND), i(7), o(XOR), i(8), o(BIT_OR), i(9), o(AND), i(10), o(OR), i(11),
        o(BIT_AND_ASSIGN), i(12)
      });

      const std::vector<std::pair<binary_op, int>> expected_sequence = {
        {binary_op::BIT_AND_ASSIGN, 12},
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
      CHECK(name.actual == single_name("a"));
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
      CHECK(name.actual == single_name("a"));
      CHECK(args.size() == 1);
      CHECK(is<name_expr>(args[0].content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg = as<name_expr>(args[0].content);
      CHECK(arg.actual == single_name("b"));

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
      CHECK(name.actual == single_name("method"));
      CHECK(args.size() == 3);

      REQUIRE(is<name_expr>(args[0].content));
      // ReSharper disable once CppUseStructuredBinding
      const auto &arg1 = as<name_expr>(args[0].content);
      CHECK(arg1.actual == linear_name({"arg", "no1"}));

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
      CHECK(as<name_expr>(vec->content).actual == single_name("vec"));

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
      CHECK(as<name_expr>(get_int_arr->content).actual == single_name("get_int_arr"));

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

    SUBCASE("member-call-member") {
      const vec_source source({
        {identifier{"cls"}, {}}, {symbol::DOT, {}}, {identifier{"method"}, {}},
        {symbol::PAREN_OPEN, {}}, {identifier{"arg"}, {}}, {symbol::PAREN_CLOSE, {}},
        {symbol::DOT, {}}, {identifier{"field"}, {}}
      });

      auto it = token_it(source);
      auto res = parse_expr(it);
      REQUIRE(res.has_value());
      REQUIRE(is<member_expr>(res->content));
      const auto &member1 = as<member_expr>(res->content);
      CHECK(member1.member == "field");
      REQUIRE(is<call_expr>(member1.base->content));
      const auto &call = as<call_expr>(member1.base->content);
      REQUIRE(is<member_expr>(call.call->content));
      const auto &member2 = as<member_expr>(call.call->content);
      CHECK(member2.member == "method");
      REQUIRE(is<name_expr>(member2.base->content));
      CHECK(as<name_expr>(member2.base->content).actual == single_name("cls"));
      REQUIRE(call.args.size() == 1);
      REQUIRE(is<name_expr>(call.args[0].content));
      CHECK(as<name_expr>(call.args[0].content).actual == single_name("arg"));
    }

    SUBCASE("complex expression") {
      START_SUBCASE;
      // (look, I know it's a senseless expression, but oh well... it's a nice nested test-case)
      // make_call() ? arr[12 - other_var] * 5 : std::test(_internal::arr()[6]) ? true * 12 : false << 14;
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
      CHECK(as<name_expr>(make_call.call->content).actual == single_name("make_call"));
      CHECK(make_call.args.empty());

      REQUIRE(is<binary_expr>(root.true_expr->content));
      const auto &true_expr = as<binary_expr>(root.true_expr->content);

      REQUIRE(is<index_expr>(true_expr.left->content));
      const auto &arr = as<index_expr>(true_expr.left->content);
      REQUIRE(is<name_expr>(arr.base->content));
      const auto &arr_name = as<name_expr>(arr.base->content);
      CHECK(arr_name.actual == single_name("arr"));

      REQUIRE(is<binary_expr>(arr.index->content));
      const auto &arr_minus = as<binary_expr>(arr.index->content);
      REQUIRE(is<literal_expr<int64_t>>(arr_minus.left->content));
      CHECK(as<literal_expr<int64_t>>(arr_minus.left->content).value == 12);

      REQUIRE(is<name_expr>(arr_minus.right->content));
      const auto &other_var = as<name_expr>(arr_minus.right->content);
      CHECK(other_var.actual == single_name("other_var"));

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
      CHECK(std_test.actual == linear_name({"std", "test"}));

      REQUIRE(std_test_call.args.size() == 1);
      REQUIRE(is<index_expr>(std_test_call.args[0].content));
      const auto &internal_arr_indexing = as<index_expr>(std_test_call.args[0].content);

      REQUIRE(is<call_expr>(internal_arr_indexing.base->content));
      const auto &internal_arr_call = as<call_expr>(internal_arr_indexing.base->content);
      REQUIRE(is<name_expr>(internal_arr_call.call->content));
      const auto &internal_arr = as<name_expr>(internal_arr_call.call->content);
      CHECK(internal_arr.actual == linear_name({"_internal", "arr"}));
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
      CHECK(as_name(*mul1.left).actual == single_name("x"));
      REQUIRE(is_name(*mul1.right));
      CHECK(as_name(*mul1.right).actual == single_name("x"));

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
      CHECK(as_name(*mul3.left).actual == single_name("i"));
      REQUIRE(is_name(*mul3.right));
      CHECK(as_name(*mul3.right).actual == single_name("x"));
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
      CHECK(as_name(*mul4.left).actual == single_name("i"));
      REQUIRE(is_name(*mul4.right));
      CHECK(as_name(*mul4.right).actual == single_name("x"));
    }
  }

  TEST_CASE("valid statements") {
    logger.enable_throw_on_error();
    SUBCASE("trivial statements") {
      const vec_source source({
        {keyword::BREAK, {}}, {symbol::SEMI, {}},
        {keyword::CONTINUE, {}}, {symbol::SEMI, {}},
        {keyword::RETURN, {}}, {symbol::SEMI, {}},
        {keyword::RETURN, {}}, {identifier{"x"}, {}}, {symbol::SEMI, {}}
      });

      // #1 -> break;
      auto it = token_it(source);
      auto res = parse_stmt(it);
      REQUIRE(res.has_value());
      CHECK(is<break_stmt>(res->content));
      CHECK(is<keyword>(it->actual));
      CHECK(as<keyword>(it->actual) == keyword::CONTINUE);

      // #2 -> continue;
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      CHECK(is<continue_stmt>(res->content));
      CHECK(is<keyword>(it->actual));
      CHECK(as<keyword>(it->actual) == keyword::RETURN);

      // #3 -> return;
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<return_stmt>(res->content));
      CHECK_FALSE(as<return_stmt>(res->content).value.has_value());
      CHECK(is<keyword>(it->actual));
      CHECK(as<keyword>(it->actual) == keyword::RETURN);

      // #4 -> return x;
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<return_stmt>(res->content));
      CHECK(as<return_stmt>(res->content).value.has_value());
      const auto &ret_val = *as<return_stmt>(res->content).value;
      REQUIRE(is<name_expr>(ret_val.content));
      CHECK(as<name_expr>(ret_val.content).actual == single_name("x"));
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("statement block") {
      const vec_source source({
        {symbol::BRACE_OPEN, {}}, {symbol::BRACE_CLOSE, {}},
        {symbol::BRACE_OPEN, {}}, {keyword::BREAK, {}}, {symbol::SEMI, {}}, {symbol::BRACE_CLOSE, {}}
      });

      auto it = token_it(source);

      // #1 -> {}
      auto res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<block>(res->content));
      CHECK(as<block>(res->content).statements.empty());
      REQUIRE(is<symbol>(it->actual));
      CHECK(as<symbol>(it->actual) == symbol::BRACE_OPEN);

      // #2 -> { break; }
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<block>(res->content));
      CHECK(as<block>(res->content).statements.size() == 1);
      const auto &first = as<block>(res->content).statements[0];
      CHECK(is<break_stmt>(first.content));
      CHECK(is<eof>(it->actual));
    }

    SUBCASE("expression statements") {
      const vec_source source({
        {identifier{"method"}, {}}, {symbol::PAREN_OPEN, {}}, {symbol::PAREN_CLOSE, {}}, {symbol::SEMI, {}},
        {literal<int64_t>{12}, {}}, {symbol::SEMI, {}},
        {literal<float>{12.5f}, {}}, {symbol::EQUALS, {}}, {literal<double>{25.0}, {}}, {symbol::DIVIDE, {}}, {literal<int64_t>{2}, {}}, {symbol::SEMI, {}}
      });

      auto it = token_it(source);

      // #1 -> method();
      auto res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<expr_stmt>(res->content));
      const auto &expr1 = as<expr_stmt>(res->content).expr;
      REQUIRE(is<call_expr>(expr1.content));
      const auto &call1 = as<call_expr>(expr1.content);
      REQUIRE(is<name_expr>(call1.call->content));
      CHECK(as<name_expr>(call1.call->content).actual == single_name("method"));
      CHECK(call1.args.empty());

      // #2 -> 12;
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<expr_stmt>(res->content));
      const auto &expr2 = as<expr_stmt>(res->content).expr;
      REQUIRE(is<literal_expr<int64_t>>(expr2.content));
      CHECK(as<literal_expr<int64_t>>(expr2.content).value == 12);

      // #3 -> 12.5f == 25.0 / 2;
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<expr_stmt>(res->content));
      const auto &expr3 = as<expr_stmt>(res->content).expr;
      REQUIRE(is<binary_expr>(expr3.content));
      const auto &equal = as<binary_expr>(expr3.content);
      CHECK(equal.op == binary_op::EQUAL);
      REQUIRE(is<literal_expr<float>>(equal.left->content));
      CHECK(as<literal_expr<float>>(equal.left->content).value == 12.5f);
      REQUIRE(is<binary_expr>(equal.right->content));
      const auto &div = as<binary_expr>(equal.right->content);
      CHECK(div.op == binary_op::DIVIDE);
      REQUIRE(is<literal_expr<double>>(div.left->content));
      CHECK(as<literal_expr<double>>(div.left->content).value == 25.0);
      REQUIRE(is<literal_expr<int64_t>>(div.right->content));
      CHECK(as<literal_expr<int64_t>>(div.right->content).value == 2);

      CHECK(is<eof>(it->actual));
    }

    // TODO: add typed variables, var vs val
    SUBCASE("var declaration statements") {
      const vec_source source({
        {keyword::VAR, {}}, {identifier{"x"}, {}}, {symbol::ASSIGN, {}}, {literal<int64_t>{42}, {}}, {symbol::SEMI, {}}
      });

      auto it = token_it(source);
      // var x = 42;
      auto res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<var_decl_stmt>(res->content));
      const auto &var_decl = as<var_decl_stmt>(res->content);
      CHECK(var_decl.var_name == "x");
      REQUIRE(is<literal_expr<int64_t>>(var_decl.value.content));
      CHECK(as<literal_expr<int64_t>>(var_decl.value.content).value == 42);
    }

    SUBCASE("assignment statements (normal and operator-based)") {
      const vec_source source({
        {identifier{"x"}, {}}, {symbol::ASSIGN, {}}, {literal<int64_t>{42}, {}}, {symbol::SEMI, {}},
        {identifier{"arr"}, {}}, {symbol::BRACKET_OPEN, {}}, {literal<int64_t>{3}, {}}, {symbol::BRACKET_CLOSE, {}}, {symbol::MULTIPLY_ASSIGN, {}}, {literal<int64_t>{0}, {}}, {symbol::SEMI, {}}
      });

      auto it = token_it(source);

      // #1 -> x = 42;
      auto res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<expr_stmt>(res->content));
      const auto &assign1 = as<expr_stmt>(res->content);
      REQUIRE(is<binary_expr>(assign1.expr.content));
      const auto &expr1 = as<binary_expr>(assign1.expr.content);
      CHECK(expr1.op == binary_op::ASSIGN);
      REQUIRE(is<name_expr>(expr1.left->content));
      CHECK(as<name_expr>(expr1.left->content).actual == single_name("x"));
      REQUIRE(is<literal_expr<int64_t>>(expr1.right->content));
      CHECK(as<literal_expr<int64_t>>(expr1.right->content).value == 42);

      // #2 -> arr[3] *= 0;
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<expr_stmt>(res->content));
      const auto &assign2 = as<expr_stmt>(res->content).expr;
      REQUIRE(is<binary_expr>(assign2.content));
      const auto &expr2 = as<binary_expr>(assign2.content);
      REQUIRE(is<index_expr>(expr2.left->content));
      const auto &idx_expr = as<index_expr>(expr2.left->content);
      REQUIRE(is<name_expr>(idx_expr.base->content));
      CHECK(as<name_expr>(idx_expr.base->content).actual == single_name("arr"));
      REQUIRE(is<literal_expr<int64_t>>(idx_expr.index->content));
      CHECK(as<literal_expr<int64_t>>(idx_expr.index->content).value == 3);
      REQUIRE(is<literal_expr<int64_t>>(expr2.right->content));
      CHECK(as<literal_expr<int64_t>>(expr2.right->content).value == 0);
      CHECK(expr2.op == binary_op::MUL_ASSIGN);

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("if-else statements") {
      location last = {"", 0, 0};
      static auto pos = [&last] {
        last.col++;
        return last;
      };
      static auto line = [&last] {
        last.line++;
        last.col = 0;
        return last;
      };

      const vec_source source({
        // if(true) {}
        {keyword::IF, line()}, {symbol::PAREN_OPEN, pos()}, {literal<bool>{true}, pos()},
        {symbol::PAREN_CLOSE, pos()}, {symbol::BRACE_OPEN, pos()}, {symbol::BRACE_CLOSE, pos()},

        // if(false) do_test();
        {keyword::IF, line()}, {symbol::PAREN_OPEN, pos()}, {literal<bool>{false}, pos()},
        {symbol::PAREN_CLOSE, pos()}, {identifier{"do_test"}}, {symbol::PAREN_OPEN, pos()},
        {symbol::PAREN_CLOSE, pos()}, {symbol::SEMI, pos()},

        // if(x == y) return x; else { do_something(); }
        {keyword::IF, line()}, {symbol::PAREN_OPEN, pos()}, {identifier{"x"}, pos()},
        {symbol::EQUALS, pos()}, {identifier{"y"}, pos()}, {symbol::PAREN_CLOSE, pos()},
        {keyword::RETURN, pos()}, {identifier{"x"}, pos()}, {symbol::SEMI, pos()},
        {keyword::ELSE, pos()}, {symbol::BRACE_OPEN, pos()}, {identifier{"do_something"}, pos()},
        {symbol::PAREN_OPEN, pos()}, {symbol::PAREN_CLOSE, pos()}, {symbol::SEMI, pos()},
        {symbol::BRACE_CLOSE, pos()},

        // if(x != y) { do_something(); } else return y;
        {keyword::IF, line()}, {symbol::PAREN_OPEN, pos()}, {identifier{"x"}, pos()},
        {symbol::NOT_EQUALS, pos()}, {identifier{"y"}, pos()}, {symbol::PAREN_CLOSE, pos()},
        {symbol::BRACE_OPEN, pos()}, {identifier{"do_something"}, pos()}, {symbol::PAREN_OPEN, pos()},
        {symbol::PAREN_CLOSE, pos()}, {symbol::SEMI, pos()}, {symbol::BRACE_CLOSE, pos()},
        {keyword::ELSE, pos()}, {keyword::RETURN, pos()}, {identifier{"y"}, pos()}, {symbol::SEMI, pos()},

        // if(a) b(); else if(c) d(); else e();
        {keyword::IF, line()}, {symbol::PAREN_OPEN, pos()}, {identifier{"a"}, pos()},
        {symbol::PAREN_CLOSE, pos()}, {identifier{"b"}, pos()}, {symbol::PAREN_OPEN, pos()},
        {symbol::PAREN_CLOSE, pos()}, {symbol::SEMI, pos()}, {keyword::ELSE, pos()},
        {keyword::IF, pos()}, {symbol::PAREN_OPEN, pos()}, {identifier{"c"}, pos()},
        {symbol::PAREN_CLOSE, pos()}, {identifier{"d"}, pos()}, {symbol::PAREN_OPEN, pos()},
        {symbol::PAREN_CLOSE, pos()}, {symbol::SEMI, pos()},{keyword::ELSE, pos()},
        {identifier{"e"}, pos()}, {symbol::PAREN_OPEN, pos()}, {symbol::PAREN_CLOSE, pos()},
        {symbol::SEMI, pos()},

        // if(a) if(b) c(); else d();
        {keyword::IF, line()}, {symbol::PAREN_OPEN, pos()}, {identifier{"a"}, pos()},
        {symbol::PAREN_CLOSE, pos()}, {keyword::IF, pos()}, {symbol::PAREN_OPEN, pos()},
        {identifier{"b"}, pos()}, {symbol::PAREN_CLOSE, pos()}, {identifier{"c"}, pos()},
        {symbol::PAREN_OPEN, pos()}, {symbol::PAREN_CLOSE, pos()}, {symbol::SEMI, pos()},
        {keyword::ELSE, pos()}, {identifier{"d"}, pos()}, {symbol::PAREN_OPEN, pos()},
        {symbol::PAREN_CLOSE, pos()}, {symbol::SEMI, pos()}
      });

      auto it = token_it(source);

      // #1 -> if(true) {}
      auto res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<if_stmt>(res->content));
      const auto &if1 = as<if_stmt>(res->content);
      REQUIRE(is<literal_expr<bool>>(if1.condition.content));
      CHECK(as<literal_expr<bool>>(if1.condition.content).value == true);
      REQUIRE(is<block>(if1.true_block->content));
      CHECK(as<block>(if1.true_block->content).statements.empty());
      CHECK(!if1.false_block.has_value());

      // #2 -> if(false) do_test();
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<if_stmt>(res->content));
      const auto &if2 = as<if_stmt>(res->content);
      REQUIRE(is<literal_expr<bool>>(if2.condition.content));
      CHECK(as<literal_expr<bool>>(if2.condition.content).value == false);
      REQUIRE(is<expr_stmt>(if2.true_block->content));
      const auto &true1 = as<expr_stmt>(if2.true_block->content);
      REQUIRE(is<call_expr>(true1.expr.content));
      const auto &call1 = as<call_expr>(true1.expr.content);
      REQUIRE(is<name_expr>(call1.call->content));
      CHECK(as<name_expr>(call1.call->content).actual == single_name("do_test"));
      CHECK(call1.args.empty());
      CHECK(!if2.false_block.has_value());

      // #3 -> if(x == y) return x; else { do_something(); }
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<if_stmt>(res->content));
      const auto &if3 = as<if_stmt>(res->content);
      REQUIRE(is<binary_expr>(if3.condition.content));
      const auto &eq = as<binary_expr>(if3.condition.content);
      CHECK(eq.op == binary_op::EQUAL);
      REQUIRE(is<name_expr>(eq.left->content));
      CHECK(as<name_expr>(eq.left->content).actual == single_name("x"));
      REQUIRE(is<name_expr>(eq.right->content));
      CHECK(as<name_expr>(eq.right->content).actual == single_name("y"));
      REQUIRE(is<return_stmt>(if3.true_block->content));
      const auto &ret1 = as<return_stmt>(if3.true_block->content);
      REQUIRE(ret1.value.has_value());
      REQUIRE(is<name_expr>(ret1.value->content));
      CHECK(as<name_expr>(ret1.value->content).actual == single_name("x"));
      REQUIRE(if3.false_block.has_value());
      REQUIRE(is<block>(if3.false_block.value()->content));
      const auto &false1 = as<block>(if3.false_block.value()->content);
      REQUIRE(false1.statements.size() == 1);
      REQUIRE(is<expr_stmt>(false1.statements[0].content));
      const auto &expr1 = as<expr_stmt>(false1.statements[0].content);
      REQUIRE(is<call_expr>(expr1.expr.content));
      const auto &call2 = as<call_expr>(expr1.expr.content);
      REQUIRE(is<name_expr>(call2.call->content));
      CHECK(as<name_expr>(call2.call->content).actual == single_name("do_something"));
      CHECK(call2.args.empty());

      // #4 -> if(x != y) { do_something(); } else return y;
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<if_stmt>(res->content));
      const auto &if4 = as<if_stmt>(res->content);
      REQUIRE(is<binary_expr>(if4.condition.content));
      const auto &neq = as<binary_expr>(if4.condition.content);
      CHECK(neq.op == binary_op::NOT_EQUAL);
      REQUIRE(is<name_expr>(neq.left->content));
      CHECK(as<name_expr>(neq.left->content).actual == single_name("x"));
      REQUIRE(is<name_expr>(neq.right->content));
      CHECK(as<name_expr>(neq.right->content).actual == single_name("y"));
      REQUIRE(is<block>(if4.true_block->content));
      const auto &true2 = as<block>(if4.true_block->content);
      REQUIRE(true2.statements.size() == 1);
      REQUIRE(is<expr_stmt>(true2.statements[0].content));
      const auto &expr2 = as<expr_stmt>(true2.statements[0].content);
      REQUIRE(is<call_expr>(expr2.expr.content));
      const auto &call3 = as<call_expr>(expr2.expr.content);
      REQUIRE(is<name_expr>(call3.call->content));
      CHECK(as<name_expr>(call3.call->content).actual == single_name("do_something"));
      CHECK(call3.args.empty());
      REQUIRE(if4.false_block.has_value());
      REQUIRE(is<return_stmt>(if4.false_block.value()->content));
      const auto &ret2 = as<return_stmt>(if4.false_block.value()->content);
      REQUIRE(ret2.value.has_value());
      REQUIRE(is<name_expr>(ret2.value->content));
      CHECK(as<name_expr>(ret2.value->content).actual == single_name("y"));

      // #5 -> if(a) b(); else if(c) d(); else e();
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<if_stmt>(res->content));
      const auto &if5 = as<if_stmt>(res->content);
      REQUIRE(is<name_expr>(if5.condition.content));
      CHECK(as<name_expr>(if5.condition.content).actual == single_name("a"));
      REQUIRE(is<expr_stmt>(if5.true_block->content));
      const auto &true3 = as<expr_stmt>(if5.true_block->content);
      REQUIRE(is<call_expr>(true3.expr.content));
      const auto &call4 = as<call_expr>(true3.expr.content);
      REQUIRE(is<name_expr>(call4.call->content));
      CHECK(as<name_expr>(call4.call->content).actual == single_name("b"));
      CHECK(call4.args.empty());
      CAPTURE(*it);
      REQUIRE(if5.false_block.has_value());
      REQUIRE(is<if_stmt>(if5.false_block.value()->content));
      const auto &if6 = as<if_stmt>(if5.false_block.value()->content);
      REQUIRE(is<name_expr>(if6.condition.content));
      CHECK(as<name_expr>(if6.condition.content).actual == single_name("c"));
      REQUIRE(is<expr_stmt>(if6.true_block->content));
      const auto &true4 = as<expr_stmt>(if6.true_block->content);
      REQUIRE(is<call_expr>(true4.expr.content));
      const auto &call5 = as<call_expr>(true4.expr.content);
      REQUIRE(is<name_expr>(call5.call->content));
      CHECK(as<name_expr>(call5.call->content).actual == single_name("d"));
      CHECK(call5.args.empty());
      REQUIRE(if6.false_block.has_value());
      REQUIRE(is<expr_stmt>(if6.false_block.value()->content));
      const auto &false2 = as<expr_stmt>(if6.false_block.value()->content);
      REQUIRE(is<call_expr>(false2.expr.content));
      const auto &call6 = as<call_expr>(false2.expr.content);
      REQUIRE(is<name_expr>(call6.call->content));
      CHECK(as<name_expr>(call6.call->content).actual == single_name("e"));
      CHECK(call6.args.empty());

      // #6 -> if(a) if(b) c(); else d();
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<if_stmt>(res->content));
      const auto &if7 = as<if_stmt>(res->content);
      REQUIRE(is<name_expr>(if7.condition.content));
      CHECK(as<name_expr>(if7.condition.content).actual == single_name("a"));
      REQUIRE(is<if_stmt>(if7.true_block->content));
      const auto &if8 = as<if_stmt>(if7.true_block->content);
      REQUIRE(is<name_expr>(if8.condition.content));
      CHECK(as<name_expr>(if8.condition.content).actual == single_name("b"));
      REQUIRE(is<expr_stmt>(if8.true_block->content));
      const auto &true5 = as<expr_stmt>(if8.true_block->content);
      REQUIRE(is<call_expr>(true5.expr.content));
      const auto &call7 = as<call_expr>(true5.expr.content);
      REQUIRE(is<name_expr>(call7.call->content));
      CHECK(as<name_expr>(call7.call->content).actual == single_name("c"));
      CHECK(call7.args.empty());
      REQUIRE(if8.false_block.has_value());
      REQUIRE(is<expr_stmt>(if8.false_block.value()->content));
      const auto &false3 = as<expr_stmt>(if8.false_block.value()->content);
      REQUIRE(is<call_expr>(false3.expr.content));
      const auto &call8 = as<call_expr>(false3.expr.content);
      REQUIRE(is<name_expr>(call8.call->content));
      CHECK(as<name_expr>(call8.call->content).actual == single_name("d"));
      CHECK(call8.args.empty());
      CHECK(!if7.false_block.has_value());

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("for vs for-each statements") {
      const vec_source source({
        // for(var x = 1; x < 10; x *= 10) call();
        {keyword::FOR, {}}, {symbol::PAREN_OPEN, {}}, {keyword::VAR, {}},
        {identifier{"x"}, {}}, {symbol::ASSIGN, {}}, {literal<int64_t>{1}, {}},
        {symbol::SEMI, {}}, {identifier{"x"}, {}}, {symbol::LESS_THAN, {}},
        {literal<int64_t>{10}, {}}, {symbol::SEMI, {}}, {identifier{"x"}, {}},
        {symbol::MULTIPLY_ASSIGN, {}}, {literal<int64_t>{10}, {}}, {symbol::PAREN_CLOSE, {}},
        {identifier{"call"}, {}},{symbol::PAREN_OPEN, {}}, {symbol::PAREN_CLOSE, {}},
        {symbol::SEMI, {}},

        // for(var x = 0; x < 10; x++) arr_check(arr[i]);
        {keyword::FOR, {}}, {symbol::PAREN_OPEN, {}}, {keyword::VAR, {}},
        {identifier{"x"}, {}}, {symbol::ASSIGN, {}}, {literal<int64_t>{0}, {}},
        {symbol::SEMI, {}}, {identifier{"x"}, {}}, {symbol::LESS_THAN, {}},
        {literal<int64_t>{10}, {}}, {symbol::SEMI, {}}, {identifier{"x"}, {}},
        {symbol::INCREMENT, {}}, {symbol::PAREN_CLOSE, {}}, {identifier{"arr_check"}, {}},
        {symbol::PAREN_OPEN, {}}, {identifier{"arr"}, {}}, {symbol::BRACKET_OPEN, {}},
        {identifier{"i"}, {}}, {symbol::BRACKET_CLOSE, {}}, {symbol::PAREN_CLOSE, {}},
        {symbol::SEMI, {}},

        // for(x: arr) arr_check(x);
        {keyword::FOR, {}}, {symbol::PAREN_OPEN, {}}, {identifier{"x"}, {}},
        {symbol::COLON, {}}, {identifier{"arr"}, {}}, {symbol::PAREN_CLOSE, {}},
        {identifier{"arr_check"}, {}}, {symbol::PAREN_OPEN, {}}, {identifier{"x"}, {}},
        {symbol::PAREN_CLOSE, {}}, {symbol::SEMI, {}}
      });

      auto it = token_it(source);

      // #1 -> for(var x = 1; x < 10; x *= 10) call();
      auto res = parse_stmt(it);
      CAPTURE(*it);
      REQUIRE(res.has_value());
      REQUIRE(is<for_stmt>(res->content));
      const auto &for1 = as<for_stmt>(res->content);
      REQUIRE(is<var_decl_stmt>(for1.init->content));
      const auto &init1 = as<var_decl_stmt>(for1.init->content);
      CHECK(init1.var_name == "x");
      REQUIRE(is<literal_expr<int64_t>>(init1.value.content));
      CHECK(as<literal_expr<int64_t>>(init1.value.content).value == 1);
      REQUIRE(is<binary_expr>(for1.condition.content));
      const auto &cond1 = as<binary_expr>(for1.condition.content);
      CHECK(cond1.op == binary_op::LESS);
      REQUIRE(is<name_expr>(cond1.left->content));
      CHECK(as<name_expr>(cond1.left->content).actual == single_name("x"));
      REQUIRE(is<literal_expr<int64_t>>(cond1.right->content));
      CHECK(as<literal_expr<int64_t>>(cond1.right->content).value == 10);
      REQUIRE(is<binary_expr>(for1.update.content));
      const auto &upd1 = as<binary_expr>(for1.update.content);
      CHECK(upd1.op == binary_op::MUL_ASSIGN);
      REQUIRE(is<name_expr>(upd1.left->content));
      CHECK(as<name_expr>(upd1.left->content).actual == single_name("x"));
      REQUIRE(is<literal_expr<int64_t>>(upd1.right->content));
      CHECK(as<literal_expr<int64_t>>(upd1.right->content).value == 10);
      REQUIRE(is<expr_stmt>(for1.block->content));
      const auto &block1 = as<expr_stmt>(for1.block->content);
      REQUIRE(is<call_expr>(block1.expr.content));
      const auto &call1 = as<call_expr>(block1.expr.content);
      REQUIRE(is<name_expr>(call1.call->content));
      CHECK(as<name_expr>(call1.call->content).actual == single_name("call"));
      CHECK(call1.args.empty());

      // #2 -> for(var x = 0; x < 10; x++) arr_check(arr[i]);
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<for_stmt>(res->content));
      const auto &for2 = as<for_stmt>(res->content);
      REQUIRE(is<var_decl_stmt>(for2.init->content));
      const auto &init2 = as<var_decl_stmt>(for2.init->content);
      CHECK(init2.var_name == "x");
      REQUIRE(is<literal_expr<int64_t>>(init2.value.content));
      CHECK(as<literal_expr<int64_t>>(init2.value.content).value == 0);
      REQUIRE(is<binary_expr>(for2.condition.content));
      const auto &cond2 = as<binary_expr>(for2.condition.content);
      CHECK(cond2.op == binary_op::LESS);
      REQUIRE(is<name_expr>(cond2.left->content));
      CHECK(as<name_expr>(cond2.left->content).actual == single_name("x"));
      REQUIRE(is<literal_expr<int64_t>>(cond2.right->content));
      CHECK(as<literal_expr<int64_t>>(cond2.right->content).value == 10);
      REQUIRE(is<unary_expr>(for2.update.content));
      const auto &upd2 = as<unary_expr>(for2.update.content);
      CHECK(upd2.op == unary_op::POST_INCR);
      REQUIRE(is<expr_stmt>(for2.block->content));
      const auto &block2 = as<expr_stmt>(for2.block->content);
      REQUIRE(is<call_expr>(block2.expr.content));
      const auto &call2 = as<call_expr>(block2.expr.content);
      REQUIRE(is<name_expr>(call2.call->content));
      CHECK(as<name_expr>(call2.call->content).actual == single_name("arr_check"));
      REQUIRE(call2.args.size() == 1);
      REQUIRE(is<index_expr>(call2.args[0].content));
      const auto &idx_expr = as<index_expr>(call2.args[0].content);
      REQUIRE(is<name_expr>(idx_expr.base->content));
      CHECK(as<name_expr>(idx_expr.base->content).actual == single_name("arr"));
      REQUIRE(is<name_expr>(idx_expr.index->content));
      CHECK(as<name_expr>(idx_expr.index->content).actual == single_name("i"));

      // #3 -> for(x: arr) arr_check(x);
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<for_each_stmt>(res->content));
      const auto &for3 = as<for_each_stmt>(res->content);
      CHECK(for3.binding == "x");
      REQUIRE(is<name_expr>(for3.collection.content));
      CHECK(as<name_expr>(for3.collection.content).actual == single_name("arr"));
      REQUIRE(is<expr_stmt>(for3.block->content));
      const auto &block3 = as<expr_stmt>(for3.block->content);
      REQUIRE(is<call_expr>(block3.expr.content));
      const auto &call3 = as<call_expr>(block3.expr.content);
      REQUIRE(is<name_expr>(call3.call->content));
      CHECK(as<name_expr>(call3.call->content).actual == single_name("arr_check"));
      REQUIRE(call3.args.size() == 1);
      REQUIRE(is<name_expr>(call3.args[0].content));

      CHECK(is<eof>(it->actual));
    }

    SUBCASE("while and do-while statements") {
      const vec_source source({
        // while(true) break;
        {keyword::WHILE, {}}, {symbol::PAREN_OPEN, {}}, {literal<bool>{true}, {}},
        {symbol::PAREN_CLOSE, {}}, {keyword::BREAK, {}}, {symbol::SEMI, {}},

        // do { continue; } while(false);
        {keyword::DO, {}}, {symbol::BRACE_OPEN, {}}, {keyword::CONTINUE, {}},
        {symbol::SEMI, {}}, {symbol::BRACE_CLOSE, {}}, {keyword::WHILE, {}},
        {symbol::PAREN_OPEN, {}}, {literal<bool>{false}, {}}, {symbol::PAREN_CLOSE, {}},
        {symbol::SEMI, {}},

        // do return; while(false);
        {keyword::DO, {}}, {keyword::RETURN, {}}, {symbol::SEMI, {}}, {keyword::WHILE, {}},
        {symbol::PAREN_OPEN, {}}, {literal<bool>{false}, {}}, {symbol::PAREN_CLOSE, {}},
        {symbol::SEMI, {}}
      });

      // #1 -> while(true) break;
      auto it = token_it(source);
      auto res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<while_stmt>(res->content));
      const auto &while1 = as<while_stmt>(res->content);
      REQUIRE(is<literal_expr<bool>>(while1.condition.content));
      CHECK(as<literal_expr<bool>>(while1.condition.content).value == true);
      REQUIRE(is<break_stmt>(while1.block->content));
      CHECK(!while1.is_do_while);

      // #2 -> do { continue; } while(false);
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<while_stmt>(res->content));
      const auto &while2 = as<while_stmt>(res->content);
      REQUIRE(is<block>(while2.block->content));
      const auto &block1 = as<block>(while2.block->content);
      REQUIRE(block1.statements.size() == 1);
      REQUIRE(is<continue_stmt>(block1.statements[0].content));
      REQUIRE(is<literal_expr<bool>>(while2.condition.content));
      CHECK(as<literal_expr<bool>>(while2.condition.content).value == false);
      CHECK(while2.is_do_while);

      // #3 -> do return; while(false);
      res = parse_stmt(it);
      REQUIRE(res.has_value());
      REQUIRE(is<while_stmt>(res->content));
      const auto &while3 = as<while_stmt>(res->content);
      REQUIRE(is<return_stmt>(while3.block->content));
      const auto &ret = as<return_stmt>(while3.block->content);
      CHECK(!ret.value.has_value());
      REQUIRE(is<literal_expr<bool>>(while3.condition.content));
      CHECK(as<literal_expr<bool>>(while3.condition.content).value == false);
      CHECK(while3.is_do_while);

      CHECK(is<eof>(it->actual));
    }
  }

  // TODO: declarations

  // TODO: mini scripts
}

TEST_SUITE("jayc - parser (parsing fails)") {
  // TODO: invalid names

  // TODO: invalid expressions

  // TODO: invalid statements

  // TODO: invalid declarations
}
