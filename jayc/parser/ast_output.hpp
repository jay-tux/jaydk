//
// Created by jay on 9/5/24.
//

#ifndef AST_OUTPUT_HPP
#define AST_OUTPUT_HPP

#include <iostream>

#include "ast.hpp"

namespace jayc::parser {
struct indent {
  size_t level = 0;
};

inline std::ostream &operator<<(std::ostream &target, const indent &i) {
  if(i.level == 0) return target;
  if(i.level == 1) return target << "+";
  target << "|";
  for(size_t j = 0; j < i.level - 1; j++) target << " |";
  target << " +";
  return target;
}

inline std::ostream &operator<<(std::ostream &target, const unary_op &uo) {
#define OPS \
  X(UN_PLUS, "+ (unary)") X(UN_MINUS, "- (unary)") X(PRE_INCR, "++ (pre)") X(PRE_DECR, "-- (pre)") \
  X(POST_INCR, "++ (post)") X(POST_DECR, "-- (post)") X(BOOL_NEG, "!") X(BIT_NEG, "~")
#define X(x, y) case jayc::parser::unary_op::x: return target << y;

  switch(uo) {
    OPS
  }
  return target;

#undef X
#undef OPS
}

inline std::ostream &operator<<(std::ostream &target, const binary_op &bo) {
#define OPS \
  X(ADD, "+") X(SUBTRACT, "-") X(MULTIPLY, "*") X(DIVIDE, "/") X(MODULO, "%") \
  X(BOOL_AND, "&&") X(BOOL_OR, "||") X(EQUAL, "==") X(NOT_EQUAL, "!=") X(LESS, "<") \
  X(LESS_EQUAL, "<=") X(GREATER, ">") X(GREATER_EQUAL, ">=") \
  X(SHIFT_LEFT, "<<") X(SHIFT_RIGHT, ">>") X(BIT_AND, "&") X(BIT_OR, "|") X(XOR, "^")
#define X(x, y) case jayc::parser::binary_op::x: return target << y;

  switch(bo) {
    OPS
  }
  return target;

#undef X
#undef OPS
}

inline std::ostream &operator<<(std::ostream &target, const qualified_name &qn) {
  if(qn.sections.empty()) return target << "(empty qualified name)";

  target << qn.sections[0];
  for(size_t i = 1; i < qn.sections.size(); i++) target << "::" << qn.sections[i];
  return target;
}

inline std::ostream &operator<<(std::ostream &target, const type_name &tn) {
  target << tn.base_name;
  if(!tn.template_args.empty()) {
    target << "<" << tn.template_args[0];
    for(size_t i = 1; i < tn.template_args.size(); i++) target << ", " << tn.template_args[i];
    target << ">";
  }
  if(tn.is_array) target << "[]";
  return target;
}

void print_expr(std::ostream &target, const expression &e, size_t i);

struct expr_printer {
  template <typename T>
  inline void operator()(std::ostream &target, const literal_expr<T> &l, const location &pos, const size_t i) const {
    target << indent{i} << "literal(" << lexer::internal_::_internal_type_name<T>::value << "): `"
           << l.value << "` (at " << pos << ")\n";
  }
  inline void operator()(std::ostream &target, const name_expr &n, const location &pos, const size_t i) const {
    target << indent{i} << "name: `" << n.name << "` (at " << pos << ")\n";
  }
  inline void operator()(std::ostream &target, const unary_expr &u, const location &pos, const size_t i) const {
    target << indent{i} << "unary operator: " << u.op << " (at " << pos << ") on\n";
    print_expr(target, *u.expr, i + 1);
  }
  inline void operator()(std::ostream &target, const binary_expr &b, const location &pos, const size_t i) const {
    target << indent{i} << "binary operator (at " << pos << ")\n";
    print_expr(target, *b.left, i + 1);
    target << indent{i + 1} << "operator " << b.op << "\n";
    print_expr(target, *b.right, i + 1);
  }
  inline void operator()(std::ostream &target, const call_expr &c, const location &pos, const size_t i) const {
    target << indent{i} << "call (at " << pos << ")\n"
           << indent{i + 1} << "functor: \n";
    print_expr(target, *c.call, i + 2);
    for(size_t i = 0; i < c.args.size(); i++) {
      target << indent{i + 1} << "args[" << i << "]:\n";
      print_expr(target, c.args[i], i + 2);
    }
  }
  inline void operator()(std::ostream &target, const member_expr &m, const location &pos, const size_t i) const {
    target << indent{i} << "access to member `" << m.member << "` (at " << pos << ")\n";
    print_expr(target, *m.base, i + 1);
  }
};

inline void print_expr(std::ostream &target, const expression &e, size_t i) {
  constexpr static expr_printer p{};
  std::visit([&target, &i, &e](const auto &x) { p(target, x, e.pos, i); }, e.content);
}

void print_stmt(std::ostream &target, const statement &s, size_t i);

struct stmt_printer {
  inline void operator()(std::ostream &target, const block &b, const location &pos, const size_t i) const {
    target << indent{i} << "statement block (at " << pos << ")\n";
    for(const auto &stmt : b.statements) {
      print_stmt(target, stmt, i + 1);
    }
  }

