//
// Created by jay on 9/14/24.
//

#ifndef HOIST_TREE_HPP
#define HOIST_TREE_HPP

// forward declarations
namespace jayc::sem {
class hoist_tree;
}

#include <string>
#include <unordered_map>
#include "sem_ast.hpp"
#include "parser/ast.hpp"
#include "util/ref_helpers.hpp"
#include "util/managed.hpp"

namespace jayc::sem {
// TODO: figure out generics/templates

class hoist_tree {
public:
  template <typename T> using map_t = std::unordered_map<std::string, jaydk::managed<T>>;

  class node {
  public:
    node &operator[](const std::string &name);
    std::string register_function(const std::string &name, const location &at);
    std::string register_contract(const std::string &name, const location &at);
    std::string register_type(const std::string &name, const location &at);
    std::string register_global(const std::string &name, const location &at);
    jaydk::opt_ref<const node> operator[](const std::string &ns) const;
    std::optional<std::string> get_local(const std::string &name) const;
    std::optional<std::string> lookup(const std::vector<std::string> &name) const;

  private:
    explicit node(hoist_tree &tree) : tree{&tree}, parent{nullptr} {}
    node(hoist_tree &tree, node &parent, std::string path_name) :
      tree{&tree}, parent{&parent}, path_name{std::move(path_name)} {}

    void avoid_duplicate(const std::string &name, const location &at) const;

    hoist_tree *tree;
    node *parent;
    std::string path_name{};
    std::unordered_map<std::string, node> children{};
    std::unordered_map<std::string, std::string> functions{};
    std::unordered_map<std::string, std::string> contracts{};
    std::unordered_map<std::string, std::string> types{};
    std::unordered_map<std::string, std::string> globals{};

    friend hoist_tree;
  };

  constexpr node &root_node() { return root; }
  [[nodiscard]] constexpr const node &root_node() const { return root; }

  function &lookup_function(const std::string &name);
  jaydk::opt_ref<const function> lookup_function(const std::string &name) const;
  contract &lookup_contract(const std::string &name);
  jaydk::opt_ref<const contract> lookup_contract(const std::string &name) const;
  type &lookup_type(const std::string &name);
  jaydk::opt_ref<const type> lookup_type(const std::string &name) const;
  global &lookup_global(const std::string &name);
  jaydk::opt_ref<const global> lookup_global(const std::string &name) const;

  std::string register_nested_type(const std::string &mangled_outer_name, const std::string &name, const type &t);

private:
  node root{*this};
  map_t<function> hoisted_functions;
  map_t<contract> hoisted_contracts;
  map_t<type> hoisted_types;
  map_t<global> hoisted_globals;

  friend node;
};
}

#endif //HOIST_TREE_HPP
