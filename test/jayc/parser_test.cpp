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

  [[nodiscard]] bool is_eof() const { return idx == tokens.size(); }

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
    logger.enable_throw_on_error();

    SUBCASE("single identifier") {
      const vec_source source({
        {identifier{"a"}, {"", 1, 1}}
      });
      auto it = token_it(source);
      const auto res = parse_qname(it);
      REQUIRE(res.has_value());
      REQUIRE(res->sections.size() == 1);
      CHECK(res->sections[0] == "a");
    }

    SUBCASE("multiple identifiers") {
      const vec_source source({
        {identifier{"a"}, {"", 1, 1}},
        {symbol{symbol::NAMESPACE}, {"", 1, 2}},
        {identifier{"b"}, {"", 1, 3}},
        {symbol{symbol::NAMESPACE}, {"", 1, 4}},
        {identifier{"c"}, {"", 1, 5}}
      });
      auto it = token_it(source);
      const auto res = parse_qname(it);
      REQUIRE(res.has_value());
      CAPTURE(res->sections);
      REQUIRE(res->sections.size() == 3);
      CHECK(res->sections[0] == "a");
      CHECK(res->sections[1] == "b");
      CHECK(res->sections[2] == "c");
    }
  }
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
}