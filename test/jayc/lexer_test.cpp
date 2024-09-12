//
// Created by jay on 9/3/24.
//

#include <string>
#include <sstream>
#include <doctest/doctest.h>
#include <algorithm>

#include "lexer/lexer.hpp"
#include "lexer/token_stream.hpp"
#include "lexer/token_output.hpp"

using namespace jaydk;
using namespace jayc::lexer;

inline static std::string symbols_source = "+-*/%\n++--=\n==!=<><=>=\n&&||!&|~^<<>>\n.::\n()[]{}\n,;:?\n+=-=*=/=%=\n&=|=^=\n=>";
inline static std::string keywords_source = "fun\tvar\nif else\tfor\nwhile do\treturn\nbreak continue\tnamespace\nstruct\tauto val";
inline static std::string id_keyword_source = "__ignored fun test var _name continue with2numbers3 ____";
inline static std::string bool_literal_source = "true false";
inline static std::string i64_literal_source = "-10 123 -589 +12";
inline static std::string ui64_literal_source = "0x123 0xABC 0x123456abcDEF";
inline static std::string f32_literal_source = "123.0f -456.58f";
inline static std::string f64_literal_source = "123.0 -456.58";
inline static std::string char_literal_source = R"('a' 'c' '\n' '\'' '"')";
inline static std::string string_literal_source = R"("abcd\n" "this is a test" "")";
inline static std::string line_comment_source = R"(// initial line comment
test something other
// second line comment
123 // line comment at end of line
_abcd
// line comment /* with unterminated block comment /* inside
fun / /
)";
inline static std::string block_comment_source = R"(/* */ fun / **/
/* let's multi-line
  here's another line */ something
/* and now - /* nesting! /* let's see
    if multi-line nesting */ can get nested even */ more /*
    I have no idea */ at what depth /* we are now */
  but this should finish it */ ()
)";

symbol operator++(symbol &s) {
  s = static_cast<symbol>(static_cast<int>(s) + 1);
  return s;
}

keyword operator++(keyword &k) {
  k = static_cast<keyword>(static_cast<int>(k) + 1);
  return k;
}