  inline void operator()(std::ostream &target, const expr_stmt &e, const location &pos, const size_t i) const {
    target << indent{i} << "expression statement (at " << pos << ")\n";
    print_expr(target, e.expr, i + 1);
  }

  inline void operator()(std::ostream &target, const var_decl_stmt &r, const location &pos, const size_t i) const {
    target << indent{i} << "declaration of `" << r.name << "` (at " << pos << "); with initial value:\n";
    print_expr(target, r.value, i + 1);
  }

  inline void operator()(std::ostream &target, const assign_stmt &a, const location &pos, const size_t i) const {
    target << indent{i} << "assignment (at " << pos << ")\n";
    target << indent{i + 1} << "to l-value:\n";
    print_expr(target, a.lvalue, i + 2);
    target << indent{i + 1} << "with value:\n";
    print_expr(target, a.value, i + 2);
  }

  inline void operator()(std::ostream &target, const op_assign_stmt &o, const location &pos, const size_t i) const {
    target << indent{i} << "assignment (at " << pos << ")\n";
    target << indent{i + 1} << "to l-value:\n";
    print_expr(target, o.lvalue, i + 2);
    target << indent{i + 1} << "using operator " << o.op << "\n";
    target << indent{i + 1} << "with value:\n";
    print_expr(target, o.value, i + 2);
  }

  inline void operator()(std::ostream &target, const if_stmt &s, const location &pos, const size_t i) const {
    target << indent{i} << "if statement (at " << pos << ")\n";
    target << indent{i + 1} << "condition:\n";
    print_expr(target, s.condition, i + 2);
    target << indent{i + 1} << "then branch:\n";
    print_stmt(target, *s.true_block, i + 2);
    if(s.false_block) {
      target << indent{i + 1} << "else branch:\n";
      print_stmt(target, **s.false_block, i + 2);
    }
  }

  inline void operator()(std::ostream &target, const for_stmt &f, const location &pos, const size_t i) const {
    target << indent{i} << "for statement (at " << pos << ")\n";
    target << indent{i + 1} << "initializer:\n";
    print_stmt(target, *f.init, i + 2);
    target << indent{i + 1} << "condition:\n";
    print_expr(target, f.condition, i + 2);
    target << indent{i + 1} << "iterator:\n";
    print_stmt(target, *f.update, i + 2);
    target << indent{i + 1} << "body:\n";
    print_stmt(target, *f.block, i + 2);
  }

  inline void operator()(std::ostream &target, const for_each_stmt &f, const location &pos, const size_t i) const {
    target << indent{i} << "for-each statement (at " << pos << ")\n";
    target << indent{i + 1} << "binding: " << f.binding << "\n";
    target << indent{i + 1} << "collection:\n";
    print_expr(target, *f.collection, i + 2);
    target << indent{i + 1} << "body:\n";
    print_stmt(target, *f.block, i + 2);
  }

  inline void operator()(std::ostream &target, const while_stmt &w, const location &pos, const size_t i) const {
    target << indent{i} << "while statement (at " << pos << ")\n";
    target << indent{i + 1} << "condition:\n";
    print_expr(target, w.condition, i + 2);
    target << indent{i + 1} << "body:\n";
    print_stmt(target, *w.block, i + 2);
  }

  inline void operator()(std::ostream &target, const break_stmt &, const location &pos, const size_t i) const {
    target << indent{i} << "break statement (at " << pos << ")\n";
  }

