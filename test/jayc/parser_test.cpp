//
// Created by jay on 9/6/24.
//

#include <string>
#include <sstream>
#include <doctest/doctest.h>
#include <algorithm>

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

  // TODO: expressions

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