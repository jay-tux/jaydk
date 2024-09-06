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
#include "parser/parser.hpp"
#include "parser/ast_output.hpp"

void if_set(std::optional<std::string> &out, const std::string &name, const argparse::ArgumentParser &parser) {
  if(parser.present(name)) out = parser.get<std::string>(name);
}

int main(const int argc, const char **argv) {
  argparse::ArgumentParser arg_parser("jayc");
  jayc::args args{};

  arg_parser.add_argument("input").help("Set the input file for compiling.");
  arg_parser.add_argument("-o", "--output").help("Set the output file.");
  arg_parser.add_argument("--lexer-out").help("Set the output file for the lexer token stream.");
  arg_parser.add_argument("--parser-out").help("Set the output file for the unchecked AST.");
  arg_parser.add_argument("--typecheck-out").help("Set the output file for type-checked AST.");
  arg_parser.add_argument("--codegen-out").help("Set the output file for raw IR.");
  arg_parser.add_argument("--link-out").help("Set the output file for post-link output.");
  arg_parser.add_argument("--bytecode-out").help("Set the output file for human-readable bytecode.");
  arg_parser.add_argument("--no-parse").flag().help("Disable parsing (and later stages).");
  arg_parser.add_argument("--no-typecheck").flag().help("Disable type-checking (and later stages).");
  arg_parser.add_argument("--no-codegen").flag().help("Disable code-generation (and later stages).");
  arg_parser.add_argument("--no-link").flag().help("Disable linking (and later stages).");
  arg_parser.add_argument("--no-output").flag().help("Disable building of bytecode file.");
  arg_parser.add_argument("-l", "--lib").default_value<std::vector<std::string>>({}).append().help("Specify libraries to link against.");
  arg_parser.add_argument("-L", "--lib-path").default_value<std::vector<std::string>>({}).append().help("Specify library search paths.");
  arg_parser.add_description("jayc - JayDK compiler.");

  try {
    arg_parser.parse_args(argc, argv);

    jayc::logger.enable_throw_on_error();

    args.input = arg_parser.get<std::string>("input");
    if(arg_parser.present("-o")) args.output = arg_parser.get<std::string>("-o");

    if_set(args.lexer_out, "--lexer-out", arg_parser);
    if_set(args.parser_out, "--parser-out", arg_parser);
    if_set(args.type_checked_ast_out, "--typecheck-out", arg_parser);
    if_set(args.codegen_out, "--codegen-out", arg_parser);
    if_set(args.linker_out, "--link-out", arg_parser);
    if_set(args.bytecode_out, "--bytecode-out", arg_parser);

    args.perform_parser = arg_parser["--no-parse"] != true;
    args.perform_type_check = arg_parser["--no-typecheck"] != true;
    args.perform_codegen = arg_parser["--no-codegen"] != true;
    args.perform_linking = arg_parser["--no-link"] != true;
    args.perform_output = arg_parser["--no-output"] != true;

    args.link_targets = arg_parser.get<std::vector<std::string>>("--lib");
    args.link_sources = arg_parser.get<std::vector<std::string>>("--lib-path");
  }
  catch(const std::exception &e) {
    std::cerr << e.what() << "\n" << arg_parser << "\n";
    return -1;
  }

  try {
    auto strm = jayc::lexer::lex(args.input);
    auto parser = jayc::parser::parser(std::move(strm));
    auto ast = parser.parse();
    std::cout << ast;
    if(jayc::logger.phase_error() != 0) {
      return -1;
    }

  }
  catch(jayc::unrecoverable &) {
    return -1;
  }

  return 0;
}