//
// Created by jay on 9/2/24.
//

#ifndef ARGS_HPP
#define ARGS_HPP

#include <string>
#include <optional>
#include <vector>

namespace jayc {
struct args {
  std::string input; //!< Input file
  std::string output = "./out.jbc"; //!< Bytecode output file

  std::optional<std::string> lexer_out = std::nullopt; //!< Output file for the lexer token stream
  bool perform_parser = true; //!< Whether to perform parsing
  std::optional<std::string> parser_out = std::nullopt; //!< Output file for the unchecked AST
  bool perform_type_check = true; //!< Whether to perform type-checking
  std::optional<std::string> type_checked_ast_out = std::nullopt; //!< Output file for type-checked AST
  bool perform_codegen = true; //!< Whether to perform code-generation
  std::optional<std::string> codegen_out = std::nullopt; //!< Output file for raw IR
  bool perform_linking = true; //!< Whether to perform linking
  std::optional<std::string> linker_out = std::nullopt; //!< Output file for post-link output
  bool perform_output = true; //!< Whether to build bytecode file
  std::optional<std::string> bytecode_out = std::nullopt; //!< Output file for human-readable bytecode
  std::vector<std::string> link_targets{}; //!< Library names to link against
  std::vector<std::string> link_sources{}; //!< Directories to search for libraries
};
}

#endif //ARGS_HPP
