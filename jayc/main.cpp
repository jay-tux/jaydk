//
// Created by jay on 9/2/24.
//

#include <cstdint>
#include <iostream>
#include <argparse/argparse.hpp>
#include <termcolor/termcolor.hpp>

#include "args.hpp"
#include "error.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token_output.hpp"

void if_set(std::optional<std::string> &out, const std::string &name, const argparse::ArgumentParser &parser) {
  if(parser.present(name)) out = parser.get<std::string>(name);
}

int main(const int argc, const char **argv) {
  argparse::ArgumentParser parser("jayc");
  jayc::args args{};

  parser.add_argument("input").help("Set the input file for compiling.");
  parser.add_argument("-o", "--output").help("Set the output file.");
  parser.add_argument("--lexer-out").help("Set the output file for the lexer token stream.");
  parser.add_argument("--parser-out").help("Set the output file for the unchecked AST.");
  parser.add_argument("--typecheck-out").help("Set the output file for type-checked AST.");
  parser.add_argument("--codegen-out").help("Set the output file for raw IR.");
  parser.add_argument("--link-out").help("Set the output file for post-link output.");
  parser.add_argument("--bytecode-out").help("Set the output file for human-readable bytecode.");
  parser.add_argument("--no-parse").flag().help("Disable parsing (and later stages).");
  parser.add_argument("--no-typecheck").flag().help("Disable type-checking (and later stages).");
  parser.add_argument("--no-codegen").flag().help("Disable code-generation (and later stages).");
  parser.add_argument("--no-link").flag().help("Disable linking (and later stages).");
  parser.add_argument("--no-output").flag().help("Disable building of bytecode file.");
  parser.add_argument("-l", "--lib").default_value<std::vector<std::string>>({}).append().help("Specify libraries to link against.");
  parser.add_argument("-L", "--lib-path").default_value<std::vector<std::string>>({}).append().help("Specify library search paths.");
  parser.add_description("jayc - JayDK compiler.");

  try {
    parser.parse_args(argc, argv);

    args.input = parser.get<std::string>("input");
    if(parser.present("-o")) args.output = parser.get<std::string>("-o");

    if_set(args.lexer_out, "--lexer-out", parser);
    if_set(args.parser_out, "--parser-out", parser);
    if_set(args.type_checked_ast_out, "--typecheck-out", parser);
    if_set(args.codegen_out, "--codegen-out", parser);
    if_set(args.linker_out, "--link-out", parser);
    if_set(args.bytecode_out, "--bytecode-out", parser);

    args.perform_parser = parser["--no-parse"] != true;
    args.perform_type_check = parser["--no-typecheck"] != true;
    args.perform_codegen = parser["--no-codegen"] != true;
    args.perform_linking = parser["--no-link"] != true;
    args.perform_output = parser["--no-output"] != true;

    args.link_targets = parser.get<std::vector<std::string>>("--lib");
    args.link_sources = parser.get<std::vector<std::string>>("--lib-path");
  }
  catch(const std::exception &e) {
    std::cerr << e.what() << "\n" << parser << "\n";
    return -1;
  }

  try {
    auto strm = jayc::lexer::lex(args.input);
    jayc::lexer::token tok{};
    while(!strm.is_eof()) {
      strm >> tok;
      std::cout << tok << "\n";
    }
    if(jayc::logger.phase_error() > 0) {
      std::cerr << termcolor::red << "Lexer failed.\n";
      return -1;
    }
  }
  catch(jayc::unrecoverable &) {
    return -1;
  }

  return 0;
}