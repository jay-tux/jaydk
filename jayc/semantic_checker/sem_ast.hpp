//
// Created by jay on 9/14/24.
//

#ifndef SEM_AST_HPP
#define SEM_AST_HPP

#include <string>
#include <utility>

// forward declarations
namespace jayc::sem {
class function;
class contract;
class type;
class global;
}

#include "parser/ast.hpp"
#include "hoist_tree.hpp"
#include "util/managed.hpp"
#include "util/variant_helpers.hpp"
#include "util/ref_helpers.hpp"

namespace jayc::sem {
using node = parser::node;

struct expression;
namespace expressions_ {
using unary_op = parser::unary_op;
using binary_op = parser::binary_op;

template <typename T>
struct literal_expr { T value; };

struct ref_expr {
  std::string ref;
};

struct unary_expr {
  unary_op op;
  jaydk::managed<expression> expr;
  type *e_type;
};

struct binary_expr {
  binary_op op;
  jaydk::managed<expression> left;
  jaydk::managed<expression> right;
  type *e_type;
};

struct ternary_expr {
  jaydk::managed<expression> condition;
  jaydk::managed<expression> true_expr;
  jaydk::managed<expression> false_expr;
  type *e_type;
};

struct call_expr {
  // T <op> T -> T.op(T)
  // <op> T -> T.op()
  // T <op> -> T.op(int)
  // T[T] -> T.op(T)
  jaydk::managed<expression> call;
  std::vector<expression> args;
  type *e_type;
};

struct member_expr {
  jaydk::managed<expression> base;
  std::string member;
};
}
using namespace expressions_;

struct expression : node {
  using actual_t = std::variant<
    literal_expr<int64_t>, literal_expr<uint64_t>, literal_expr<float>,
    literal_expr<double>, literal_expr<char>, literal_expr<std::string>,
    literal_expr<bool>, ref_expr, unary_expr, binary_expr, ternary_expr,
    call_expr, member_expr
  >;

  template <typename T> requires(jaydk::is_alternative_for<T, actual_t>)
  expression(const T &t, const location &pos, type &e_type) : node{pos}, content{t}, e_type{&e_type} {}

  actual_t content;
  type *e_type;

  static expression dummy;
};

struct statement;
namespace statements_ {
struct block { std::vector<statement> statements; };
struct expr_stmt { expression expr; };
struct var_decl_stmt {
  std::string name;
  jaydk::opt_ref<type> e_type;
  expression value;
  bool is_mutable;
};

struct if_stmt {
  expression condition;
  jaydk::managed<statement> true_block;
  std::optional<jaydk::managed<statement>> false_block;
};

struct loop_stmt {
  std::optional<jaydk::managed<statement>> init;
  expression condition;
  jaydk::managed<statement> block;
  std::optional<expression> update;
};

struct break_stmt {};
struct continue_stmt {};
struct return_stmt { std::optional<expression> value; };
}
using namespace statements_;

struct statement : node {
  using actual_t = std::variant<
    block, expr_stmt, var_decl_stmt, if_stmt, loop_stmt, break_stmt, continue_stmt, return_stmt
  >;

  template <typename T> requires(jaydk::is_alternative_for<T, actual_t>)
  statement(const T &t, const location &pos) : node{pos}, content{t} {}

  actual_t content;
};

class global : node {
public:
  global();
  global(std::string name, type &e_type, expression initial, bool is_mutable, location decl) :
    node{std::move(decl)}, name{std::move(name)}, e_type{&e_type}, initial{std::move(initial)}, is_mutable{is_mutable} {}

  [[nodiscard]] constexpr const std::string &get_name() const { return name; }
  [[nodiscard]] constexpr const type &get_type() const { return *e_type; }
  [[nodiscard]] constexpr const expression &get_initial() const { return initial; }
  [[nodiscard]] constexpr bool is_var() const { return is_mutable; }
  [[nodiscard]] constexpr location &declared_at() { return pos; }

private:
  std::string name;
  type *e_type;
  expression initial;
  bool is_mutable;
};

class function : node {
public:
  function();
  function(std::string name, std::vector<std::pair<std::string, type *>> params, type *ret_type, statement body, location decl) :
    node{std::move(decl)}, name{std::move(name)}, params{std::move(params)}, ret_type{ret_type}, body{std::move(body)} {}

  [[nodiscard]] constexpr const std::string &get_name() const { return name; }
  [[nodiscard]] constexpr const std::vector<std::pair<std::string, type *>> &get_params() const { return params; }
  [[nodiscard]] constexpr const type &get_ret_type() const { return *ret_type; }
  [[nodiscard]] constexpr const statement &get_body() const { return body; }
  [[nodiscard]] constexpr location &declared_at() { return pos; }

private:
  std::string name;
  std::vector<std::pair<std::string, type *>> params;
  type *ret_type;
  statement body;
};

class contract : node {
public:
  contract();
  contract(std::string name, std::vector<std::pair<std::string, type *>> requirements, location decl) :
    node{std::move(decl)}, name{std::move(name)}, requirements{std::move(requirements)} {}

  [[nodiscard]] constexpr const std::string &get_name() const { return name; }
  [[nodiscard]] constexpr const std::vector<std::pair<std::string, type *>> &get_requirements() const { return requirements; }
  [[nodiscard]] constexpr location &declared_at() { return pos; }

private:
  std::string name;
  std::vector<std::pair<std::string, type *>> requirements;
};

namespace types_ {
enum class primitive { INT64, UINT64, FLOAT32, FLOAT64, CHAR, BOOL };
struct void_type {};
struct invalid_type {};

struct array_type {
  type *element_type;
};

struct function_type {
  std::vector<type *> params;
  type *ret_type;
};

struct type_alias {
  type *actual;
};

class record_type : node {
public:
  using field = global;

  record_type();
  record_type(std::string name, std::vector<field> fields, const std::optional<type *> base,
              std::vector<contract *> explicitly_implemented, std::vector<function> member_functions, location decl) :
    node{std::move(decl)}, name{std::move(name)}, fields{std::move(fields)}, base{base},
    explicitly_implemented{std::move(explicitly_implemented)}, member_functions{std::move(member_functions)} {}

  [[nodiscard]] constexpr const std::string &get_name() const { return name; }
  [[nodiscard]] constexpr const std::vector<field> &get_fields() const { return fields; }
  [[nodiscard]] constexpr const std::optional<type *> &get_base() const { return base; }
  [[nodiscard]] constexpr const std::vector<contract *> &get_explicitly_implemented() const { return explicitly_implemented; }
  [[nodiscard]] constexpr const std::vector<function> &get_member_functions() const { return member_functions; }
  [[nodiscard]] constexpr location &declared_at() { return pos; }

private:
  std::string name;
  std::vector<field> fields;
  std::optional<type *> base;
  std::vector<contract *> explicitly_implemented;
  std::vector<function> member_functions;
  std::unordered_map<std::string, std::string> nested_types;
};
}
using namespace types_;

class type : node {
public:
  using actual_t = std::variant<
    primitive, void_type, invalid_type, array_type, function_type, type_alias, record_type
  >;

  type();
  type(std::string name, actual_t t, const location &pos) : node{pos}, name{std::move(name)}, content{std::move(t)} {}

  [[nodiscard]] constexpr const actual_t &get_actual() const { return content; }
  [[nodiscard]] constexpr location &declared_at() { return pos; }

private:
  std::string name;
  actual_t content;
};

using sem_ast = hoist_tree;
}

#endif //SEM_AST_HPP
