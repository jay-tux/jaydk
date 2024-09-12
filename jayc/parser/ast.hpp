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

struct name {
  // name: <identifier>(< <name>(, <name>)* >)? ([])? (::<name>)?
  std::string section;
  std::vector<name> template_args;
  jaydk::heap_opt<name> next;
  bool is_array;
};

namespace expressions_ {
struct expression;
template <typename T>
struct literal_expr {
  T value;
};

struct name_expr {
  name actual;
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
  std::string var_name;
  std::optional<name> type_name;
  expression value;
  bool is_mutable;
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
  expression update; // TODO: convert to statement (x++ works, but x += 2 doesn't) -> requires moving assignment to expressions
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
  jaydk::managed<statement> block;
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

struct function_decl {
  struct arg {
    name type;
    std::string arg_name;
    location pos;
  };
  struct no_return_type {};
  struct auto_type {};
  using return_type_t = std::variant<no_return_type, auto_type, name>;

  std::string function_name;
  std::vector<arg> args;
  return_type_t return_type;
  std::vector<statement> body;
};

struct template_function_decl {
  struct template_arg {
    std::string arg_name;
    std::vector<name> constraints;
  };

  function_decl base;
  std::vector<template_arg> template_args;
};

struct ext_function_decl {
  using arg = function_decl::arg;
  using return_type_t = function_decl::return_type_t;

  name receiver;
  std::string ext_func_name;
  std::vector<arg> args;
  return_type_t return_type;
  std::vector<statement> body;
};

struct template_ext_function_decl {
  using template_arg = template_function_decl::template_arg;

  ext_function_decl base;
  std::vector<template_arg> template_args;
};

struct global_decl {
  std::string glob_name;
  std::optional<name> type;
  expression value;
  bool is_mutable;
};

// struct typed_global_decl {
//   name type;
//   std::string glob_name;
//   std::optional<expression> initial;
//   bool is_mutable;
// };

struct template_type_decl;

struct type_decl {
  std::string type_name;
  std::vector<name> bases;
  std::vector<std::pair<global_decl, location>> fields;
  std::vector<std::pair<function_decl, location>> members;
  std::vector<std::pair<template_function_decl, location>> template_members;
  std::vector<std::pair<type_decl, location>> nested_types;
  std::vector<std::pair<template_type_decl, location>> nested_template_types;
};

struct template_type_decl {
  using template_arg = template_function_decl::template_arg;

  type_decl base;
  std::vector<template_arg> template_args;
};

struct declaration : node {
  using actual_t = std::variant<
    namespace_decl, function_decl, template_function_decl, ext_function_decl, template_ext_function_decl,
    type_decl, template_type_decl, global_decl
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
