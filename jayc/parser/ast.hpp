//
// Created by jay on 9/5/24.
//

#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <utility>
#include <vector>
#include <optional>

#include "error_queue.hpp"
#include "util.hpp"

namespace jayc::parser {
struct node {
  explicit node(location pos) : pos{std::move(pos)} {}
  location pos;
};

enum struct unary_op {
  UN_PLUS, UN_MINUS, PRE_INCR, POST_INCR, PRE_DECR, POST_DECR, BOOL_NEG, BIT_NEG
};
enum struct binary_op {
  ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO,
  EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
  BOOL_AND, BOOL_OR, BIT_AND, BIT_OR, XOR, SHIFT_LEFT, SHIFT_RIGHT
};

struct qualified_name {
  // qname: <name>(::<name>)*
  std::vector<std::string> sections;
};

struct type_name {
  // tname: <qname>(<<tname>(,<tname>)*>)?([])?
  qualified_name base_name;
  std::vector<type_name> template_args;
  bool is_array;
};

namespace expressions_ {
struct expression;
template <typename T>
struct literal_expr {
  T value;
};

struct name_expr {
  qualified_name name;
};

struct unary_expr {
  unary_op op;
  jaydk::managed<expression> expr;
};

struct binary_expr {
  binary_op op;
  jaydk::managed<expression> left;
  jaydk::managed<expression> right;
};

struct ternary_expr {
  jaydk::managed<expression> cond;
  jaydk::managed<expression> true_expr;
  jaydk::managed<expression> false_expr;
};

struct call_expr {
  jaydk::managed<expression> call;
  std::vector<expression> args;
};

struct index_expr {
  jaydk::managed<expression> base;
  jaydk::managed<expression> index;
};

struct member_expr {
  jaydk::managed<expression> base;
  std::string member;
};

struct expression : node {
  using actual_t = std::variant<
    literal_expr<int64_t>, literal_expr<uint64_t>, literal_expr<float>, literal_expr<double>,
    literal_expr<char>, literal_expr<std::string>, literal_expr<bool>, name_expr,
    unary_expr, binary_expr, ternary_expr, call_expr, index_expr, member_expr
  >;

  template <typename T> requires(jaydk::is_alternative_for<T, actual_t>)
  expression(const T &t, const location &pos) : node{pos}, content{t} {}

  actual_t content;
};
}
using namespace expressions_;

namespace statements_ {
struct statement;
struct block {
  std::vector<statement> statements;
};

struct expr_stmt {
  expression expr;
};

struct var_decl_stmt {
  std::string name;
  expression value;
};

struct assign_stmt {
  expression lvalue;
  expression value;
};

struct op_assign_stmt {
  expression lvalue;
  binary_op op = binary_op::ADD;
  expression value;
};

struct if_stmt {
  expression condition;
  jaydk::managed<statement> true_block;
  std::optional<jaydk::managed<statement>> false_block;
};

struct for_stmt {
  jaydk::managed<statement> init;
  expression condition;
  expression update;
  jaydk::managed<statement> block;
};

struct for_each_stmt {
  std::string binding;
  expression collection;
  jaydk::managed<statement> block;
};

struct while_stmt {
  bool is_do_while = false;
  expression condition;
  std::vector<statement> block;
};

struct break_stmt {};
struct continue_stmt {};

struct return_stmt {
  std::optional<expression> value;
};

struct statement : node {
  using actual_t = std::variant<
    block, expr_stmt, var_decl_stmt, assign_stmt, op_assign_stmt, if_stmt, for_stmt, for_each_stmt,
    while_stmt, break_stmt, continue_stmt, return_stmt
  >;

  template <typename T> requires(jaydk::is_alternative_for<T, actual_t>)
  statement(const T &t, const location &pos) : node{pos}, content{t} {}

  actual_t content;
};
}
using namespace statements_;

namespace declarations_ {
struct declaration;

struct namespace_decl {
  std::string name;
  std::vector<declaration> declarations;
};

// TODO: template arguments?
struct function_decl {
  struct arg {
    type_name type;
    std::string name;
    location pos;
  };
  std::string name;
  std::vector<arg> args;
  std::vector<statement> body;
};

// TODO: template arguments?
struct ext_function_decl {
  using arg = function_decl::arg;
  type_name receiver;
  std::string name;
  std::vector<arg> args;
  std::vector<statement> body;
};

struct global_decl {
  std::string name;
  expression value;
};

struct typed_global_decl {
  type_name type;
  std::string name;
  std::optional<expression> initial;
};

struct type_decl {
  std::string name;
  std::vector<std::string> template_args;
  std::vector<type_name> bases;
  std::vector<std::pair<typed_global_decl, location>> fields;
  std::vector<std::pair<function_decl, location>> members;
  std::vector<std::pair<type_decl, location>> nested_types;
};

struct declaration : node {
  using actual_t = std::variant<
    namespace_decl, function_decl, ext_function_decl, type_decl, global_decl, typed_global_decl
  >;

  template <typename T> requires(jaydk::is_alternative_for<T, actual_t>)
  inline declaration(const T &t, const location &pos) : node{pos}, content{t} {}

  actual_t content;
};
}
using namespace declarations_;

using ast = std::vector<declaration>;
}

#endif //AST_HPP
