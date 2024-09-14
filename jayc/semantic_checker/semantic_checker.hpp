//
// Created by jay on 9/14/24.
//

#ifndef SEMANTIC_CHECKER_HPP
#define SEMANTIC_CHECKER_HPP

#include "parser/ast.hpp"
#include "sem_ast.hpp"

namespace jayc::sem {
std::optional<sem_ast> check_semantics(const parser::ast &ast);
}

#endif //SEMANTIC_CHECKER_HPP