  inline void operator()(std::ostream &target, const continue_stmt &, const location &pos, const size_t i) const {
    target << indent{i} << "continue statement (at " << pos << ")\n";
  }

  inline void operator()(std::ostream &target, const return_stmt &r, const location &pos, const size_t i) const {
    target << indent{i} << "return statement (at " << pos << ")\n";
    if(r.value) {
      target << indent{i + 1} << "with value:\n";
      print_expr(target, *r.value, i + 2);
    }
  }
};

inline void print_stmt(std::ostream &target, const statement &s, size_t i) {
  static constexpr stmt_printer p{};
  std::visit([&target, &i, &s](const auto &x) { p(target, x, s.pos, i); }, s.content);
}

void print_decl(std::ostream &target, const declaration &d, size_t i);

struct decl_printer {
  inline void operator()(std::ostream &target, const namespace_decl &n, const location &pos, const size_t i) const {
    target << indent{i} << "declaration of namespace `" << n.name << "` (at " << pos << ")\n";
    for(const auto &d : n.declarations) {
      print_decl(target, d, i + 1);
    }
  }

  inline void operator()(std::ostream &target, const function_decl &f, const location &pos, const size_t i) const {
    target << indent{i} << "declaration of function `" << f.name << "` (at " << pos << ")\n";
    target << indent{i + 1} << "with " << f.args.size() << " argument(s)\n";
    for(const auto &[t, n, p]: f.args) {
      target << indent{i + 2} << t << " " << n << " (at " << p << ")\n";
    }
    target << indent{i + 1} << "with body\n";
    for(const auto &s: f.body) {
      print_stmt(target, s, i + 2);
    }
  }

  inline void operator()(std::ostream &target, const ext_function_decl &f, const location &pos, const size_t i) const {
    target << indent{i} << "declaration of function `" << f.name << "` (at " << pos << ")\n";
    target << indent{i + 1} << "with receiver " << f.receiver << "\n";
    target << indent{i + 1} << "with " << f.args.size() << " argument(s)\n";
    for(const auto &[t, n, p]: f.args) {
      target << indent{i + 2} << t << " " << n << " (at " << p << ")\n";
    }
    target << indent{i + 1} << "with body\n";
    for(const auto &s: f.body) {
      print_stmt(target, s, i + 2);
    }
  }

  inline void operator()(std::ostream &target, const type_decl &t, const location &pos, const size_t i) const {
    target << indent{i} << "declaration of type `" << t.name << "` (at " << pos << ")\n";

    target << indent{i + 1} << "with " << t.template_args.size() << " template argument(s):\n";
    for(const auto &n: t.template_args) {
      target << indent{i + 2} << n << "\n";
    }

    target << indent{i + 1} << "with " << t.bases.size() << " base(s) and/or implemented interface(s):\n";
    for(const auto &n: t.bases) {
      target << indent{i + 2} << n << "\n";
    }

    target << indent{i + 1} << "with " << t.fields.size() << " member field(s):\n";
    for(const auto &[t, n, init, p]: t.fields) {
      target << indent{i + 2} << t << " " << n << " (at " << p << ")";
      if(init.has_value()) {
        target << "; with initial value:\n";
        print_expr(target, *init, i + 3);
      }
      else {
        target << "\n";
      }
    }

    target << indent{i + 1} << "with " << t.members.size() << " member function(s):\n";
    for(const auto &[fd, p]: t.members) {
      (*this)(target, fd, p, i + 2);
    }

    target << indent{i + 1} << "with " << t.nested_types.size() << " nested type(s):\n";
    for(const auto &[td, p]: t.nested_types) {
      (*this)(target, td, p, i + 2);
    }
  }

  inline void operator()(std::ostream &target, const global_decl &g, const location &pos, const size_t i) const {
    target << indent{i} << "declaration of global variable `" << g.name << "` (at " << pos << "); with initial value:\n";
    print_expr(target, g.value, i + 1);
  }
};

inline void print_decl(std::ostream &target, const declaration &d, size_t i) {
  constexpr static decl_printer p{};
  std::visit([&target, &d, i](const auto &x) { p(target, x, d.pos, i); }, d.content);
}

}

inline std::ostream &operator<<(std::ostream &target, const jayc::parser::ast &ast) {
  for(const auto &d: ast) jayc::parser::print_decl(target, d, 0);
  return target;
}

#endif //AST_OUTPUT_HPP
