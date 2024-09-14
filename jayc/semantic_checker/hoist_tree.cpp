//
// Created by jay on 9/14/24.
//

#include <unordered_set>

#include "hoist_tree.hpp"
#include "sem_ast.hpp"
#include "mangler.hpp"
#include "semantic_error.hpp"
#include "util/optional_helpers.hpp"

using namespace jaydk;
using namespace jayc::sem;

template <typename K, typename V>
opt_ref<const V> operator>>(const std::unordered_map<K, managed<V>> &map, const K &key) {
  if(const auto it = map.find(key); it != map.end()) {
    return *it->second | ref{} | maybe{};
  }
  return std::nullopt;
}

template <typename K, typename V>
std::optional<V> operator>>(const std::unordered_map<K, V> &map, const K &key) {
  if(const auto it = map.find(key); it != map.end()) {
    return it->second | maybe{};
  }
  return std::nullopt;
}

function &hoist_tree::lookup_function(const std::string &name) { return *hoisted_functions[name]; }
contract &hoist_tree::lookup_contract(const std::string &name) { return *hoisted_contracts[name]; }
type &hoist_tree::lookup_type(const std::string &name) { return *hoisted_types[name]; }
global &hoist_tree::lookup_global(const std::string &name) { return *hoisted_globals[name]; }

opt_ref<const function> hoist_tree::lookup_function(const std::string &name) const { return hoisted_functions >> name; }
opt_ref<const contract> hoist_tree::lookup_contract(const std::string &name) const { return hoisted_contracts >> name; }
opt_ref<const type> hoist_tree::lookup_type(const std::string &name) const { return hoisted_types >> name; }
opt_ref<const global> hoist_tree::lookup_global(const std::string &name) const { return hoisted_globals >> name; }

hoist_tree::node &hoist_tree::node::operator[](const std::string &name) {
  // TODO: do we need to check for duplicates here?
  return children.emplace(name, node{*tree, *this, mangler::mangle_ns(path_name, name)}).first->second;
}

template <typename T>
std::string do_register(
  const std::string &mangled, const std::string &name, std::unordered_map<std::string, std::string> &map,
  std::unordered_map<std::string, managed<T>> &hoisted
) {
  const auto res = hoisted.emplace(mangled, alloc<T>()).first->first;
  map.emplace(name, mangled);
  return res;
}

void hoist_tree::node::avoid_duplicate(const std::string &name, const location &at) const {
  (children >> name) | [this, &name, &at](const node &) -> int {
    throw semantic_error::redefine_ns(path_name, name, at);
  } || (functions >> name) | [this, &name, &at](const std::string &f) -> int {
    throw semantic_error::redefine_X(
      path_name, "function", name, tree->lookup_function(f).declared_at(), at
    );
  } || (contracts >> name) | [this, &name, &at](const std::string &c) -> int {
    throw semantic_error::redefine_X(
      path_name, "contract", name, tree->lookup_contract(c).declared_at(), at
    );
  } || (types >> name) | [this, &name, &at](const std::string &t) -> int {
    throw semantic_error::redefine_X(
      path_name, "type", name, tree->lookup_type(t).declared_at(), at
    );
  } || (globals >> name) | [this, &name, &at](const std::string &g) -> int {
    throw semantic_error::redefine_X(
      path_name, "global", name, tree->lookup_global(g).declared_at(), at
    );
  };
}

std::string hoist_tree::node::register_function(const std::string &name, const location &at) {
  avoid_duplicate(name, at);
  return do_register(mangler::mangle_function(path_name, name), name, functions, tree->hoisted_functions);
}

std::string hoist_tree::node::register_contract(const std::string &name, const location &at) {
  avoid_duplicate(name, at);
  return do_register(mangler::mangle_contract(path_name, name), name, contracts, tree->hoisted_contracts);
}

std::string hoist_tree::node::register_type(const std::string &name, const location &at) {
  avoid_duplicate(name, at);
  return do_register(mangler::mangle_initial_type(path_name, name), name, types, tree->hoisted_types);
}

std::string hoist_tree::node::register_global(const std::string &name, const location &at) {
  avoid_duplicate(name, at);
  return do_register(mangler::mangle_global(path_name, name), name, globals, tree->hoisted_globals);
}

opt_ref<const hoist_tree::node> hoist_tree::node::operator[](const std::string &ns) const {
  if(const auto it = children.find(ns); it != children.end()) {
    return it->second | ref{} | maybe{};
  }
  return std::nullopt;
}

std::optional<std::string> hoist_tree::node::get_local(const std::string &name) const {
  if(children.contains(name)) return std::nullopt;

  return (functions >> name) || (contracts >> name) || (types >> name) || (globals >> name);
}

std::optional<std::string> hoist_tree::node::lookup(const std::vector<std::string> &name) const {
  if (name.empty())
    return std::nullopt;

  const auto *current = this;

  while(current != nullptr) {
    const node *local = current;
    size_t idx = 0;
    for(; idx < name.size() - 1; ++idx) {
      const auto it = local->children.find(name[idx]);
      if(it == local->children.end()) break; // not found in subtree, move up one level
      local = &it->second; // maybe found, move down one level
    }

    if(idx == name.size() - 1) {
      // final level, should be type/function/contract/global name
      if (auto res = local->get_local(name.back())) return res;
      // not found -> move to parent
    }

    current = current->parent;
  }

  return std::nullopt;
}

std::string hoist_tree::register_nested_type(const std::string &mangled_outer_name, const std::string &name,
                                             const type &t) {
  const auto mangled = mangler::mangle_ns(mangled_outer_name, name);

  if (const auto [it, inserted] = hoisted_types.emplace(mangled, alloc(t)); !inserted) {
    throw semantic_error::redefine_X(mangled_outer_name, "type", name, it->second->declared_at(), t.declared_at());
  }

  return mangled;
}
