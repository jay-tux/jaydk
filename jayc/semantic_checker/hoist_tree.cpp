//
// Created by jay on 9/14/24.
//

#include <unordered_set>

#include "hoist_tree.hpp"
#include "sem_ast.hpp"
#include "mangler.hpp"
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
  //
  return children.emplace(name, node{*tree, *this, mangler::mangle_ns(path_name, name)}).first->second;
}

template <typename T>
std::string do_register(
  const std::string &path_name, const std::string &name, std::unordered_map<std::string, std::string> &map,
  std::unordered_map<std::string, managed<T>> &hoisted
) {
  const auto mangled = hoisted.emplace(mangler::mangle_ns(path_name, name), alloc<T>()).first->first;
  map.emplace(name, mangled);
  return mangled;
}

void hoist_tree::node::avoid_duplicate(const std::string &name, const location &at) const {
  //
}


std::string hoist_tree::node::register_function(const std::string &name, const location &at) {
  avoid_duplicate(name, at);
  return do_register(path_name, name, functions, tree->hoisted_functions);
}

std::string hoist_tree::node::register_contract(const std::string &name, const location &at) {
  avoid_duplicate(name, at);
  return do_register(path_name, name, contracts, tree->hoisted_contracts);
}

std::string hoist_tree::node::register_type(const std::string &name, const location &at) {
  avoid_duplicate(name, at);
  return do_register(path_name, name, types, tree->hoisted_types);
}

std::string hoist_tree::node::register_global(const std::string &name, const location &at) {
  avoid_duplicate(name, at);
  return do_register(path_name, name, globals, tree->hoisted_globals);
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
  if(name.empty()) return std::nullopt;

  const node *current = this;

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
  //
}