TEST_SUITE("jayc - lexer (lexing okay)") {
  TEST_CASE("empty stream = eof") {
    auto lex = lex_source("");
    token t;

    REQUIRE_FALSE(lex.is_eof());
    lex >> t;
    REQUIRE(is<eof>(t.actual));
    REQUIRE(lex.is_eof());
  }

  TEST_CASE("symbol tokens") {
    auto lex = lex_source(symbols_source);
    token t;

    std::vector<std::pair<int, int>> symbol_positions = {
      // 12345
      // +-*/%
      {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5},
      // 12345
      // ++--=
      {2, 1}, {2, 3}, {2, 5},
      // 1234567890
      // ==!=<><=>=
      {3, 1}, {3, 3}, {3, 5}, {3, 6}, {3, 7}, {3, 9},
      // 1234567890123
      // &&||!&|~^<<>>
      {4, 1}, {4, 3}, {4, 5}, {4, 6}, {4, 7}, {4, 8},
      {4, 9}, {4, 10}, {4, 12},
      // 123
      // .::
      {5, 1}, {5, 2},
      // 123456
      // ()[]{}
      {6, 1}, {6, 2}, {6, 3}, {6, 4}, {6, 5}, {6, 6},
      // 1234
      // ,;:?
      {7, 1}, {7, 2}, {7, 3}, {7, 4},
      // 1234567890
      // +=-=*=/=%=
      {8, 1}, {8, 3}, {8, 5}, {8, 7}, {8, 9},
      // 123456
      // &=|=^=
      {9, 1}, {9, 3}, {9, 5},
      // 12
      // =>
      {10, 1}
    };

    for(auto s = symbol::PLUS; s <= symbol::ARROW; ++s) {
      REQUIRE_FALSE(lex.is_eof());
      lex >> t;
      CAPTURE(t);
      REQUIRE(is<symbol>(t.actual));
      CAPTURE(std::get<symbol>(t.actual));
      CHECK(std::get<symbol>(t.actual) == s);
      CHECK(t.pos.line == symbol_positions[static_cast<int>(s)].first);
      CHECK(t.pos.col == symbol_positions[static_cast<int>(s)].second);
    }
    CHECK_FALSE(lex.is_eof());
    CAPTURE(t);
    lex >> t;
    CHECK(is<eof>(t.actual));
    CHECK(lex.is_eof());
    REQUIRE(jayc::logger.phase_error() == 0);
  }

  TEST_CASE("keyword tokens") {
    auto lex = lex_source(keywords_source);
    token t;
    // for\tvar\nif else\tfor\nwhile do\treturn\nbreak continue\tnamespace\nstruct\tauto
    std::vector<std::pair<int, int>> keyword_positions = {
      // 1234 567
      // for\tvar
      {1, 1}, {1, 5},
      // 12345678 901
      // if else\tfor
      {2, 1}, {2, 4}, {2, 9},
      // 123456789 012345
      // while do\treturn
      {3, 1}, {3, 7}, {3, 10},
      // 123456789012345 678901234
      // break continue\tnamespace
      {4, 1}, {4, 7}, {4, 16},
      // 1234567 89012345
      // struct\tauto val
      {5, 1}, {5, 8}, {5, 13}
    };

    for(auto k = keyword::FUN; k <= keyword::VAL; ++k) {
      REQUIRE_FALSE(lex.is_eof());
      lex >> t;
      CAPTURE(t);
      REQUIRE(is<keyword>(t.actual));
      CAPTURE(std::get<keyword>(t.actual));
      CHECK(std::get<keyword>(t.actual) == k);
      CHECK(t.pos.line == keyword_positions[static_cast<int>(k)].first);
      CHECK(t.pos.col == keyword_positions[static_cast<int>(k)].second);
    }
    lex >> t;
    CHECK(is<eof>(t.actual));
    CHECK(lex.is_eof());
    REQUIRE(jayc::logger.phase_error() == 0);
  }

  TEST_CASE("identifier vs keyword") {
    auto lex = lex_source(id_keyword_source);
    token t;
    std::vector tokens = {
      //          1         2         3         4         5
      // 12345678901234567890123456789012345678901234567890123456
      // __ignored fun test var _name continue with2numbers3 ____
      token{ .actual = identifier{"__ignored"}, .pos = {"", 1, 1} },
      token{ .actual = keyword::FUN, .pos = {"", 1, 11} },
      token{ .actual = identifier{"test"}, .pos = {"", 1, 15} },
      token{ .actual = keyword::VAR, .pos = {"", 1, 20} },
      token{ .actual = identifier{"_name"}, .pos = {"", 1, 24} },
      token{ .actual = keyword::CONTINUE, .pos = {"", 1, 30} },
      token{ .actual = identifier{"with2numbers3"}, .pos = {"", 1, 39} },
      token{ .actual = identifier{"____"}, .pos = {"", 1, 53} }
    };

    for(const auto &[actual, at] : tokens) {
      REQUIRE_FALSE(lex.is_eof());
      lex >> t;
      CAPTURE(t);
      REQUIRE(t.actual.index() == actual.index());
      if(is<identifier>(t.actual)) {
        CHECK(std::get<identifier>(t.actual).ident == std::get<identifier>(actual).ident);
      }
      else if(is<keyword>(t.actual)) {
        CHECK(std::get<keyword>(t.actual) == std::get<keyword>(actual));
      }
      CHECK(t.pos.line == at.line);
      CHECK(t.pos.col == at.col);
    }
    lex >> t;
    CHECK(is<eof>(t.actual));
    CHECK(lex.is_eof());
    REQUIRE(jayc::logger.phase_error() == 0);
  }

  TEST_CASE("literals") {
    token t;

    auto lex1 = lex_source(bool_literal_source);
    REQUIRE_FALSE(lex1.is_eof());
    lex1 >> t;
    REQUIRE(is<literal<bool>>(t.actual));
    CHECK(std::get<literal<bool>>(t.actual).value);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 1);
    lex1 >> t;
    REQUIRE(is<literal<bool>>(t.actual));
    CHECK_FALSE(std::get<literal<bool>>(t.actual).value);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 6);
    REQUIRE(jayc::logger.phase_error() == 0);

    auto lex2 = lex_source(i64_literal_source);
    REQUIRE_FALSE(lex2.is_eof());
    lex2 >> t;
    REQUIRE(is<literal<int64_t>>(t.actual));
    CHECK(std::get<literal<int64_t>>(t.actual).value == -10);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 1);
    lex2 >> t;
    REQUIRE(is<literal<int64_t>>(t.actual));
    CHECK(std::get<literal<int64_t>>(t.actual).value == 123);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 5);
    lex2 >> t;
    REQUIRE(is<literal<int64_t>>(t.actual));
    CHECK(std::get<literal<int64_t>>(t.actual).value == -589);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 9);
    lex2 >> t;
    REQUIRE(is<literal<int64_t>>(t.actual));
    CHECK(std::get<literal<int64_t>>(t.actual).value == 12);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 14);
    REQUIRE(jayc::logger.phase_error() == 0);

    auto lex3 = lex_source(ui64_literal_source);
    REQUIRE_FALSE(lex3.is_eof());
    lex3 >> t;
    REQUIRE(is<literal<uint64_t>>(t.actual));
    CHECK(std::get<literal<uint64_t>>(t.actual).value == 0x123);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 1);
    lex3 >> t;
    REQUIRE(is<literal<uint64_t>>(t.actual));
    CHECK(std::get<literal<uint64_t>>(t.actual).value == 0xabc);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 7);
    lex3 >> t;
    REQUIRE(is<literal<uint64_t>>(t.actual));
    CHECK(std::get<literal<uint64_t>>(t.actual).value == 0x123456abcDEF);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 13);
    REQUIRE(jayc::logger.phase_error() == 0);

    auto lex4 = lex_source(f32_literal_source);
    REQUIRE_FALSE(lex4.is_eof());
    lex4 >> t;
    REQUIRE(is<literal<float>>(t.actual));
    CHECK(std::get<literal<float>>(t.actual).value == 123.0f);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 1);
    lex4 >> t;
    REQUIRE(is<literal<float>>(t.actual));
    CHECK(std::get<literal<float>>(t.actual).value == -456.58f);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 8);
    REQUIRE(jayc::logger.phase_error() == 0);

    auto lex5 = lex_source(f64_literal_source);
    REQUIRE_FALSE(lex5.is_eof());
    lex5 >> t;
    REQUIRE(is<literal<double>>(t.actual));
    CHECK(std::get<literal<double>>(t.actual).value == 123.0);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 1);
    lex5 >> t;
    REQUIRE(is<literal<double>>(t.actual));
    CHECK(std::get<literal<double>>(t.actual).value == -456.58);
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 7);
    REQUIRE(jayc::logger.phase_error() == 0);

    auto lex6 = lex_source(char_literal_source);
    REQUIRE_FALSE(lex6.is_eof());
    lex6 >> t;
    REQUIRE(is<literal<char>>(t.actual));
    CHECK(std::get<literal<char>>(t.actual).value == 'a');
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 1);
    lex6 >> t;
    REQUIRE(is<literal<char>>(t.actual));
    CHECK(std::get<literal<char>>(t.actual).value == 'c');
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 5);
    lex6 >> t;
    REQUIRE(is<literal<char>>(t.actual));
    CHECK(std::get<literal<char>>(t.actual).value == '\n');
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 9);
    lex6 >> t;
    REQUIRE(is<literal<char>>(t.actual));
    CHECK(std::get<literal<char>>(t.actual).value == '\'');
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 14);
    lex6 >> t;
    REQUIRE(is<literal<char>>(t.actual));
    CHECK(std::get<literal<char>>(t.actual).value == '"');
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 19);
    REQUIRE(jayc::logger.phase_error() == 0);

    //          1         2
    // 1234567890123456789012345678
    // "abcd\n" "this is a test" ""
    auto lex7 = lex_source(string_literal_source);
    REQUIRE_FALSE(lex7.is_eof());
    lex7 >> t;
    REQUIRE(is<literal<std::string>>(t.actual));
    CHECK(std::get<literal<std::string>>(t.actual).value == "abcd\n");
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 1);
    lex7 >> t;
    REQUIRE(is<literal<std::string>>(t.actual));
    CHECK(std::get<literal<std::string>>(t.actual).value == "this is a test");
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 10);
    lex7 >> t;
    REQUIRE(is<literal<std::string>>(t.actual));
    CHECK(std::get<literal<std::string>>(t.actual).value == "");
    CHECK(t.pos.line == 1);
    CHECK(t.pos.col == 27);
    REQUIRE(jayc::logger.phase_error() == 0);
  }

  TEST_CASE("line comments") {
    auto lex = lex_source(line_comment_source);
    token t;
    REQUIRE_FALSE(lex.is_eof());

    std::string src = "<inline script>";
    std::vector expected = {
      token{ .actual = identifier{"test"}, .pos = {src, 2, 1} },
      token{ .actual = identifier{"something"}, .pos = {src, 2, 6} },
      token{ .actual = identifier{"other"}, .pos = {src, 2, 16} },
      token{ .actual = literal<int64_t>{123}, .pos = {src, 4, 1} },
      token{ .actual = identifier{"_abcd"}, .pos = {src, 5, 1} },
      token{ .actual = keyword::FUN, .pos = {src, 7, 1} },
      token{ .actual = symbol::DIVIDE, .pos = {src, 7, 5} },
      token{ .actual = symbol::DIVIDE, .pos = {src, 7, 7} },
      token{ .actual = eof{}, .pos = {src, 8, 1} }
    };

    for(const auto &[actual, pos]: expected) {
      lex >> t;
      REQUIRE(t.actual.index() == actual.index());
      if(is<identifier>(actual)) {
        CHECK(std::get<identifier>(t.actual).ident == std::get<identifier>(actual).ident);
      }
      else if(is<literal<int64_t>>(actual)) {
        CHECK(std::get<literal<int64_t>>(t.actual).value == std::get<literal<int64_t>>(actual).value);
      }
      else if(is<keyword>(actual)) {
        CHECK(std::get<keyword>(t.actual) == std::get<keyword>(actual));
      }
      else if(is<symbol>(actual)) {
        CHECK(std::get<symbol>(t.actual) == std::get<symbol>(actual));
      }
      CHECK(t.pos.file == pos.file);
      CHECK(t.pos.line == pos.line);
      CHECK(t.pos.col == pos.col);
    }

    CHECK(lex.is_eof());
  }

  TEST_CASE("block comments") {
    auto lex = lex_source(block_comment_source);
    token t;
    REQUIRE_FALSE(lex.is_eof());

    const std::string src = "<inline script>";
    std::vector expected = {
      token{ .actual = keyword::FUN, .pos = {src, 1, 7} },
      token{ .actual = symbol::DIVIDE, .pos = {src, 1, 11} },
      token{ .actual = symbol::MULTIPLY, .pos = {src, 1, 13} },
      token{ .actual = symbol::MULTIPLY, .pos = {src, 1, 14} },
      token{ .actual = symbol::DIVIDE, .pos = {src, 1, 15} },
      token{ .actual = identifier{"something"}, .pos = {src, 3, 26} },
      token{ .actual = symbol::PAREN_OPEN, .pos = {src, 7, 32} },
      token{ .actual = symbol::PAREN_CLOSE, .pos = {src, 7, 33} },
      token{ .actual = eof{}, .pos = {src, 8, 1} }
    };

    for(const auto &[actual, pos]: expected) {
      lex >> t;
      REQUIRE(t.actual.index() == actual.index());
      if(is<identifier>(actual)) {
        CHECK(std::get<identifier>(t.actual).ident == std::get<identifier>(actual).ident);
      }
      else if(is<keyword>(actual)) {
        CHECK(std::get<keyword>(t.actual) == std::get<keyword>(actual));
      }
      else if(is<symbol>(actual)) {
        CHECK(std::get<symbol>(t.actual) == std::get<symbol>(actual));
      }
      CHECK(t.pos.file == pos.file);
      CHECK(t.pos.line == pos.line);
      CHECK(t.pos.col == pos.col);
    }

    CHECK(lex.is_eof());
  }

  TEST_CASE("factorial script") {
    const std::string script = TEST_SOURCE "/inputs/factorial.jay";
    auto lex = jayc::lexer::lex(script);
    token t;

    std::vector<token> expected = {
      //          1         2
      // 123456789012345678901234567
      // fun factorial(int x): int {
      { .actual = keyword::FUN, .pos = {script, 1, 1} },
      { .actual = identifier{"factorial"}, .pos = {script, 1, 5} },
      { .actual = symbol::PAREN_OPEN, .pos = {script, 1, 14} },
      { .actual = identifier{"int"}, .pos = {script, 1, 15} },
      { .actual = identifier{"x"}, .pos = {script, 1, 19} },
      { .actual = symbol::PAREN_CLOSE, .pos = {script, 1, 20} },
      { .actual = symbol::COLON, .pos = {script, 1, 21} },
      {.actual = identifier{"int"}, .pos = {script, 1, 23} },
      { .actual = symbol::BRACE_OPEN, .pos = {script, 1, 27} },

      //          1         2
      // 123456789012345678901234
      //     if(x <= 1) return 1;
      { .actual = keyword::IF, .pos = {script, 2, 5} },
      { .actual = symbol::PAREN_OPEN, .pos = {script, 2, 7} },
      { .actual = identifier{"x"}, .pos = {script, 2, 8} },
      { .actual = symbol::LESS_THAN_EQUALS, .pos = {script, 2, 10} },
      { .actual = literal<int64_t>{1}, .pos = {script, 2, 13} },
      { .actual = symbol::PAREN_CLOSE, .pos = {script, 2, 14} },
      { .actual = keyword::RETURN, .pos = {script, 2, 16} },
      { .actual = literal<int64_t>{1}, .pos = {script, 2, 23} },
      { .actual = symbol::SEMI, .pos = {script, 2, 24} },

      //          1         2         3
      // 12345678901234567890123456789012
      //     return x * factorial(x - 1);
      { .actual = keyword::RETURN, .pos = {script, 3, 5} },
      { .actual = identifier{"x"}, .pos = {script, 3, 12} },
      { .actual = symbol::MULTIPLY, .pos = {script, 3, 14} },
      { .actual = identifier{"factorial"}, .pos = {script, 3, 16} },
      { .actual = symbol::PAREN_OPEN, .pos = {script, 3, 25} },
      { .actual = identifier{"x"}, .pos = {script, 3, 26} },
      { .actual = symbol::MINUS, .pos = {script, 3, 28} },
      { .actual = literal<int64_t>{1}, .pos = {script, 3, 30} },
      { .actual = symbol::PAREN_CLOSE, .pos = {script, 3, 31} },
      { .actual = symbol::SEMI, .pos = {script, 3, 32} },

      // 1
      // }
      { .actual = symbol::BRACE_CLOSE, .pos = {script, 4, 1} },

      //          1         2         3
      // 123456789012345678901234567890123456
      // fun factorial_non_rec(int x): auto {
      { .actual = keyword::FUN, .pos = {script, 6, 1} },
      { .actual = identifier{"factorial_non_rec"}, .pos = {script, 6, 5} },
      { .actual = symbol::PAREN_OPEN, .pos = {script, 6, 22} },
      { .actual = identifier{"int"}, .pos = {script, 6, 23} },
      { .actual = identifier{"x"}, .pos = {script, 6, 27} },
      { .actual = symbol::PAREN_CLOSE, .pos = {script, 6, 28} },
      { .actual = symbol::COLON, .pos = {script, 6, 29}},
      { .actual = keyword::AUTO, .pos = {script, 6, 31} },
      { .actual = symbol::BRACE_OPEN, .pos = {script, 6, 36} },

      //          1
      // 1234567890123456
      //     int acc = x;
      { .actual = identifier{"int"}, .pos = {script, 7, 5} },
      { .actual = identifier{"acc"}, .pos = {script, 7, 9} },
      { .actual = symbol::ASSIGN, .pos = {script, 7, 13} },
      { .actual = identifier{"x"}, .pos = {script, 7, 15} },
      { .actual = symbol::SEMI, .pos = {script, 7, 16} },

      //          1         2
      // 12345678901234567890
      //     while(x --> 0) {
      { .actual = keyword::WHILE, .pos = {script, 8, 5} },
      { .actual = symbol::PAREN_OPEN, .pos = {script, 8, 10} },
      { .actual = identifier{"x"}, .pos = {script, 8, 11} },
      { .actual = symbol::DECREMENT, .pos = {script, 8, 13} },
      { .actual = symbol::GREATER_THAN, .pos = {script, 8, 15} },
      { .actual = literal<int64_t>{0}, .pos = {script, 8, 17} },
      { .actual = symbol::PAREN_CLOSE, .pos = {script, 8, 18} },
      { .actual = symbol::BRACE_OPEN, .pos = {script, 8, 20} },

      //          1
      // 12345678901234567
      //         acc *= x;
      { .actual = identifier{"acc"}, .pos = {script, 9, 9} },
      { .actual = symbol::MULTIPLY_ASSIGN, .pos = {script, 9, 13} },
      { .actual = identifier{"x"}, .pos = {script, 9, 16} },
      { .actual = symbol::SEMI, .pos = {script, 9, 17} },

      // 12345
      //     }
      { .actual = symbol::BRACE_CLOSE, .pos = {script, 10, 5} },

      //          1
      // 123456789012345
      //     return acc;
      { .actual = keyword::RETURN, .pos = {script, 11, 5} },
      { .actual = identifier{"acc"}, .pos = {script, 11, 12} },
      { .actual = symbol::SEMI, .pos = {script, 11, 15} },

      // 1
      // }
      { .actual = symbol::BRACE_CLOSE, .pos = {script, 12, 1} },

      { .actual = eof{}, .pos = {script, 13, 1} }
    };

    for(const auto &e : expected) {
      lex >> t;
      const auto &[actual, pos] = e;
      SUBCASE(("Token at " + pos.file + "; line " + std::to_string(pos.line) + ":" + std::to_string(pos.col)).data()) {
        CAPTURE(t);
        CAPTURE(e);
        REQUIRE(t.actual.index() == actual.index());
        if(is<keyword>(t.actual)) CHECK(as<keyword>(t.actual) == as<keyword>(actual));
        else if(is<identifier>(t.actual)) CHECK(as<identifier>(t.actual).ident == as<identifier>(actual).ident);
        else if(is<symbol>(t.actual)) CHECK(as<symbol>(t.actual) == as<symbol>(actual));
        else if(is<literal<int64_t>>(t.actual)) CHECK(as<literal<int64_t>>(t.actual).value == as<literal<int64_t>>(actual).value);

        CHECK(t.pos.file == pos.file);
        CHECK(t.pos.line == pos.line);
        CHECK(t.pos.col == pos.col);
      }
    }
    CHECK(lex.is_eof());
  }
}

TEST_SUITE("jayc - lexer (lexing fails)") {
  // TODO: tests with invalid input:
  //  -> unterminated string literal
  //  -> unterminated character literal
  //  -> unterminated block comment
  //  -> character literal too wide
  //  -> incomplete float literal (ends with `.` or `.f` or `.F`)
}