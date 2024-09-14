//
// Created by jay on 9/14/24.
//

#include <vector>

#include "hoist_tree.hpp"
#include "sem_ast.hpp"
#include "semantic_checker.hpp"

using namespace jaydk;
using namespace jayc::parser;
using namespace jayc::sem;

bool build_initial(hoist_tree &out, const ast &ast) {
  static auto ns_only = [](const std::vector<declaration> &decls) {
    return filter_is_as<namespace_decl>(decls | std::views::transform([](const declaration &x) { return x.content; }));
  };

  // step 0: register builtin types etc
  jayc::location builtin_loc{ "(builtin)", 0, 0 };
  std::vector<std::pair<type, std::vector<std::string>>> builtins {
    {type("void", void_type{}, builtin_loc), {}},
    {type("int", primitive::INT64, builtin_loc), {"int64"}},
    {type("uint", primitive::UINT64, builtin_loc), {"uint64"}},
    {type("float", primitive::FLOAT32, builtin_loc), {"float32"}},
    {type("float64", primitive::FLOAT64, builtin_loc), {}},
    {type("char", primitive::CHAR, builtin_loc), {"uint8"}},
    {type("bool", primitive::BOOL, builtin_loc), {"uint1"}}
  };

  for(const auto &[t, alias] : builtins) {
    auto name = out.root_node().register_type(t.get_name(), t.declared_at());
    out.lookup_type(name) = t;
    auto *type_ptr = &out.lookup_type(name);
    for(const auto &a : alias) {
      out.root_node().register_type(a, t.declared_at());
      out.lookup_type(a) = {a, type_alias{type_ptr}, builtin_loc};
    }
  }

  // step 1: register all namespaces
  std::vector<std::pair<hoist_tree::node &, const namespace_decl &>> ns_stack;
  ns_stack.emplace_back(out.root_node(), ast);

  while(!ns_stack.empty()) {
    auto [node, d] = ns_stack.back();
    ns_stack.pop_back();
    for(const auto &sub : ns_only(d.declarations)) {
      ns_stack.emplace_back(node[sub.name], sub);
    }
  }

  // step 2: register all types
  ns_stack.emplace_back(out.root_node(), ast);
  while(!ns_stack.empty()) {
    auto [node, d] = ns_stack.back();
    ns_stack.pop_back();
    for(const auto &sub : filter_is_as<type_decl>(d.declarations)) {
      node.register_type(sub.name, sub.location);
    }

    // TODO: type registration
  }

  // step 2: register all functions
  // TODO

  // step 3: register all globals
  // TODO

  return true;
}

std::optional<sem_ast> jayc::sem::check_semantics(const ast &ast) {
  // step 1: build base hoisting tree, with placeholders everywhere
  hoist_tree tree;
  if(!build_initial(tree, ast)) return std::nullopt;

  // step 2: one-by-one, type-check everything
  // TODO

  return tree;
}